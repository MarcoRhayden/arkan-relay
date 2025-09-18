#include "infrastructure/net/RecvPipeline.hpp"

#include <cstddef>

#include "application/services/protocol/ProtocolScanner.hpp"
#include "domain/protocol/Opcodes.hpp"

namespace arkan::relay::infrastructure::net
{
namespace app = arkan::relay::application::services;

void RecvPipeline::process(std::span<const uint8_t> buf, app::ChecksumState& S,
                           bool& drop_head_c70b) const
{
  drop_head_c70b = false;
  if (buf.size() < 2) return;

  const app::ScanResult r = scanner.scan(buf);

  // HEAD resets
  if (r.head_c70a || r.head_b300)
  {
    S.reset_all();
    return;
  }

  // Inline scan ONLY C7 0A
  if (r.off_c70a != SIZE_MAX)
  {
    S.reset_all();
  }

  // HEAD drop condition (C7 0B)
  if (r.head_c70b)
  {
    drop_head_c70b = true;
  }
}

}  // namespace arkan::relay::infrastructure::net
