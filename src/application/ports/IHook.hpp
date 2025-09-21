#pragma once
#include <cstddef>
#include <functional>
#include <span>
#ifdef _WIN32
#include <winsock2.h>
#endif

namespace arkan::relay::application::ports
{

// Windows hook port. BridgeService wires these callbacks.
struct IHook
{
  using Bytes = std::span<const std::byte>;

  // Observability: raw client traffic (already transformed by trampolines)
  std::function<void(Bytes)> on_send;
  std::function<void(Bytes)> on_recv;

  virtual bool install() = 0;
  virtual void uninstall() = 0;

  // Injection points (Kore -> client). Return false if no active socket/session.
  virtual bool try_inject_send(Bytes bytes) = 0;
  virtual bool try_inject_recv(Bytes bytes) = 0;  // may reuse send-path for injection

  // Called by trampolines
  virtual void emit_send(Bytes) = 0;
  virtual void emit_recv(Bytes) = 0;
  virtual void notify_socket(SOCKET s) = 0;

  virtual ~IHook() = default;
};

}  // namespace arkan::relay::application::ports
