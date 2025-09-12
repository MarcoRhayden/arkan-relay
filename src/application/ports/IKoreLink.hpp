#pragma once
#include <functional>
#include <span>
#include <vector>
#include <cstddef>
#include <string>

namespace arkan::relay::application::ports {

struct IKoreLink {
  using FrameHandler = std::function<void(char kind, std::span<const std::byte> payload)>;

  virtual ~IKoreLink() = default;
  virtual void connect(const std::string& host, uint16_t port) = 0;
  virtual void close() = 0;

  // 'R'/'S'/'K'
  virtual void send_frame(char kind, std::span<const std::byte> payload) = 0;
  virtual void on_frame(FrameHandler h) = 0;
};

} // namespace arkan::relay::application::ports
