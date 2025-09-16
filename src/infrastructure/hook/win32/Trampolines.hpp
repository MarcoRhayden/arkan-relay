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
struct IHook;  // forward declaration (avoids cross-include)
}

namespace arkan::relay::infrastructure::win32
{

// Shared state between trampolines/hook
struct TrampState
{
  // __cdecl; checksum receives seed64 (high<<32 | low)
  using seed64_fp = unsigned long long(__cdecl*)(const BYTE*, UINT);
  using csum_fp = BYTE(__cdecl*)(const BYTE*, UINT, UINT, unsigned long long);

  seed64_fp seed_fp = nullptr;
  csum_fp checksum_fp = nullptr;

  int(WSAAPI* original_send)(SOCKET, const char*, int, int) = nullptr;
  int(WSAAPI* original_recv)(SOCKET, char*, int, int) = nullptr;

  std::atomic<uint32_t> high{0}, low{0};
  std::atomic<int> counter{0};
  std::atomic<bool> found1c0b{false};
  std::mutex send_mtx;

  arkan::relay::application::ports::IHook* owner = nullptr;
};

// Trampolines (implemented in Trampolines.cpp)
struct Trampolines
{
  static inline TrampState* S = nullptr;

  static inline void init(TrampState* s)
  {
    S = s;
  }

  static BYTE call_seed(const BYTE* data, size_t len);
  static BYTE call_checksum(const BYTE* data, size_t len, uint32_t counter, uint32_t low,
                            uint32_t high);

  static int WSAAPI send(SOCKET s, const char* buf, int len, int flags);
  static int WSAAPI recv(SOCKET s, char* buf, int len, int flags);
};

}  // namespace arkan::relay::infrastructure::win32
