#pragma once
#include <cstdint>
#include <string>

#include "application/ports/ILogger.hpp"

namespace arkan::relay::infrastructure::win32
{

struct ResolvedAddrs
{
  uintptr_t send_slot{};
  uintptr_t recv_slot{};
  uintptr_t seed_fn{};
  uintptr_t checksum_fn{};
};

class AddressResolver
{
 public:
  // Parse hex strings from Settings, resolve RVAs (base + offset) if needed.
  static ResolvedAddrs resolve(const std::string& sendHex, const std::string& recvHex,
                               const std::string& seedHex, const std::string& checksumHex);

  // Log page state/protection for diagnostics (debug-level).
  static void log_pages(arkan::relay::application::ports::ILogger& log, const ResolvedAddrs& addrs);

 private:
  static uintptr_t parse_hex_ptr(const std::string& hex);
  static uintptr_t ensure_absolute_or_offset(uintptr_t addr);
  static const char* protect_to_str(unsigned long p);
  static const char* state_to_str(unsigned long s);
};

}  // namespace arkan::relay::infrastructure::win32
