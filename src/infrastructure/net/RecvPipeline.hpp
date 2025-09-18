#pragma once
#include <cstdint>
#include <span>

#include "application/services/protocol/ChecksumState.hpp"
#include "application/services/protocol/ProtocolScanner.hpp"

namespace arkan::relay::infrastructure::net
{
struct RecvPipeline
{
  explicit RecvPipeline(const arkan::relay::application::services::IProtocolScanner& s) : scanner(s)
  {
  }

  void process(std::span<const uint8_t> buf, arkan::relay::application::services::ChecksumState& S,
               bool& drop_head_c70b) const;

 private:
  const arkan::relay::application::services::IProtocolScanner& scanner;
};

}  // namespace arkan::relay::infrastructure::net
