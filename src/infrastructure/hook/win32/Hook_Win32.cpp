#include "Hook_Win32.hpp"

#include <winsock2.h>

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

}  // namespace arkan::relay::infrastructure::hook
