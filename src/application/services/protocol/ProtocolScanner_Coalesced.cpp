#include "application/services/protocol/ProtocolScanner_Coalesced.hpp"

#include "domain/protocol/Opcodes.hpp"

namespace arkan::relay::application::services
{
namespace op = arkan::relay::domain::protocol::op;

class ProtocolScannerCoalesced final : public IProtocolScanner
{
 public:
  ScanResult scan(std::span<const uint8_t> buf) const override
  {
    ScanResult r{};
    if (buf.size() >= 2)
    {
      auto h0 = buf[0], h1 = buf[1];
      r.head_c70a = (h0 == op::C7_0A.a && h1 == op::C7_0A.b);
      r.head_b300 = (h0 == op::B3_00.a && h1 == op::B3_00.b);
      r.head_c70b = (h0 == op::C7_0B.a && h1 == op::C7_0B.b);
    }
    // inline C7 0A
    for (size_t i = 1; i + 1 < buf.size(); ++i)
    {
      if (buf[i] == op::C7_0A.a && buf[i + 1] == op::C7_0A.b)
      {
        r.off_c70a = i;
        break;
      }
    }
    return r;
  }
};

const IProtocolScanner& DefaultProtocolScanner()
{
  static ProtocolScannerCoalesced instance;
  return instance;
}

}  // namespace arkan::relay::application::services
