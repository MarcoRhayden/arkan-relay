#include "infrastructure/hook/Hook_Win32.hpp"
#include <cstring>
#include <cstdlib>

using arkan::relay::application::ports::LogLevel;
namespace hook = arkan::relay::infrastructure::hook;

// Small helper to identify packets by first two bytes.
// Returns true if buffer has at least 2 bytes and matches a,b.
static inline bool is_pkt(const unsigned char* p, int len, unsigned char a, unsigned char b) {
  return len >= 2 && static_cast<unsigned char>(p[0]) == a && static_cast<unsigned char>(p[1]) == b;
}

/*
 * Hook_Win32 ctor
 *  - Stores logger and settings references
 *  - Parses the optional hex addresses from Settings (TOML) into uintptr_t
 *    These addresses are *absolute addresses inside the RO client process*
 */
hook::Hook_Win32::Hook_Win32(ports::ILogger& log, const domain::Settings& s)
  : log_(log), cfg_(s) {
  auto parse = [](const std::optional<std::string>& hex)->uintptr_t{
    if (!hex || hex->empty()) return 0;
    return static_cast<uintptr_t>(std::strtoull(hex->c_str(), nullptr, 16));
  };
  addr_send_ptr_ = parse(s.fnSendAddr);
  addr_recv_ptr_ = parse(s.fnRecvAddr);
  addr_seed_     = parse(s.fnSeedAddr);
  addr_checksum_ = parse(s.fnChecksumAddr);
}

/*
 * patch_pointer(address, new_value, old_value)
 *  - Switches the contents of a pointer-sized slot in target process memory.
 *  - We virtual-protect the region to PAGE_READWRITE, write the new value,
 *    restore previous protection and return true on success.
 *  - If old_value != nullptr, we return the previously stored pointer in *old_value.
 *
 *  IMPORTANT: This expects 'address' to be the address of a slot that stores a function pointer,
 *             not the address of the function itself.
 */
bool hook::Hook_Win32::patch_pointer(uintptr_t address, uintptr_t new_value, uintptr_t* old_value) {
  if (!address) return false;

  DWORD oldProt{};
  if (!VirtualProtect(reinterpret_cast<LPVOID>(address), sizeof(uintptr_t), PAGE_READWRITE, &oldProt))
    return false;

  auto* slot = reinterpret_cast<uintptr_t*>(address);
  if (old_value) *old_value = *slot;
  *slot = new_value;

  DWORD tmp{};
  VirtualProtect(reinterpret_cast<LPVOID>(address), sizeof(uintptr_t), oldProt, &tmp);
  return true;
}

/*
 * install()
 *  - Validates that all required addresses were provided via TOML:
 *      send slot, recv slot, seed function, checksum function.
 *  - Stores the seed/checksum function pointers into static globals used by trampolines.
 *  - Patches the send/recv slot addresses to point to our hooked_* trampolines,
 *    while capturing the original function pointers (so we can call-through and restore later).
 *  - Resets the internal counters/state and publishes the 'this' pointer so trampolines can
 *    dispatch the Bridge callbacks (on_send/on_recv).
 */
bool hook::Hook_Win32::install() {
  if (!addr_send_ptr_ || !addr_recv_ptr_ || !addr_seed_ || !addr_checksum_) {
    log_.app(LogLevel::err, "Hook_Win32.install(): missing required addresses (send/recv/seed/checksum).");
    return false;
  }

  // Set static function pointers used by trampolines.
  s_seed_fp     = reinterpret_cast<seed_fp>(addr_seed_);
  s_checksum_fp = reinterpret_cast<sum_fp>(addr_checksum_);
  if (!s_seed_fp || !s_checksum_fp) {
    log_.app(LogLevel::err, "Hook_Win32.install(): invalid seed/checksum function pointers.");
    return false;
  }

  // Install hooks, capturing original function pointers from the slots.
  uintptr_t orig_send = 0;
  if (!patch_pointer(addr_send_ptr_, reinterpret_cast<uintptr_t>(&hooked_send), &orig_send))
    return false;
  s_original_send = reinterpret_cast<send_fp>(orig_send);

  uintptr_t orig_recv = 0;
  if (!patch_pointer(addr_recv_ptr_, reinterpret_cast<uintptr_t>(&hooked_recv), &orig_recv))
    return false;
  s_original_recv = reinterpret_cast<recv_fp>(orig_recv);

  // Reset state and publish 'this' so trampolines can call callbacks.
  s_counter.store(0, std::memory_order_relaxed);
  s_found1c0b.store(false, std::memory_order_relaxed);
  s_self.store(this, std::memory_order_release);

  log_.app(LogLevel::info, "SEND/RECV hooked successfully.");
  return true;
}

/*
 * uninstall()
 *  - Restores original send/recv pointers back into the slot addresses, if available.
 *  - Clears the published 'self' pointer to stop trampolines from dispatching callbacks.
 */
void hook::Hook_Win32::uninstall() {
  s_self.store(nullptr, std::memory_order_release);

  if (addr_send_ptr_ && s_original_send) {
    patch_pointer(addr_send_ptr_, reinterpret_cast<uintptr_t>(s_original_send), nullptr);
  }
  if (addr_recv_ptr_ && s_original_recv) {
    patch_pointer(addr_recv_ptr_, reinterpret_cast<uintptr_t>(s_original_recv), nullptr);
  }
}

/*
 * call_seed()
 *  - Calls the external SEED function (provided by address in TOML).
 *  - Returns a single byte to be appended as checksum on the first 0x1C 0x0B packet.
 *  - If the original RO seed also updates 'high'/'low' words used later by CHECKSUM,
 *    capture them here.
 */
BYTE hook::Hook_Win32::call_seed(const BYTE* data, size_t len) {
  auto fp = s_seed_fp;
  if (!fp) return 0;
  BYTE v = fp(data, static_cast<UINT>(len));

  // TODO: If the seed export updates state that yields (high/low), assign here:
  // s_low.store(...); s_high.store(...);

  return v;
}

/*
 * call_checksum()
 *  - Calls the external CHECKSUM function (provided by address in TOML).
 *  - Inputs: current packet bytes, rolling counter, low/high words (if required).
 *  - Returns the checksum byte to append to the outgoing packet.
 */
BYTE hook::Hook_Win32::call_checksum(const BYTE* data, size_t len,
                                     uint32_t counter, uint32_t low, uint32_t high) {
  auto fp = s_checksum_fp;
  if (!fp) return 0;
  return fp(data, static_cast<UINT>(len), counter, low, high);
}

/*
 * hooked_send()
 *  - This is a trampoline for 'send' that:
 *      1) Detects special packets:
 *         - 0x26 0x0C → reset s_counter and s_found1c0b
 *         - 0x1C 0x0B → mark that we are in the "special" phase requiring SEED/CHECKSUM
 *      2) If in the special phase and len>1, recompute the trailing checksum byte:
 *         - For the very first 0x1C 0x0B pair, the checksum is SEED(data)
 *         - For subsequent packets, checksum is CHECKSUM(data, counter, low, high)
 *           (low/high may come from SEED)
 *      3) Increments the rolling counter 0x1000 while in the special phase.
 *      4) Dispatches the 'on_send' callback, if provided by BridgeService.
 *      5) Calls the original send() with the possibly modified buffer.
 */
int WSAAPI hook::Hook_Win32::hooked_send(SOCKET s, const char* buf, int len, int flags) {
  if (!s_original_send) return SOCKET_ERROR;

  const unsigned char* u = reinterpret_cast<const unsigned char*>(buf);
  const bool is260c = is_pkt(u, len, 0x26, 0x0C);
  const bool is1c0b = is_pkt(u, len, 0x1C, 0x0B);

  // 0x26 0x0C: reset internal rolling state (beginning of auth/handshake)
  if (is260c) {
    s_counter.store(0, std::memory_order_relaxed);
    s_found1c0b.store(false, std::memory_order_relaxed);
  }

  // 0x1C 0x0B: enter the special framing (seed/checksum flow)
  if (is1c0b) {
    s_found1c0b.store(true, std::memory_order_relaxed);
  }

  std::vector<unsigned char> tmp;
  const bool in_special = s_found1c0b.load(std::memory_order_relaxed);

  if (in_special && len > 1) {
    /*
     *  - For the very first 1C 0B packet (counter == 0): keep only the 2-byte header
     *    and compute SEED on those 2 bytes → output = [1C,0B, seed(...) ]   (3 bytes)
     *  - For subsequent packets (counter > 0): keep the whole packet minus the trailing
     *    checksum byte, then compute CHECKSUM on that kept region and append it.
     */
    if (is1c0b && s_counter.load(std::memory_order_relaxed) == 0) {
      // keep exactly the 2-byte header
      tmp.assign(u, u + 2);

      // SEED over the two bytes
      const BYTE add = call_seed(tmp.data(), tmp.size());
      tmp.push_back(add);
    } else {
      // keep everything but the trailing checksum
      // (len >= 2 here, so len-1 is safe as "drop last byte")
      tmp.assign(u, u + (len - 1));

      const BYTE add = call_checksum(
        tmp.data(), tmp.size(),
        static_cast<uint32_t>(s_counter.load(std::memory_order_relaxed)),
        s_low.load(std::memory_order_relaxed),
        s_high.load(std::memory_order_relaxed)
      );
      tmp.push_back(add);
    }

    buf = reinterpret_cast<const char*>(tmp.data());
    len = static_cast<int>(tmp.size());
  }

  // Rolling counter increments while in special mode (mod 0x1000)
  if (in_special) {
    int c = s_counter.load(std::memory_order_relaxed);
    c = (c + 1) & 0x0FFF;
    s_counter.store(c, std::memory_order_relaxed);
  }

  // Dispatch to BridgeService (if any listener was set)
  if (auto self = s_self.load(std::memory_order_acquire)) {
    if (self->on_send) {
      self->on_send(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(buf),
        static_cast<size_t>(len)
      });
    }
  }

  return s_original_send(s, buf, len, flags);
}

/*
 * hooked_recv()
 *  - Trampoline for 'recv' that:
 *      1) Calls the original recv()
 *      2) Applies state resets based on specific incoming packets:
 *         - 0xC7 0x0A → reset counter (map changed)
 *         - 0xB3 0x00 → reset counter and leave "special" phase (switch char)
 *         - 0xC7 0x0B → reset counter, leave "special" phase, and drop packet (return 0)
 *      3) Dispatches 'on_recv' callback if provided
 *      4) Returns the original recv() result (or 0 for the "drop" case)
 */
int WSAAPI hook::Hook_Win32::hooked_recv(SOCKET s, char* buf, int len, int flags) {
  if (!s_original_recv) return SOCKET_ERROR;

  int ret = s_original_recv(s, buf, len, flags);
  if (ret <= 0) return ret;

  const unsigned char* u = reinterpret_cast<const unsigned char*>(buf);

  // Incoming control packets that reset or drop state.
  if (is_pkt(u, ret, 0xC7, 0x0A)) s_counter.store(0, std::memory_order_relaxed);                                   // map changed
  if (is_pkt(u, ret, 0xB3, 0x00)) { s_counter.store(0, std::memory_order_relaxed); s_found1c0b.store(false, std::memory_order_relaxed); } // switch char
  if (is_pkt(u, ret, 0xC7, 0x0B)) { s_counter.store(0, std::memory_order_relaxed); s_found1c0b.store(false, std::memory_order_relaxed); return 0; } // drop

  // Dispatch to BridgeService (if a callback is set)
  if (auto self = s_self.load(std::memory_order_acquire)) {
    if (self->on_recv) {
      self->on_recv(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(buf),
        static_cast<size_t>(ret)
      });
    }
  }

  return ret;
}
