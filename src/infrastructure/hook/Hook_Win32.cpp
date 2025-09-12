#include "infrastructure/hook/Hook_Win32.hpp"
#include <cstring>

using arkan::relay::application::ports::LogLevel;
namespace hook = arkan::relay::infrastructure::hook;

static inline bool is_pkt(const unsigned char* p, int len, unsigned char a, unsigned char b) {
  return len >= 2 && static_cast<unsigned char>(p[0]) == a && static_cast<unsigned char>(p[1]) == b;
}

hook::Hook_Win32::Hook_Win32(application::ports::ILogger& log, const domain::Settings& s)
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

bool hook::Hook_Win32::patch_pointer(uintptr_t address, uintptr_t new_value, uintptr_t* old_value) {
  if (!address) return false;

  DWORD oldProt{};
  if (!VirtualProtect(reinterpret_cast<LPVOID>(address), sizeof(uintptr_t), PAGE_READWRITE, &oldProt))
    return false;

  uintptr_t* slot = reinterpret_cast<uintptr_t*>(address);
  if (old_value) *old_value = *slot;
  *slot = new_value;

  DWORD tmp{};
  VirtualProtect(reinterpret_cast<LPVOID>(address), sizeof(uintptr_t), oldProt, &tmp);
  return true;
}

bool hook::Hook_Win32::apply() {
  if (!addr_send_ptr_ || !addr_recv_ptr_ || !addr_seed_ || !addr_checksum_) {
    log_.app(LogLevel::err, "Hook_Win32.apply(): missing required addresses.");
    return false;
  }
  if (!patch_pointer(addr_send_ptr_,  reinterpret_cast<uintptr_t>(&hooked_send),
                     reinterpret_cast<uintptr_t*>(&s_original_send)))
    return false;
  if (!patch_pointer(addr_recv_ptr_,  reinterpret_cast<uintptr_t>(&hooked_recv),
                     reinterpret_cast<uintptr_t*>(&s_original_recv)))
    return false;

  log_.app(LogLevel::info, "SEND/RECV hooked successfully.");
  return true;
}

void hook::Hook_Win32::detach() {
  // restore original functions
}

BYTE hook::Hook_Win32::call_seed(const BYTE* data, size_t len) {
  using seed_fp = BYTE (WINAPI*)(const BYTE*, UINT);
  auto fp = reinterpret_cast<seed_fp>(addr_seed_);
  if (!fp) return 0;
  auto randByte = fp(data, static_cast<UINT>(len));

  return randByte;
}

BYTE hook::Hook_Win32::call_checksum(const BYTE* data, size_t len, uint32_t counter, uint32_t low, uint32_t high) {
  using sum_fp = BYTE (WINAPI*)(const BYTE*, UINT, UINT, UINT, UINT);
  auto fp = reinterpret_cast<sum_fp>(addr_checksum_);
  if (!fp) return 0;
  return fp(data, static_cast<UINT>(len), counter, low, high); 
}

int WSAAPI hook::Hook_Win32::hooked_send(SOCKET s, const char* buf, int len, int flags) {
  if (!s_original_send) return SOCKET_ERROR;

  const unsigned char* u = reinterpret_cast<const unsigned char*>(buf);
  bool is260c = is_pkt(u, len, 0x26, 0x0C);
  bool is1c0b = is_pkt(u, len, 0x1C, 0x0B);

  if (is260c) {
    s_counter.store(0, std::memory_order_relaxed);
    s_found1c0b.store(false, std::memory_order_relaxed);
  }

  if (is1c0b) {
    s_found1c0b.store(true, std::memory_order_relaxed);
  }

  std::vector<unsigned char> tmp;
  if (s_found1c0b.load(std::memory_order_relaxed) && len > 1) {
    // remove last byte (checksum) and recalc   
    tmp.assign(u, u + (is1c0b ? 2 : len - 1));
    // first 1C 0B → SEED (counter==0), else CHECKSUM(counter,low,high)  :contentReference[oaicite:13]{index=13}
    BYTE add{};
    if (s_counter.load() == 0) {
      add = call_seed(tmp.data(), tmp.size());
      // high/low were updated in seed (seed >> 32 / &0xFFFFFFFF) :contentReference[oaicite:14]{index=14}
      // If needed, retrieve high/low here from the actual SEED (impl. depends on your export).
    } else {
      add = call_checksum(tmp.data(), tmp.size(), s_counter.load(), s_low.load(), s_high.load());
    }
    tmp.push_back(add);
    buf = reinterpret_cast<const char*>(tmp.data());
    len = static_cast<int>(tmp.size());
  }

  if (s_found1c0b.load()) {
    // counter = (counter + 1) & 0xFFF  :contentReference[oaicite:15]{index=15}
    int c = s_counter.load();
    c = (c + 1) & 0x0FFF;
    s_counter.store(c);
  }

  return s_original_send(s, buf, len, flags);
}

int WSAAPI hook::Hook_Win32::hooked_recv(SOCKET s, char* buf, int len, int flags) {
  if (!s_original_recv) return SOCKET_ERROR;
  int ret = s_original_recv(s, buf, len, flags);
  if (ret <= 0) return ret;

  const unsigned char* u = reinterpret_cast<const unsigned char*>(buf);
  if (is_pkt(u, ret, 0xC7, 0x0A)) s_counter.store(0);             // map changed  :contentReference[oaicite:16]{index=16}
  if (is_pkt(u, ret, 0xB3, 0x00)) { s_counter.store(0); s_found1c0b.store(false); } // switch char  :contentReference[oaicite:17]{index=17}
  if (is_pkt(u, ret, 0xC7, 0x0B)) { s_counter.store(0); s_found1c0b.store(false); return 0; } // drop  :contentReference[oaicite:18]{index=18}

  return ret;
}

void hook::Hook_Win32::on_client_send(std::span<const std::byte>) {}
void hook::Hook_Win32::on_client_recv(std::span<const std::byte>) {}
