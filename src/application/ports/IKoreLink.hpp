#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace arkan::relay::application::ports
{

struct ReconnectPolicy
{
  std::chrono::milliseconds initial{500};
  std::chrono::milliseconds max{30000};
  double backoff{2.0};
  double jitter_p{0.2};
};

struct IKoreLink
{
  virtual ~IKoreLink() = default;

  virtual void connect(const std::string& host, uint16_t port) = 0;

  virtual void close() = 0;

  virtual void send_frame(char kind, std::span<const std::byte> payload) = 0;

  virtual void on_frame(std::function<void(char, std::span<const std::byte>)> cb) = 0;

  virtual void set_candidate_ports(std::vector<uint16_t> /*ports*/) {}
  virtual void set_reconnect_policy(const ReconnectPolicy& /*p*/) {}
};

}  // namespace arkan::relay::application::ports
