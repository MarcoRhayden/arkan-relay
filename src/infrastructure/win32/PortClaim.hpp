#pragma once

#ifdef _WIN32
#include <windows.h>

#include <cstdint>
#include <string>

namespace arkan::relay::infrastructure
{

class PortClaim
{
 public:
  PortClaim() = default;

  ~PortClaim()
  {
    release();
  }

  // Try to claim host:port
  bool claim(const std::string& host, uint16_t port);

  // Release claim if any.
  void release();

  // Query helpers
  bool is_claimed() const
  {
    return h_ != nullptr;
  }

  // Returns the kernel object name used for the claim
  std::string claimed_name() const
  {
    return name_;
  }

 private:
  // Build a stable name for the host:port
  static std::string make_name(const std::string& host, uint16_t port);

  HANDLE h_{nullptr};
  std::string name_;
};

}  // namespace arkan::relay::infrastructure

#endif
