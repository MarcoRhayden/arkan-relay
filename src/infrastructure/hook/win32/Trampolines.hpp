#pragma once

// clang-format off
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <windows.h>
// clang-format on

#include <atomic>
#include <cstdint>
#include <mutex>

namespace arkan::relay::application::ports
{
struct IHook;
}

namespace arkan::relay::infrastructure::win32
{

// Shared state between trampolines/hook
struct TrampState
{
  using seed64_fp = unsigned long long(__cdecl*)(BYTE*, UINT);
  using csum_fp = BYTE(__cdecl*)(BYTE*, UINT, UINT, unsigned long long);

  seed64_fp seed_fp = nullptr;
  csum_fp checksum_fp = nullptr;

  int(WSAAPI* original_send)(SOCKET, const char*, int, int) = nullptr;
  int(WSAAPI* original_recv)(SOCKET, char*, int, int) = nullptr;

  std::atomic<uint32_t> high{0}, low{0};
  std::atomic<int> counter{0};
  std::atomic<bool> found1c0b{false};

  std::mutex send_mtx;

  std::atomic<SOCKET> last_socket{INVALID_SOCKET};

  inline void reset_all_relaxed() noexcept
  {
    counter.store(0, std::memory_order_relaxed);
    found1c0b.store(false, std::memory_order_relaxed);
    low.store(0, std::memory_order_relaxed);
    high.store(0, std::memory_order_relaxed);
  }

  arkan::relay::application::ports::IHook* owner = nullptr;
};

// Trampolines
struct Trampolines
{
  static inline TrampState* S = nullptr;

  static inline void init(TrampState* s)
  {
    S = s;
  }

  static int WSAAPI send(SOCKET s, const char* buf, int len, int flags);
  static int WSAAPI recv(SOCKET s, char* buf, int len, int flags);
};

}  // namespace arkan::relay::infrastructure::win32
