#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace arkan::relay::application::services
{

struct ScanResult
{
  bool head_c70a = false;
  bool head_b300 = false;
  bool head_c70b = false;
  size_t off_c70a = SIZE_MAX;  // first inline (non-zero) occurrence
};

struct IProtocolScanner
{
  virtual ~IProtocolScanner() = default;
  virtual ScanResult scan(std::span<const uint8_t> buf) const = 0;
};

}  // namespace arkan::relay::application::services
