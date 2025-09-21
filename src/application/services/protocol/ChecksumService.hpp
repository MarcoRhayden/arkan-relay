#pragma once

#include <cstddef>
#include <cstdint>

#include "ChecksumState.hpp"

namespace arkan::relay::application::services
{

struct IChecksumService
{
  virtual ~IChecksumService() = default;

  // seed():
  // - Generate one pseudo-random byte (signed range −128..127) and append it to the payload
  //   for the seed64 call.
  // - Invoke seed64 (via callback or concrete implementation).
  // - Store the high/low parts of the returned seed64 into S.high / S.low.
  // - Return the random byte to be appended (NOT the seed64 value).
  virtual uint8_t seed(const uint8_t* data, size_t len, ChecksumState& S) = 0;

  // checksum():
  // - Rebuild seed64 as (high << 32) | low and perform the checksum computation.
  // - Return the checksum byte to be appended.
  virtual uint8_t checksum(const uint8_t* data, size_t len, uint32_t counter, uint32_t low,
                           uint32_t high, ChecksumState& S) = 0;
};

}  // namespace arkan::relay::application::services
