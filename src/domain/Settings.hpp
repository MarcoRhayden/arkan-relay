#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arkan::relay::domain {

    struct Settings {
        // relay ports (XKORE_PORTS)
        std::vector<uint16_t> ports{6900, 6901, 6902};

        // Flags
        bool showConsole{false};
        bool saveLog{true};
        bool saveSocketLog{true};

        // Addresses
        std::string fnSeedAddr; // ex: "0x12345678"
        std::string fnChecksumAddr; // ex: "0x9ABCDEF0"
        std::string fnSendAddr; // ex: "0x...."
        std::string fnRecvAddr; // ex: "0x...."

        // Logging
        std::string logsDir;
        std::string appLogFilename;
        std::string socketLogFilename;

        // Configuration file name (TOML)
        std::string configPath{"arkan-relay.toml"};
    };

} // namespace arkan::relay::domain