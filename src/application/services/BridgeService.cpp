#include "application/services/BridgeService.hpp"
#include <string>

using arkan::relay::application::ports::LogLevel;
using Bytes = arkan::relay::application::ports::IHook::Bytes;

namespace arkan::relay::application::services {

    void BridgeService::start() {
        if (running_) return;

        // Wire hook → link
        hook_.on_send([this](const Bytes& b) {
            // 'S' frame: client sent data; forward to Kore
            link_.send_frame('S', std::span<const std::byte>(b.data(), b.size()));
        });

        hook_.on_recv([this](const Bytes& b) {
            // 'R' frame: client received data; forward to Kore
            link_.send_frame('R', std::span<const std::byte>(b.data(), b.size()));
        });

        // Wire link → (TODO) client (original send/recv functions)
        link_.on_frame([this](char kind, std::span<const std::byte> payload) {
            switch (kind) {
            case 'S':
                // TODO: call original send to inject outbound path if needed
                log_.app(LogLevel::debug, "Kore→client 'S' frame (" + std::to_string(payload.size()) + " bytes)");
                break;
            case 'R':
                // TODO: inject into client recv path
                log_.app(LogLevel::debug, "Kore→client 'R' frame (" + std::to_string(payload.size()) + " bytes)");
                break;
            case 'K':
                // keepalive from Kore
                log_.app(LogLevel::trace, "Kore keepalive");
                break;
            default:
                log_.app(LogLevel::warn, std::string("Unknown frame kind: ") + kind);
            }
        });

        // Connect to Kore1
        link_.connect(cfg_.kore1.host, cfg_.kore1.port);

        // Install hooks last (so callbacks are ready)
        if (!hook_.install()) {
            log_.app(LogLevel::err, "Hook installation failed");
        }

        running_ = true;
        log_.app(LogLevel::info, "BridgeService started");
    }

    void BridgeService::stop() {
        if (!running_) return;

        hook_.uninstall();
        link_.close();
        
        running_ = false;
        log_.app(LogLevel::info, "BridgeService stopped");
    }

} // namespace arkan::relay::application::services
