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
#include <chrono>
#include <cstdint>
#include <mutex>

namespace arkan::relay::application::ports
{
struct IHook;
}

namespace arkan::relay::infrastructure::win32
{

// -----------------------------------------------------------------------------
// Shared state between trampolines/hook
// -----------------------------------------------------------------------------
struct TrampState
{
  using seed64_fp = unsigned long long(__cdecl*)(BYTE*, UINT);
  using csum_fp = BYTE(__cdecl*)(BYTE*, UINT, UINT, unsigned long long);

  seed64_fp seed_fp = nullptr;
  csum_fp checksum_fp = nullptr;

  // When true => the trampolines should NOT emit an 'S' frame to the Bridge for this send.
  // Use atomic to synchronize across threads.
  std::atomic<bool> suppress_emit_send{false};

  // original (unhooked) function pointers captured at patch time
  int(WSAAPI* original_send)(SOCKET, const char*, int, int) = nullptr;
  int(WSAAPI* original_recv)(SOCKET, char*, int, int) = nullptr;

  // Checksum / seed state (atomic to be safe when accessed from multiple threads)
  std::atomic<uint32_t> high{0}, low{0};
  std::atomic<int> counter{0};
  std::atomic<bool> found1c0b{false};

  // Mutex that serializes send transformation / seed/checksum calls
  std::mutex send_mtx;

  // Last observed socket used for detecting new sessions / resetting state
  std::atomic<SOCKET> last_socket{INVALID_SOCKET};

  inline void reset_all_relaxed() noexcept
  {
    counter.store(0, std::memory_order_relaxed);
    found1c0b.store(false, std::memory_order_relaxed);
    low.store(0, std::memory_order_relaxed);
    high.store(0, std::memory_order_relaxed);
  }

  // Bridge backref
  arkan::relay::application::ports::IHook* owner = nullptr;
};

// -----------------------------------------------------------------------------
// Trampolines API
// -----------------------------------------------------------------------------
struct Trampolines
{
  // Initialize trampolines with shared state. `s` must remain valid for the
  // lifetime of the trampoline usage (or until deinit is called).
  static void init(TrampState* s);

  // Query helpers
  // Returns true if the Trampolines subsystem has been initialized (init called).
  static bool is_installed();

  // Returns the raw TrampState pointer (or nullptr) in a thread-safe manner.
  static TrampState* get_state();

  // Hooked send/recv (same signatures as Winsock)
  static int WSAAPI send(SOCKET s, const char* buf, int len, int flags);
  static int WSAAPI recv(SOCKET s, char* buf, int len, int flags);
};

}  // namespace arkan::relay::infrastructure::win32
