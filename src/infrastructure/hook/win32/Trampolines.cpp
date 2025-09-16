#include "Trampolines.hpp"

#include <span>
#include <string>
#include <vector>

#include "domain/protocol/Opcodes.hpp"
#include "shared/hex/Hex.hpp"

#pragma comment(lib, "Ws2_32.lib")

/*
  Trampolines.cpp
  -----------------------------------------------------------------------------------------------
  This file contains the trampoline layer used by the Win32 hook. It provides:
    - SEH-wrapped leaf calls to the target seed/checksum functions (no destructors, no STL).
    - Hex/log helpers (delegating to shared hex utilities).
    - Hooked send/recv entry points that implement the domain protocol rules.
*/

namespace arkan::relay::infrastructure::win32
{

// Short namespace alias to the canonical opcode set (domain layer).
namespace op = arkan::relay::domain::protocol::op;

// Bring shared hex dump helper into scope for concise logging.
using arkan::relay::shared::hex::hex_dump;

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
// SEH "leaf" wrappers
// - These wrappers are intentionally minimal (no STL objects with destructors) so that
//   exceptions thrown by the target functions are caught and do not propagate across
//   boundary layers. This mirrors the "leaf" style used in low-level hooking.
// -----------------------------------------------------------------------------------------------

static unsigned long long __stdcall seh_call_seed(TrampState::seed64_fp fp, BYTE* data, UINT len,
                                                  BOOL* ok)
{
  __try
  {
    // Target is __cdecl; we call inside __try/__except to safely trap faults.
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

static BYTE __stdcall seh_call_checksum(TrampState::csum_fp fp, BYTE* data, UINT len, UINT counter,
                                        unsigned long long seed64, BOOL* ok)
{
  __try
  {
    // Target is __cdecl; we call inside __try/__except to safely trap faults.
    BYTE r = fp(data, len, counter, seed64);
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
// Hex logging helpers
// - log_hex_buf(): builds a compact, truncated hex dump line using shared hex utilities.
// - starts_with2(): small span-based predicate for the first two bytes (opcode match).
// -----------------------------------------------------------------------------------------------

static inline void log_hex_buf(const char* tag, const BYTE* p, size_t n, size_t max = 32)
{
  std::span<const std::byte> sp{reinterpret_cast<const std::byte*>(p), n};
  std::string line;
  line.reserve(64 + max * 3);
  line.append(tag).append(" n=").append(std::to_string(n)).append(": ");
  line += hex_dump(sp, max);
  dbg_str(line);
}

static inline bool starts_with2(std::span<const BYTE> p, BYTE a, BYTE b)
{
  return p.size() >= 2 && p[0] == a && p[1] == b;
}

// -----------------------------------------------------------------------------------------------
// One-time PRNG seeding
// -----------------------------------------------------------------------------------------------

static void seed_prng_once()
{
  static std::once_flag once;
  std::call_once(once,
                 []
                 {
                   LARGE_INTEGER t;
                   ::QueryPerformanceCounter(&t);
                   unsigned int s =
                       (unsigned int)(t.QuadPart ^ GetTickCount() ^ GetCurrentProcessId());
                   std::srand(s);
                   char b[96];
                   std::snprintf(b, sizeof(b), "[TRACE] srand init s=0x%08X\n", s);
                   dbg(b);
                 });
}

// -----------------------------------------------------------------------------------------------
// Seed / Checksum bridges
// - call_seed(): appends a signed random byte to the payload, calls seed64, stores high/low.
// - call_checksum(): computes checksum using stored seed64 parts and the rolling counter.
// -----------------------------------------------------------------------------------------------

BYTE Trampolines::call_seed(const BYTE* data, size_t len)
{
  if (!S || !S->seed_fp) return 0;

  seed_prng_once();

  // Random extra byte in the range [-128..127], stored as BYTE.
  int r = std::rand() % 256;
  int rb = r - 128;
  BYTE extra = static_cast<BYTE>(rb & 0xFF);

  // Buffer = payload + 1 random byte.
  std::vector<BYTE> tmp;
  tmp.reserve(len + 1);
  tmp.insert(tmp.end(), data, data + len);
  tmp.push_back(extra);

  // Call seed64 via SEH wrapper (fault-safe).
  BOOL ok = FALSE;
  unsigned long long seed64 =
      seh_call_seed(S->seed_fp, tmp.data(), static_cast<UINT>(tmp.size()), &ok);
  if (!ok) dbg("[SEED][EXC] exception inside seed_fp\n");

  // Split and publish seed64 parts for later checksum usage.
  const uint32_t high = static_cast<uint32_t>(seed64 >> 32);
  const uint32_t low = static_cast<uint32_t>(seed64 & 0xFFFFFFFFu);

  S->high.store(high, std::memory_order_relaxed);
  S->low.store(low, std::memory_order_relaxed);

  char buf[160];
  std::snprintf(buf, sizeof(buf), "[SEED] byte=%d seed=0x%llx high=%u low=%u\n", rb, seed64, high,
                low);
  dbg(buf);

  // Protocol returns the random byte (not seed64's return).
  return extra;
}

BYTE Trampolines::call_checksum(const BYTE* data, size_t len, uint32_t counter, uint32_t low,
                                uint32_t high)
{
  if (!S || !S->checksum_fp) return 0;

  // Recompose seed64 from stored hi/lo parts.
  const unsigned long long seed64 =
      (static_cast<unsigned long long>(high) << 32) | static_cast<unsigned long long>(low);

  // Call checksum via SEH wrapper (fault-safe).
  BOOL ok = FALSE;
  BYTE result =
      seh_call_checksum(S->checksum_fp, const_cast<BYTE*>(reinterpret_cast<const BYTE*>(data)),
                        static_cast<UINT>(len), static_cast<UINT>(counter), seed64, &ok);

  if (!ok) dbg("[CHECKSUM][EXC] exception inside checksum_fp\n");

  char buf[128];
  std::snprintf(buf, sizeof(buf), "[CHECKSUM] counter=%u low=%u high=%u -> %u\n", counter, low,
                high, result);
  dbg(buf);

  return result;
}

// -----------------------------------------------------------------------------------------------
// Hooked recv()
// - Reads incoming data, logs a compact hex dump, and updates protocol state:
//   * C7 0A: reset counter.
//   * B3 00: reset counter and clear 1C 0B flag.
//   * C7 0B: drop-and-read-next loop (bounded by drop_guard).
// -----------------------------------------------------------------------------------------------

int WSAAPI Trampolines::recv(SOCKET s, char* buf, int len, int flags)
{
  if (!S || !S->original_recv) return SOCKET_ERROR;

  // Minimal wrapper to call the original recv and log basic errors.
  auto do_recv = [&](char* obuf, int olen) -> int
  {
    int r = S->original_recv(s, obuf, olen, flags);
    if (r == SOCKET_ERROR)
    {
      int e = WSAGetLastError();
      char b[96];
      std::snprintf(b, sizeof(b), "[RECV][ERR] SOCKET_ERROR wsa=%d\n", e);
      dbg(b);
    }
    return r;
  };

  int ret = do_recv(buf, len);
  if (ret <= 0) return ret;

  BYTE* data = reinterpret_cast<BYTE*>(buf);
  size_t n = static_cast<size_t>(ret);

  log_hex_buf("[RECV] raw       ", data, n);

  // C7 0A -> counter = 0
  if (starts_with2(std::span<const BYTE>{data, n}, op::C7_0A.a, op::C7_0A.b))
  {
    S->counter.store(0, std::memory_order_relaxed);
    dbg("[RECV] C7 0A -> counter=0\n");
  }

  // B3 00 -> counter = 0; found1c0b = false
  if (starts_with2(std::span<const BYTE>{data, n}, op::B3_00.a, op::B3_00.b))
  {
    S->counter.store(0, std::memory_order_relaxed);
    S->found1c0b.store(false, std::memory_order_relaxed);
    dbg("[RECV] B3 00 -> counter=0, found1c0b=false\n");
  }

  // C7 0B -> drop packet(s) and read next (do not return 0 to avoid "connection closed")
  int drop_guard = 0;
  while (starts_with2(std::span<const BYTE>{data, n}, op::C7_0B.a, op::C7_0B.b))
  {
    dbg("[RECV] C7 0B -> drop & read next\n");
    S->counter.store(0, std::memory_order_relaxed);
    S->found1c0b.store(false, std::memory_order_relaxed);

    if (++drop_guard > 8) break;  // defensive upper bound

    ret = do_recv(buf, len);
    if (ret <= 0) return ret;
    data = reinterpret_cast<BYTE*>(buf);
    n = static_cast<size_t>(ret);

    log_hex_buf("[RECV] after-drop", data, n);
  }

  return ret;
}

// -----------------------------------------------------------------------------------------------
// Hooked send()
// - Logs outgoing data and enforces protocol rules:
//   * 26 0C or C7 0A: reset counter & clear 1C 0B flag.
//   * 1C 0B: set flag that changes the next checksum calculation.
//   * If 1C 0B has been seen, rebuild the packet excluding the tail byte and append
//     either seed byte (counter == 0) or checksum byte (counter > 0). Counter rolls mod 0x1000.
// -----------------------------------------------------------------------------------------------

int WSAAPI Trampolines::send(SOCKET s, const char* buf, int len, int flags)
{
  if (!S || !S->original_send) return SOCKET_ERROR;
  if (len <= 0 || !buf) return S->original_send(s, buf, len, flags);

  std::lock_guard<std::mutex> lk(S->send_mtx);

  // Copy input into a mutable buffer we can transform if needed.
  std::vector<BYTE> data(reinterpret_cast<const BYTE*>(buf),
                         reinterpret_cast<const BYTE*>(buf) + len);

  log_hex_buf("[SEND] in        ", data.data(), data.size());

  // Flag/transition opcodes at packet head.
  auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());

  const bool is260c = op::is(bytes, op::_26_0C);
  const bool is1c0b = op::is(bytes, op::_1C_0B);
  const bool isc70a = op::is(bytes, op::C7_0A);

  if (is260c || isc70a)
  {
    S->counter.store(0, std::memory_order_relaxed);
    S->found1c0b.store(false, std::memory_order_relaxed);
    dbg(is260c ? "[SEND] 26 0C -> counter=0, found1c0b=false\n"
               : "[SEND] C7 0A -> counter=0, found1c0b=false\n");
  }

  if (is1c0b)
  {
    S->found1c0b.store(true, std::memory_order_relaxed);
    dbg("[SEND] 1C 0B -> found1c0b=true\n");
  }

  if (S->found1c0b.load(std::memory_order_relaxed))
  {
    std::vector<BYTE> data_for_calc;
    bool append_checksum = false;

    if (data.size() >= 2)
    {
      // Drop the tail byte before computing (as per protocol).
      data.pop_back();

      auto bytes =
          std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());

      const bool is1c0b_now = op::is(bytes, op::_1C_0B);
      if (is1c0b_now)
      {
        // For 1C 0B, compute only over [0x1C, 0x0B].
        data_for_calc.push_back(static_cast<BYTE>(op::_1C_0B.a));
        data_for_calc.push_back(static_cast<BYTE>(op::_1C_0B.b));
      }
      else
      {
        // Otherwise, compute over the payload without the tail.
        data_for_calc = data;
      }
      append_checksum = true;
    }
    else
    {
      dbg("[SEND][strict] len < 2, no-change\n");
    }

    if (append_checksum)
    {
      const int counter = S->counter.load(std::memory_order_relaxed);
      BYTE to_append = 0;

      if (counter == 0)
      {
        to_append = call_seed(data_for_calc.data(), data_for_calc.size());
      }
      else
      {
        const uint32_t low = S->low.load(std::memory_order_relaxed);
        const uint32_t high = S->high.load(std::memory_order_relaxed);
        to_append = call_checksum(data_for_calc.data(), data_for_calc.size(), counter, low, high);
      }

      // Rebuild the final buffer.
      std::vector<BYTE> final_data;
      final_data.reserve(data_for_calc.size() + 1);
      final_data.insert(final_data.end(), data_for_calc.begin(), data_for_calc.end());
      final_data.push_back(to_append);
      data = std::move(final_data);

      // Roll counter (12-bit).
      int nc = (counter + 1) & 0x0FFF;
      S->counter.store(nc, std::memory_order_relaxed);

      char b2[200];
      std::snprintf(b2, sizeof(b2),
                    "[SEND] opcode=%02X append=%u counter=%d->%d found1c0b=%d out_len=%zu\n",
                    (unsigned)data_for_calc[0], (unsigned)to_append, counter, nc,
                    (int)S->found1c0b.load(), data.size());
      dbg(b2);
    }
  }

  log_hex_buf("[SEND] out       ", data.data(), data.size());

  // Call original send with the (possibly) modified payload.
  const int result = S->original_send(s, reinterpret_cast<const char*>(data.data()),
                                      static_cast<int>(data.size()), flags);
  if (result == SOCKET_ERROR)
  {
    int e = WSAGetLastError();
    char b[96];
    std::snprintf(b, sizeof(b), "[SEND][ERR] SOCKET_ERROR wsa=%d\n", e);
    dbg(b);
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
