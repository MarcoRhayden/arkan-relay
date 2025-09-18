#include "infrastructure/net/SendPipeline.hpp"

#include "domain/protocol/Opcodes.hpp"
#include "domain/protocol/ProtocolRules.hpp"

namespace arkan::relay::infrastructure::net
{
namespace app = arkan::relay::application::services;
namespace dom = arkan::relay::domain::protocol;
namespace op = arkan::relay::domain::protocol::op;

void SendPipeline::transform(std::vector<uint8_t>& data, app::ChecksumState& S) const
{
  std::span<const uint8_t> head(data.data(), data.size());
  dom::ProtocolRules::on_send_head(head, S);

  if (!S.found1c0b.load(std::memory_order_relaxed)) return;
  if (data.size() < 2) return;

  data.pop_back();

  bool is_1c0b = (data.size() >= 2 && data[0] == op::_1C_0B.a && data[1] == op::_1C_0B.b);

  std::vector<uint8_t> calc;
  if (is_1c0b)
  {
    calc = {op::_1C_0B.a, op::_1C_0B.b};
  }
  else
  {
    calc = data;
  }

  const uint32_t counter = static_cast<uint32_t>(S.counter.load(std::memory_order_relaxed));
  uint8_t appended = 0;
  if (counter == 0)
  {
    appended = svc_.seed(calc.data(), calc.size(), S);
  }
  else
  {
    appended =
        svc_.checksum(calc.data(), calc.size(), counter, S.low.load(std::memory_order_relaxed),
                      S.high.load(std::memory_order_relaxed), S);
  }

  if (is_1c0b)
  {
    data = calc;
    data.push_back(appended);
  }
  else
  {
    data.push_back(appended);
  }

  S.roll12();
}

}  // namespace arkan::relay::infrastructure::net
