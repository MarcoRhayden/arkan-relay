#pragma once

#include <cstddef>
#include <cstdint>

namespace arkan::relay::application::services
{
struct ChecksumState;  // fwd-decl

struct IChecksumService
{
  virtual ~IChecksumService() = default;

  virtual uint8_t seed(const uint8_t* data, size_t len, ChecksumState& S) = 0;

  virtual uint8_t checksum(const uint8_t* data, size_t len, uint32_t counter, uint32_t low,
                           uint32_t high, ChecksumState& S) = 0;
};
}  // namespace arkan::relay::application::services
