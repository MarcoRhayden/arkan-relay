#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace arkan::relay::domain::protocol::op
{

// Two-byte opcode (A,B), e.g. 0xC7 0x0A
struct Opcode2
{
  uint8_t a;
  uint8_t b;

  // Match against the first 2 bytes of a buffer
  constexpr bool matches(std::span<const uint8_t> buf) const noexcept
  {
    return buf.size() >= 2 && buf[0] == a && buf[1] == b;
  }
  // Overload for std::byte buffers
  constexpr bool matches(std::span<const std::byte> buf) const noexcept
  {
    return buf.size() >= 2 && std::to_integer<uint8_t>(buf[0]) == a &&
           std::to_integer<uint8_t>(buf[1]) == b;
  }
};

// ---- Canonical opcodes ----
inline constexpr Opcode2 C7_0A{0xC7, 0x0A};   // resets counter
inline constexpr Opcode2 C7_0B{0xC7, 0x0B};   // drop/keepalive handling
inline constexpr Opcode2 B3_00{0xB3, 0x00};   // reset counter + clear flags
inline constexpr Opcode2 _26_0C{0x26, 0x0C};  // session/init boundary
inline constexpr Opcode2 _1C_0B{0x1C, 0x0B};  // special trigger
// inline constexpr Opcode2 _0B_1C{0x0B, 0x1C};  // special trigger / reset checksum

// Conveniences
constexpr bool is(std::span<const uint8_t> buf, Opcode2 op) noexcept
{
  return op.matches(buf);
}
constexpr bool is(std::span<const std::byte> buf, Opcode2 op) noexcept
{
  return op.matches(buf);
}

}  // namespace arkan::relay::domain::protocol::op
