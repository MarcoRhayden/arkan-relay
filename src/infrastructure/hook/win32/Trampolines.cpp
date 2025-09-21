#include "Trampolines.hpp"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "application/ports/IHook.hpp"
#include "application/services/protocol/ChecksumService_Callback.hpp"
#include "application/services/protocol/ChecksumState.hpp"
#include "application/services/protocol/ProtocolScanner.hpp"
#include "application/services/protocol/ProtocolScanner_Coalesced.hpp"
#include "domain/protocol/Opcodes.hpp"
#include "infrastructure/net/RecvPipeline.hpp"
#include "infrastructure/net/SendPipeline.hpp"
#include "shared/hex/Hex.hpp"

#pragma comment(lib, "Ws2_32.lib")

namespace arkan::relay::infrastructure::win32
{

namespace op = arkan::relay::domain::protocol::op;
namespace app = arkan::relay::application::services;
using arkan::relay::infrastructure::net::RecvPipeline;
using arkan::relay::infrastructure::net::SendPipeline;

// -----------------------------------------------------------------------------------------------
// Minimal debug helpers
// -----------------------------------------------------------------------------------------------
static inline void dbg(const char* m)
{
  ::OutputDebugStringA(m);
}

static inline void dbg_str(const std::string& s)
{
  ::OutputDebugStringA(s.c_str());
}

// -----------------------------------------------------------------------------------------------
// File-local atomic pointer to the shared TrampState.
// - init() stores the pointer (memory_order_release) after the subsystem is ready.
// - get_state() / is_installed() load with memory_order_acquire.
// This guarantees a consistent view to other threads.
// -----------------------------------------------------------------------------------------------
static std::atomic<TrampState*> g_tramp_state{nullptr};

void Trampolines::init(TrampState* s)
{
  g_tramp_state.store(s, std::memory_order_release);
}

TrampState* Trampolines::get_state()
{
  return g_tramp_state.load(std::memory_order_acquire);
}

bool Trampolines::is_installed()
{
  return get_state() != nullptr;
}

// -----------------------------------------------------------------------------------------------
// SEH wrappers (local) para os callbacks do serviço
// -----------------------------------------------------------------------------------------------
static unsigned long long __stdcall seh_call_seed(TrampState::seed64_fp fp, uint8_t* data,
                                                  unsigned len, BOOL* ok)
{
  __try
  {
    unsigned long long r = fp(data, len);
    if (ok) *ok = TRUE;
    return r;
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    if (ok) *ok = FALSE;
    return 0ULL;
  }
}

static uint8_t __stdcall seh_call_checksum(TrampState::csum_fp fp, uint8_t* data, unsigned len,
                                           unsigned counter, unsigned long long seed64, BOOL* ok)
{
  __try
  {
    uint8_t r = fp(data, len, counter, seed64);
    if (ok) *ok = TRUE;
    return r;
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    if (ok) *ok = FALSE;
    return 0;
  }
}

// -----------------------------------------------------------------------------------------------
// Helper: log_hex_buf
// -----------------------------------------------------------------------------------------------
static inline void log_hex_buf(const char* tag, const uint8_t* p, size_t n, size_t max = 32)
{
  std::span<const std::byte> sp{reinterpret_cast<const std::byte*>(p), n};
  dbg_str(arkan::relay::shared::hex::make_line(tag, sp, max));
}

// -----------------------------------------------------------------------------------------------
// recv()
// -----------------------------------------------------------------------------------------------
int WSAAPI Trampolines::recv(SOCKET s, char* buf, int len, int flags)
{
  TrampState* S = get_state();
  if (S && S->owner) S->owner->notify_socket(s);

  if (!S || !S->original_recv) return SOCKET_ERROR;

  // if the socket has changed, it is a new session - clears state
  SOCKET prev = S->last_socket.load(std::memory_order_relaxed);
  if (prev != s)
  {
    S->last_socket.store(s, std::memory_order_relaxed);
    S->reset_all_relaxed();
    dbg("[RECV] new socket detected -> reset state\n");
  }

  app::ChecksumState state{S->counter, S->found1c0b, S->low, S->high};

  // Scanner + pipeline
  RecvPipeline rpipe{app::DefaultProtocolScanner()};

  // Wrapper
  auto do_recv = [&](char* obuf, int olen) -> int
  {
    int r = S->original_recv(s, obuf, olen, flags);
    if (r == SOCKET_ERROR)
    {
      int e = WSAGetLastError();
      char b[96];
      std::snprintf(b, sizeof(b), "[RECV][ERR] SOCKET_ERROR wsa=%d\n", e);
      dbg(b);

      if (e == WSAECONNRESET || e == WSAENOTCONN || e == WSAECONNABORTED || e == WSAESHUTDOWN)
      {
        S->reset_all_relaxed();
        S->last_socket.store(INVALID_SOCKET, std::memory_order_relaxed);
        dbg("[RECV] error -> reset state\n");
      }
    }
    else if (r == 0)
    {
      S->reset_all_relaxed();
      S->last_socket.store(INVALID_SOCKET, std::memory_order_relaxed);
      dbg("[RECV] connection closed -> reset state\n");
    }
    return r;
  };

  int ret = do_recv(buf, len);
  if (ret <= 0) return ret;

  if (S->owner && ret > 0)
  {
    std::span<const std::byte> v{reinterpret_cast<const std::byte*>(buf), (size_t)ret};
    S->owner->emit_recv(v);
  }

  uint8_t* data = reinterpret_cast<uint8_t*>(buf);
  size_t n = static_cast<size_t>(ret);
  log_hex_buf("[RECV] raw       ", data, n);

  bool drop = false;
  rpipe.process(std::span<const uint8_t>(data, n), state, drop);

  int drop_guard = 0;
  while (drop)
  {
    dbg("[RECV] C7 0B -> drop & read next\n");
    state.counter.store(0, std::memory_order_relaxed);
    state.found1c0b.store(false, std::memory_order_relaxed);

    if (++drop_guard > 8) break;

    ret = do_recv(buf, len);
    if (ret <= 0) return ret;

    data = reinterpret_cast<uint8_t*>(buf);
    n = static_cast<size_t>(ret);
    log_hex_buf("[RECV] after-drop", data, n);

    drop = false;
    rpipe.process(std::span<const uint8_t>(data, n), state, drop);
  }

  return ret;
}

// -----------------------------------------------------------------------------------------------
// send()
// -----------------------------------------------------------------------------------------------
int WSAAPI Trampolines::send(SOCKET s, const char* buf, int len, int flags)
{
  TrampState* S = get_state();
  if (S && S->owner) S->owner->notify_socket(s);
  if (!S || !S->original_send) return SOCKET_ERROR;
  if (len <= 0 || !buf) return S->original_send(s, buf, len, flags);

  // if the socket has changed, it is a new session - clears state
  SOCKET prev = S->last_socket.load(std::memory_order_relaxed);
  if (prev != s)
  {
    S->last_socket.store(s, std::memory_order_relaxed);
    S->reset_all_relaxed();
    dbg("[SEND] new socket detected -> reset state\n");
  }

  std::lock_guard<std::mutex> lk(S->send_mtx);

  app::ChecksumState state{S->counter, S->found1c0b, S->low, S->high};

  app::ChecksumService_Callback::Seed64Fn seed_cb = [&](uint8_t* data,
                                                        unsigned n) -> unsigned long long
  {
    if (!S->seed_fp) return 0ULL;
    BOOL ok = FALSE;
    return seh_call_seed(S->seed_fp, data, n, &ok);
  };

  app::ChecksumService_Callback::ChecksumFn csum_cb =
      [&](uint8_t* data, unsigned n, unsigned counter, unsigned long long seed64) -> uint8_t
  {
    if (!S->checksum_fp) return 0;
    BOOL ok = FALSE;
    return seh_call_checksum(S->checksum_fp, data, n, counter, seed64, &ok);
  };

  app::ChecksumService_Callback svc{std::move(seed_cb), std::move(csum_cb)};
  SendPipeline spipe{svc};

  std::vector<uint8_t> data(reinterpret_cast<const uint8_t*>(buf),
                            reinterpret_cast<const uint8_t*>(buf) + len);

  log_hex_buf("[SEND] in        ", data.data(), data.size());

  spipe.transform(data, state);

  log_hex_buf("[SEND] out       ", data.data(), data.size());

  // emit to Bridge AFTER transform (wire-level)
  if (!(S->suppress_emit_send.exchange(false, std::memory_order_acq_rel)))
  {
    if (S->owner)
    {
      std::span<const std::byte> v{reinterpret_cast<const std::byte*>(data.data()), data.size()};
      S->owner->emit_send(v);
    }
  }

  // send to real socket (transformed)
  const int result = S->original_send(s, reinterpret_cast<const char*>(data.data()),
                                      static_cast<int>(data.size()), flags);

  if (result == SOCKET_ERROR)
  {
    int e = WSAGetLastError();
    char b[96];
    std::snprintf(b, sizeof(b), "[SEND][ERR] SOCKET_ERROR wsa=%d\n", e);
    dbg(b);

    if (e == WSAECONNRESET || e == WSAENOTCONN || e == WSAECONNABORTED || e == WSAESHUTDOWN)
    {
      S->reset_all_relaxed();
      S->last_socket.store(INVALID_SOCKET, std::memory_order_relaxed);
      dbg("[SEND] error -> reset state\n");
    }
  }
  else
  {
    char b[96];
    std::snprintf(b, sizeof(b), "[SEND] sent=%d\n", result);
    dbg(b);
  }
  return result;
}

}  // namespace arkan::relay::infrastructure::win32
