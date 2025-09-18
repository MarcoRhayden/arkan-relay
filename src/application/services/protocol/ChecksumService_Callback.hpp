#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#include "application/ports/IChecksumService.hpp"

namespace arkan::relay::application::services
{
struct ChecksumState;

class ChecksumService_Callback : public IChecksumService
{
 public:
  using Seed64Fn = std::function<unsigned long long(uint8_t* /*data*/, unsigned /*len*/)>;
  using ChecksumFn = std::function<uint8_t(uint8_t* /*data*/, unsigned /*len*/,
                                           unsigned /*counter*/, unsigned long long /*seed64*/)>;

  explicit ChecksumService_Callback(Seed64Fn seed64_fn, ChecksumFn checksum_fn);

  // IChecksumService
  uint8_t seed(const uint8_t* data, size_t len, ChecksumState& S) override;
  uint8_t checksum(const uint8_t* data, size_t len, uint32_t counter, uint32_t low, uint32_t high,
                   ChecksumState& S) override;

 private:
  static void seed_prng_once() noexcept;

  Seed64Fn seed64_fn_;
  ChecksumFn checksum_fn_;
};
}  // namespace arkan::relay::application::services
