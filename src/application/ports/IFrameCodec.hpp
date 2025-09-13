#pragma once
#include <vector>
#include <span>
#include <cstddef>

namespace arkan::relay::application::ports {

  struct IFrameCodec {
    virtual ~IFrameCodec() = default;

    virtual std::size_t feed(std::span<const std::byte>,
                            std::vector<std::vector<std::byte>>& /*out*/) { return 0; }

    virtual std::vector<std::byte> encode(std::span<const std::byte> payload) {
      return std::vector<std::byte>(payload.begin(), payload.end());
    }
  };

} // namespace arkan::relay::application::ports
