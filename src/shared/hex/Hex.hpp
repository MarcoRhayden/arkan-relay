#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>

#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif

namespace arkan::relay::shared::hex
{

// -----------------------------------------------------------------------------
// hexadecimal width for pointers
// -----------------------------------------------------------------------------
constexpr int ptr_width = (sizeof(uintptr_t) == 4) ? 8 : (sizeof(uintptr_t) == 8) ? 16 : 8;

// -----------------------------------------------------------------------------
// hex_dump(data, max_len)
// -----------------------------------------------------------------------------
inline std::string hex_dump(std::span<const std::byte> data, size_t max_len = 64)
{
  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setfill('0');

  const size_t take = (max_len > 0) ? (std::min)(max_len, data.size()) : data.size();

  for (size_t i = 0; i < take; ++i)
  {
    const auto v = static_cast<unsigned int>(std::to_integer<unsigned char>(data[i]));
    oss << std::setw(2) << v << ' ';
  }

  if (take < data.size())
  {
    oss << "...(" << data.size() << " bytes total)";
  }

  return oss.str();
}

// Overloads
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

// -----------------------------------------------------------------------------
// to_hex(value, width)
// -----------------------------------------------------------------------------
template <class UInt>
inline std::string to_hex(UInt value, int width = -1)
{
  static_assert(std::is_unsigned<UInt>::value, "to_hex requires an unsigned integer type");

  if (width < 0) width = static_cast<int>(sizeof(UInt) * 2);

  std::ostringstream oss;
  oss << "0x" << std::uppercase << std::hex << std::setw(width) << std::setfill('0')
      << static_cast<unsigned long long>(value);
  return oss.str();
}

// -----------------------------------------------------------------------------
// to_hex_ptr(p)
// -----------------------------------------------------------------------------
inline std::string to_hex_ptr(uintptr_t p)
{
  std::ostringstream oss;
  oss << "0x" << std::uppercase << std::hex << std::setw(ptr_width) << std::setfill('0')
      << static_cast<unsigned long long>(p);
  return oss.str();
}

// -----------------------------------------------------------------------------
// make_line(tag, span, max)
// -----------------------------------------------------------------------------
inline std::string make_line(const char* tag, std::span<const std::byte> sp, size_t max = 32)
{
  std::string line;
  line.reserve(64 + max * 3);
  line.append(tag).append(" n=").append(std::to_string(sp.size())).append(": ");
  line += hex_dump(sp, max);
  return line;
}

}  // namespace arkan::relay::shared::hex
