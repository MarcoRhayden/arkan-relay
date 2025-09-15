#pragma once

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>

namespace arkan::relay::shared::hex
{

// ----------------------------------------------------------------------------------------------
// Architecture-dependent pointer width (hex digits)
// ----------------------------------------------------------------------------------------------
constexpr int ptr_width = (sizeof(uintptr_t) == 4) ? 8 : (sizeof(uintptr_t) == 8) ? 16 : 8;

// ----------------------------------------------------------------------------------------------
// to_hex(value, width)
// - Formats an unsigned integer to "0x" prefixed, uppercase hex, zero-padded.
// - Default width = sizeof(UInt) * 2 if not provided.
// ----------------------------------------------------------------------------------------------
template <class UInt, typename = std::enable_if_t<std::is_unsigned_v<UInt>>>
inline std::string to_hex(UInt value, int width = -1)
{
  if (width < 0) width = static_cast<int>(sizeof(UInt) * 2);
  std::ostringstream oss;
  oss << "0x" << std::uppercase << std::hex << std::setw(width) << std::setfill('0')
      << static_cast<unsigned long long>(value);
  return oss.str();
}

// ----------------------------------------------------------------------------------------------
// to_hex_ptr(p)
// - Formats a pointer-sized integer using architecture-specific width.
// ----------------------------------------------------------------------------------------------
inline std::string to_hex_ptr(uintptr_t p)
{
  std::ostringstream oss;
  oss << "0x" << std::uppercase << std::hex << std::setw(ptr_width) << std::setfill('0')
      << static_cast<unsigned long long>(p);
  return oss.str();
}

// ----------------------------------------------------------------------------------------------
// hex_dump(data, max_len)
// - Compact one-line hex dump with space-separated bytes.
// - Truncates after max_len bytes and appends "...(N bytes total)".
// - Accepts std::span<std::byte>; overloads provided for common byte views.
// ----------------------------------------------------------------------------------------------
inline std::string hex_dump(std::span<const std::byte> data, size_t max_len = 64)
{
  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setfill('0');

  size_t count = 0;
  for (auto b : data)
  {
    const auto v = static_cast<unsigned int>(std::to_integer<unsigned char>(b));
    oss << std::setw(2) << v << ' ';
    if (++count >= max_len)
    {
      oss << "...(" << data.size() << " bytes total)";
      break;
    }
  }
  return oss.str();
}

// Convenience overloads (no copies):
inline std::string hex_dump(const uint8_t* data, size_t len, size_t max_len = 64)
{
  return hex_dump(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), len),
                  max_len);
}

inline std::string hex_dump(const char* data, size_t len, size_t max_len = 64)
{
  return hex_dump(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), len),
                  max_len);
}

}  // namespace arkan::relay::shared::hex
