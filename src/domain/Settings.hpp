#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arkan::relay::domain
{

struct Settings
{
  struct Reconnect
  {
    int initial_ms{500};
    int max_ms{30000};
    double backoff{2.0};
    double jitter_p{0.2};
  };

  struct Kore
  {
    std::string host{"127.0.0.1"};
    std::vector<uint16_t> ports{5293, 5294, 5295};
    Reconnect reconnect{};
  } kore;

  struct Relay
  {
    int ioThreads{2};
    std::size_t recvBuffer{65536};
    std::size_t sendBuffer{65536};
    std::size_t maxSessions{512};
    std::string framing{"none"};
  } relay;

  // Console / Logs
  bool showConsole{false};
  bool saveLog{true};
  bool saveSocketLog{true};
  std::string logsDir{"logs"};
  std::string appLogFilename{"relay_app.log"};
  std::string socketLogFilename{"relay_socket.log"};
  std::string configPath{"arkan-relay.toml"};

  std::optional<std::string> fnSeedAddr;
  std::optional<std::string> fnChecksumAddr;
  std::optional<std::string> fnSendAddr;
  std::optional<std::string> fnRecvAddr;
};

}  // namespace arkan::relay::domain
