#pragma once
#include <functional>
#include <span>
#include <cstddef>

namespace arkan::relay::application::ports {

  // Windows hook port. BridgeService
  // Implementation (Hook_Win32) calls these callbacks when intercepting send/recv.
  struct IHook {
    using Bytes = std::span<const std::byte>;

    virtual ~IHook() = default;

    // Lifecycle
    virtual bool install() = 0;
    virtual void uninstall() = 0;

    // Callbacks (filled by BridgeService)
    std::function<void(Bytes)> on_send;
    std::function<void(Bytes)> on_recv;
  };

} // namespace arkan::relay::application::ports
