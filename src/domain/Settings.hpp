#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arkan::relay::domain {

  struct Settings {
    std::vector<uint16_t> ports{6900, 6901, 6902};
    bool showConsole{false};
    bool saveLog{true};
    bool saveSocketLog{true};

    std::optional<std::string> fnSeedAddr;
    std::optional<std::string> fnChecksumAddr;
    std::optional<std::string> fnSendAddr;
    std::optional<std::string> fnRecvAddr;

    std::string logsDir{"logs"};
    std::string appLogFilename{"relay_app.log"};
    std::string socketLogFilename{"relay_socket.log"};
    std::string configPath{"arkan-relay.toml"};

    struct Relay {
      int         ioThreads{2};
      std::size_t recvBuffer{65536};
      std::size_t sendBuffer{65536};
      std::size_t maxSessions{512};
      std::string framing{"none"}; // "none" | "rsk" (for future)
    } relay;

    struct Kore1 {
      std::string host{"127.0.0.1"};
      uint16_t    port{2350}; // example local Kore1 port
    } kore1;
  };

} // namespace arkan::relay::domain
