#pragma once
#include <cstdint>
#include <span>
#include <vector>

#include "application/ports/IChecksumService.hpp"
#include "application/services/protocol/ChecksumState.hpp"

namespace arkan::relay::infrastructure::net
{

struct SendPipeline
{
  explicit SendPipeline(arkan::relay::application::services::IChecksumService& svc) : svc_(svc) {}

  // In-place transform
  void transform(std::vector<uint8_t>& data,
                 arkan::relay::application::services::ChecksumState& S) const;

 private:
  arkan::relay::application::services::IChecksumService& svc_;
};

}  // namespace arkan::relay::infrastructure::net
