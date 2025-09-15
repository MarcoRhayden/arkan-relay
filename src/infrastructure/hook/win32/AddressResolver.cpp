#include "win32/AddressResolver.hpp"

#include <windows.h>

#include <cstdio>
#include <string>

using arkan::relay::application::ports::LogLevel;

namespace arkan::relay::infrastructure::win32
{

static inline bool is_commit(void* p)
{
  MEMORY_BASIC_INFORMATION mbi{};
  return ::VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.State == MEM_COMMIT;
}

uintptr_t AddressResolver::parse_hex_ptr(const std::string& hex)
{
  if (hex.empty()) return 0;
  return static_cast<uintptr_t>(std::strtoull(hex.c_str(), nullptr, 16));
}

uintptr_t AddressResolver::ensure_absolute_or_offset(uintptr_t addr)
{
  if (!addr) return 0;
  if (is_commit(reinterpret_cast<void*>(addr))) return addr;
  const auto base = reinterpret_cast<uintptr_t>(::GetModuleHandleW(nullptr));
  const uintptr_t candidate = base + addr;
  if (is_commit(reinterpret_cast<void*>(candidate))) return candidate;
  return addr;
}

ResolvedAddrs AddressResolver::resolve(const std::string& sendHex, const std::string& recvHex,
                                       const std::string& seedHex, const std::string& checksumHex)
{
  ResolvedAddrs out{};
  out.send_slot = ensure_absolute_or_offset(parse_hex_ptr(sendHex));
  out.recv_slot = ensure_absolute_or_offset(parse_hex_ptr(recvHex));
  out.seed_fn = ensure_absolute_or_offset(parse_hex_ptr(seedHex));
  out.checksum_fn = ensure_absolute_or_offset(parse_hex_ptr(checksumHex));
  return out;
}

const char* AddressResolver::protect_to_str(unsigned long p)
{
  switch (p & 0xFF)
  {
    case PAGE_NOACCESS:
      return "NOACCESS";
    case PAGE_READONLY:
      return "READONLY";
    case PAGE_READWRITE:
      return "READWRITE";
    case PAGE_WRITECOPY:
      return "WRITECOPY";
    case PAGE_EXECUTE:
      return "EXECUTE";
    case PAGE_EXECUTE_READ:
      return "EXECUTE_READ";
    case PAGE_EXECUTE_READWRITE:
      return "EXECUTE_READWRITE";
    case PAGE_EXECUTE_WRITECOPY:
      return "EXECUTE_WRITECOPY";
    default:
      return "UNKNOWN";
  }
}

const char* AddressResolver::state_to_str(unsigned long s)
{
  if (s == MEM_COMMIT) return "COMMIT";
  if (s == MEM_RESERVE) return "RESERVE";
  if (s == MEM_FREE) return "FREE";
  return "UNKNOWN";
}

void AddressResolver::log_pages(arkan::relay::application::ports::ILogger& log,
                                const ResolvedAddrs& addrs)
{
  auto log_one = [&](const char* name, uintptr_t addr)
  {
    if (!addr) return;
    MEMORY_BASIC_INFORMATION mbi{};
    if (::VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == sizeof(mbi))
    {
      std::string msg = std::string("Addr '") + name + "' @ " + to_hex_ptr(addr) + " page={" +
                        state_to_str(mbi.State) + "," + protect_to_str(mbi.Protect) + "}";
      log.app(LogLevel::debug, msg);
    }
  };
  log_one("sendSlot", addrs.send_slot);
  log_one("recvSlot", addrs.recv_slot);
  log_one("seedFn", addrs.seed_fn);
  log_one("checksumFn", addrs.checksum_fn);
}

}  // namespace arkan::relay::infrastructure::win32
