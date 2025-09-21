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

// overload for const std::byte* (handy when calling with vector<std::byte>.data())
inline std::string hex_dump(const std::byte* data, size_t len, size_t max_len = 64)
{
  return hex_dump(std::span<const std::byte>(data, len), max_len);
}

// template overload for spans of any trivially-copyable element type that is NOT std::byte
template <typename T, typename = std::enable_if_t<!std::is_same_v<std::remove_cv_t<T>, std::byte>>>
inline std::string hex_dump(std::span<const T> data, size_t max_len = 64)
{
  // reinterpret the span<T> as bytes: pointer cast + multiply length by sizeof(T)
  auto ptr = reinterpret_cast<const std::byte*>(data.data());
  const size_t len_bytes = data.size() * sizeof(T);
  return hex_dump(std::span<const std::byte>(ptr, len_bytes), max_len);
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
