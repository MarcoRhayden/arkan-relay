#include "Hook_Win32.hpp"

#include <winsock2.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <utility>

#include "win32/AddressResolver.hpp"
#include "win32/SlotPatcher.hpp"
#include "win32/SlotWatchdog.hpp"
#include "win32/Trampolines.hpp"

namespace arkan::relay::infrastructure::hook
{

using arkan::relay::application::ports::LogLevel;
using namespace arkan::relay::infrastructure::win32;

struct Hook_Win32::Impl
{
  ResolvedAddrs addrs{};
  TrampState tramp{};
  SlotWatchdog watchdog{};
};

Hook_Win32::Hook_Win32(ports::ILogger& log, const domain::Settings& s)
    : p_(std::make_unique<Impl>()), log_(log), cfg_(s)
{
  p_->addrs =
      AddressResolver::resolve(cfg_.fnSendAddr.value_or(""), cfg_.fnRecvAddr.value_or(""),
                               cfg_.fnSeedAddr.value_or(""), cfg_.fnChecksumAddr.value_or(""));
  AddressResolver::log_pages(log_, p_->addrs);
}

Hook_Win32::~Hook_Win32()
{
  uninstall();
}

bool Hook_Win32::install()
{
  const auto& a = p_->addrs;
  if (!a.send_slot || !a.recv_slot || !a.seed_fn || !a.checksum_fn)
  {
    log_.app(LogLevel::err, "Hook_Win32.install(): missing addresses (send/recv/seed/checksum).");
    return false;
  }

  // Wire helpers + backref for callbacks
  p_->tramp.seed_fp = reinterpret_cast<TrampState::seed64_fp>(a.seed_fn);
  p_->tramp.checksum_fp = reinterpret_cast<TrampState::csum_fp>(a.checksum_fn);
  if (!p_->tramp.seed_fp || !p_->tramp.checksum_fp)
  {
    log_.app(LogLevel::err, "Hook_Win32.install(): invalid seed/checksum function pointers.");
    return false;
  }

  // Backref so trampolines can call owner->notify_socket(s) if desired
  p_->tramp.owner = this;
  Trampolines::init(&p_->tramp);

  using SendFP = int(WSAAPI*)(SOCKET, const char*, int, int);
  using RecvFP = int(WSAAPI*)(SOCKET, char*, int, int);

  // Patch SEND slot (wait until non-null, capture original)
  if (!SlotPatcher::wait_and_patch<SendFP>(
          a.send_slot, reinterpret_cast<SendFP>(&Trampolines::send), p_->tramp.original_send, log_))
  {
    log_.app(LogLevel::err, "Hook_Win32.install(): failed to patch SEND slot (wait timeout).");
    return false;
  }

  // Patch RECV slot (rollback SEND if we fail here)
  if (!SlotPatcher::wait_and_patch<RecvFP>(
          a.recv_slot, reinterpret_cast<RecvFP>(&Trampolines::recv), p_->tramp.original_recv, log_))
  {
    if (p_->tramp.original_send) SlotPatcher::force<SendFP>(a.send_slot, p_->tramp.original_send);
    log_.app(LogLevel::err, "Hook_Win32.install(): failed to patch RECV slot (wait timeout).");
    return false;
  }

  log_.app(LogLevel::info, "SEND/RECV slots patched successfully.");

  // Keep slots ours even if overwritten later
  p_->watchdog.start<SendFP, RecvFP>(a.send_slot, reinterpret_cast<SendFP>(&Trampolines::send),
                                     a.recv_slot, reinterpret_cast<RecvFP>(&Trampolines::recv));
  return true;
}

void Hook_Win32::uninstall()
{
  if (!p_) return;  // idempotent

  p_->watchdog.stop();

  // Restore original function pointers (best effort)
  if (p_->addrs.send_slot && p_->tramp.original_send)
    SlotPatcher::force<decltype(p_->tramp.original_send)>(p_->addrs.send_slot,
                                                          p_->tramp.original_send);
  if (p_->addrs.recv_slot && p_->tramp.original_recv)
    SlotPatcher::force<decltype(p_->tramp.original_recv)>(p_->addrs.recv_slot,
                                                          p_->tramp.original_recv);
}

/* ------------------------ public wrappers ------------------------ */

bool Hook_Win32::try_inject_send(Bytes b)
{
  return try_inject_send_internal(b, true);
}

bool Hook_Win32::try_inject_send(Bytes b, bool needs_checksum)
{
  return try_inject_send_internal(b, needs_checksum);
}

/* ------------------------ internal implementation ------------------------ */
bool Hook_Win32::try_inject_send_internal(Bytes b, bool needs_checksum)
{
  // capture last valid socket
  const SOCKET s = p_->tramp.last_socket.load(std::memory_order_acquire);
  if (s == INVALID_SOCKET) return false;

  // enqueue bytes with flag
  {
    std::lock_guard<std::mutex> lk(inj_send_mtx_);
    InjectMsg m;
    m.data.assign(reinterpret_cast<const uint8_t*>(b.data()),
                  reinterpret_cast<const uint8_t*>(b.data()) + b.size());
    m.needs_checksum = needs_checksum;
    m.attempts = 0;
    m.next_try = std::chrono::steady_clock::time_point::min();  // ready immediately
    inj_send_q_.emplace_back(std::move(m));
  }

  // lightweight drain now
  drain_injected_sends_();
  return true;
}

bool Hook_Win32::try_inject_recv(Bytes b)
{
  // inbound emulation not implemented yet
  (void)b;
  // (intentionally quiet)
  return true;
}

void Hook_Win32::emit_send(Bytes b)
{
  if (on_send) on_send(b);
}
void Hook_Win32::emit_recv(Bytes b)
{
  if (on_recv) on_recv(b);
}

void Hook_Win32::notify_socket(SOCKET s)
{
  p_->tramp.last_socket.store(s, std::memory_order_release);

  // Attempt drain in case messages were waiting for a valid socket
  // (safe to call from notify context)
  drain_injected_sends_();
}

// -----------------------------------------------------------------------------
// Helper: requeue_with_backoff
// Moves 'item' back into inj_send_q_ with retry/backoff bookkeeping.
// -----------------------------------------------------------------------------
void Hook_Win32::requeue_with_backoff(InjectMsg item, const char* reason)
{
  // increment attempts
  item.attempts = static_cast<uint8_t>(item.attempts + 1);

  if (item.attempts > MAX_INJECT_ATTEMPTS)
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "[INJECT][SEND] dropping after max retries (%s) attempts=%u",
                  reason, static_cast<unsigned>(item.attempts));
    log_.sock(LogLevel::warn, buf);
    return;
  }

  // compute backoff delay as chrono duration
  const auto delay = BACKOFF_BASE_MS * static_cast<unsigned>(item.attempts);
  item.next_try = std::chrono::steady_clock::now() + delay;

  {
    std::lock_guard<std::mutex> lk(inj_send_mtx_);
    inj_send_q_.push_front(std::move(item));
  }

  // log debug with ms
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(delay).count();
  char lb[256];
  std::snprintf(lb, sizeof(lb), "[INJECT][SEND] requeued attempts=%u next_in=%lldms (%s)",
                static_cast<unsigned>(item.attempts), static_cast<long long>(ms), reason);
  log_.sock(LogLevel::debug, lb);
}

/* ------------------------ drain / send logic ------------------------ */
void Hook_Win32::drain_injected_sends_()
{
  const SOCKET s = p_->tramp.last_socket.load(std::memory_order_acquire);
  if (s == INVALID_SOCKET) return;

  // collect up to kDrainBatchMax messages whose next_try <= now
  std::vector<InjectMsg> batch;
  batch.reserve(kDrainBatchMax);
  {
    std::lock_guard<std::mutex> lk(inj_send_mtx_);
    if (inj_send_q_.empty()) return;

    const auto now = std::chrono::steady_clock::now();
    // process up to kDrainBatchMax items that are due
    for (size_t i = 0; i < kDrainBatchMax && !inj_send_q_.empty(); ++i)
    {
      InjectMsg& front = inj_send_q_.front();
      if (front.next_try == std::chrono::steady_clock::time_point::min() || front.next_try <= now)
      {
        batch.emplace_back(std::move(front));
        inj_send_q_.pop_front();
      }
      else
      {
        // front not due yet -> stop taking more (FIFO order preserved)
        break;
      }
    }
    if (batch.empty()) return;  // nothing ready to send now
  }

  for (auto& im : batch)
  {
    // If hook not installed, requeue and skip sending to avoid sending raw invalid packets.
    bool hook_installed = (p_->tramp.original_send != nullptr);
    if (!hook_installed)
    {
      // requeue at front (safe) so next time we can try again
      {
        std::lock_guard<std::mutex> lk(inj_send_mtx_);
        // ensure next_try is immediate so it will be considered when hook installed
        if (im.next_try == std::chrono::steady_clock::time_point::min())
          im.next_try = std::chrono::steady_clock::time_point::min();
        inj_send_q_.push_front(std::move(im));
      }
      log_.sock(LogLevel::warn, "[INJECT][SEND] hook not installed yet -> requeued message");
      return;
    }

    // Build final buffer to send: (data [+ optional checksum byte])
    std::vector<uint8_t> to_send;
    to_send.reserve(im.data.size() + (im.needs_checksum ? 1U : 0U));
    to_send.insert(to_send.end(), im.data.begin(), im.data.end());
    if (im.needs_checksum)
    {
      to_send.push_back(DEFAULT_CHECKSUM_BYTE);
    }

    // RAII guard to ensure we always reset suppress_emit_send even on early returns/exceptions
    struct SuppressGuard
    {
      std::atomic<bool>& flag;
      SuppressGuard(std::atomic<bool>& f) : flag(f)
      {
        flag.store(true, std::memory_order_release);
      }
      ~SuppressGuard()
      {
        flag.store(false, std::memory_order_release);
      }
    } guard(p_->tramp.suppress_emit_send);

    // Log short summary & limited hex dump (cheap)
    {
      char hexb[3 * HEX_DUMP_LIMIT + 64];
      size_t p = 0;
      const size_t take = std::min(HEX_DUMP_LIMIT, to_send.size());
      for (size_t i = 0; i < take && p + 8 < sizeof(hexb); ++i)
      {
        p += std::snprintf(hexb + p, sizeof(hexb) - p, "%02X ", to_send[i]);
      }
      if (take < to_send.size())
        p += std::snprintf(hexb + p, sizeof(hexb) - p, "...(%zu)", to_send.size());

      char buf[512];
      // convert SOCKET to an integer-sized type safely, then to long long for %lld
      const auto socket_val = static_cast<intptr_t>(s);
      std::snprintf(
          buf, sizeof(buf), "[INJECT][SEND] socket=%lld len=%zu needs_checksum=%d data=%s",
          static_cast<long long>(socket_val), to_send.size(), (int)im.needs_checksum, hexb);
      log_.sock(LogLevel::debug, buf);
    }

    // call Trampolines::send ONCE for the whole logical message.
    int r = arkan::relay::infrastructure::win32::Trampolines::send(
        s, reinterpret_cast<const char*>(to_send.data()), static_cast<int>(to_send.size()), 0);

    if (r == SOCKET_ERROR)
    {
      const int e = WSAGetLastError();
      char eb[256];
      std::snprintf(eb, sizeof(eb), "[INJECT][SEND] SOCKET_ERROR wsa=%d wrote=%d/%zu", e, r,
                    to_send.size());
      log_.sock(LogLevel::warn, eb);

      if (e == WSAECONNRESET || e == WSAENOTCONN || e == WSAECONNABORTED || e == WSAESHUTDOWN)
      {
        p_->tramp.last_socket.store(INVALID_SOCKET, std::memory_order_release);
      }

      // Requeue the whole original message (with same needs_checksum flag) for a retry later
      {
        InjectMsg re;
        re.data = std::move(im.data);
        re.needs_checksum = im.needs_checksum;

        // delegate to helper which handles attempts/backoff/drop
        re.attempts = im.attempts;  // start from original attempts
        requeue_with_backoff(std::move(re), "socket_error");
      }
      log_.sock(LogLevel::warn, "[INJECT][SEND] send failed -> message requeued");
    }
    else if (static_cast<size_t>(r) < to_send.size())
    {
      // partial write: requeue the original logical message (avoid partial transformations)
      char wb[128];
      std::snprintf(wb, sizeof(wb), "[INJECT][SEND] partial write=%d/%zu -> requeued", r,
                    to_send.size());
      log_.sock(LogLevel::warn, wb);

      InjectMsg re;
      re.data = std::move(im.data);
      re.needs_checksum = im.needs_checksum;
      re.attempts = im.attempts;
      requeue_with_backoff(std::move(re), "partial_write");
    }
    else
    {
      // success: all bytes sent
      char wb[128];
      std::snprintf(wb, sizeof(wb), "[INJECT][SEND] wrote=%d total=%zu/%zu", r,
                    static_cast<size_t>(r), to_send.size());
      log_.sock(LogLevel::info, wb);
    }
  }
}

}  // namespace arkan::relay::infrastructure::hook
