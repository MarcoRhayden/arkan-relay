#include "application/services/protocol/ChecksumService_Callback.hpp"

#include <chrono>
#include <cstdlib>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

#include "application/services/protocol/ChecksumState.hpp"

namespace arkan::relay::application::services
{

ChecksumService_Callback::ChecksumService_Callback(Seed64Fn seed64_fn, ChecksumFn checksum_fn)
    : seed64_fn_(std::move(seed64_fn)), checksum_fn_(std::move(checksum_fn))
{
}

void ChecksumService_Callback::seed_prng_once() noexcept
{
  static bool done = false;
  if (done) return;
#ifdef _WIN32
  LARGE_INTEGER t;
  ::QueryPerformanceCounter(&t);
  std::srand(static_cast<unsigned>(t.QuadPart ^ GetTickCount() ^ GetCurrentProcessId()));
#else
  const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  std::srand(static_cast<unsigned>(now));
#endif
  done = true;
}

uint8_t ChecksumService_Callback::seed(const uint8_t* data, size_t len, ChecksumState& S)
{
  seed_prng_once();

  const int r = std::rand() % 256;
  const int rb = r - 128;
  const uint8_t extra = static_cast<uint8_t>(rb & 0xFF);

  std::vector<uint8_t> tmp;
  tmp.reserve(len + 1);
  tmp.insert(tmp.end(), data, data + len);
  tmp.push_back(extra);

  const unsigned long long seed64 = seed64_fn_(tmp.data(), static_cast<unsigned>(tmp.size()));

  const uint32_t high = static_cast<uint32_t>(seed64 >> 32);
  const uint32_t low = static_cast<uint32_t>(seed64 & 0xFFFFFFFFull);
  S.high.store(high, std::memory_order_relaxed);
  S.low.store(low, std::memory_order_relaxed);

  return extra;
}

uint8_t ChecksumService_Callback::checksum(const uint8_t* data, size_t len, uint32_t counter,
                                           uint32_t low, uint32_t high, ChecksumState& /*S*/)
{
  const unsigned long long seed64 =
      (static_cast<unsigned long long>(high) << 32) | static_cast<unsigned long long>(low);

  return checksum_fn_(const_cast<uint8_t*>(data), static_cast<unsigned>(len),
                      static_cast<unsigned>(counter), seed64);
}

}  // namespace arkan::relay::application::services
