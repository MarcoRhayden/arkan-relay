#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "application/services/protocol/ChecksumState.hpp"
#include "application/services/protocol/ProtocolScanner.hpp"
#include "domain/protocol/Opcodes.hpp"

namespace arkan::relay::domain::protocol
{

struct ProtocolRules
{
  static void on_recv(const arkan::relay::application::services::ScanResult& s,
                      arkan::relay::application::services::ChecksumState& S)
  {
    if (s.head_c70a)
    {
      S.reset_all();
      return;
    }
    if (s.head_b300)
    {
      S.reset_all();
      return;
    }
    if (s.off_c70a != SIZE_MAX)
    {
      S.reset_all();
    }
  }

  static inline void on_send_head(std::span<const uint8_t> head,
                                  arkan::relay::application::services::ChecksumState& S) noexcept
  {
    using namespace arkan::relay::domain::protocol::op;

    if (head.size() >= 2)
    {
      const uint8_t a = head[0];
      const uint8_t b = head[1];

      if ((a == _26_0C.a && b == _26_0C.b) || (a == C7_0A.a && b == C7_0A.b))
      {
        S.counter.store(0, std::memory_order_relaxed);
        S.found1c0b.store(false, std::memory_order_relaxed);
        return;
      }

      if (a == _1C_0B.a && b == _1C_0B.b)
      {
        S.found1c0b.store(true, std::memory_order_relaxed);
        return;
      }
    }
  }
};

}  // namespace arkan::relay::domain::protocol
