#include "application/services/BridgeService.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <vector>

using arkan::relay::application::ports::LogLevel;
using Bytes = arkan::relay::application::ports::IHook::Bytes;

namespace arkan::relay::application::services
{

// Helper: join candidate ports for logging
static std::string join_ports(const std::vector<uint16_t>& v)
{
  std::ostringstream oss;
  for (size_t i = 0; i < v.size(); ++i)
  {
    if (i) oss << ',';
    oss << v[i];
  }
  return oss.str();
}

void BridgeService::start()
{
  if (running_) return;

  // ---- Hook - Kore ----------------------------------------------------------
  hook_.on_send = [this](Bytes b)
  {
    log_.sock(LogLevel::info, "SEND \xE2\x86\x92 " + shared::hex::hex_dump(b));

    // link_.send_frame('S', b);
  };

  hook_.on_recv = [this](Bytes b)
  {
    // console summary
    log_.sock(LogLevel::info, "RECV \xE2\x86\x90 " + shared::hex::hex_dump(b));
    // forward as 'R' frame to Kore
    link_.send_frame('R', b);
  };

  // ---- Kore - client ----------------------------------------------
  // -------- Kore → client ---------------------------------------------------
  link_.on_frame(
      [this](char kind, std::span<const std::byte> payload)
      {
        auto hex_len = std::to_string(payload.size());

        switch (kind)
        {
          case 'S':
          {
            const bool ok = hook_.try_inject_send(payload);
            if (ok)
              log_.sock(LogLevel::debug, "Kore→client inject S (" + hex_len + " bytes) ok");
            else
              log_.sock(LogLevel::warn,
                        "Kore→client inject S (" + hex_len + " bytes) failed (no socket yet?)");
            break;
          }
          case 'R':
          {
            const bool ok = hook_.try_inject_recv(payload);
            if (ok)
              log_.sock(LogLevel::debug, "Kore→client inject R (" + hex_len + " bytes) ok");
            else
              log_.sock(LogLevel::warn,
                        "Kore→client inject R (" + hex_len + " bytes) failed (no socket yet?)");
            break;
          }
          case 'K':
            log_.sock(LogLevel::trace, "Kore keepalive");
            break;

          default:
            log_.sock(LogLevel::warn, std::string("Unknown frame kind: ") + kind);
            break;
        }
      });

  // ---- Install hook before connecting --------------------------------------
  if (!hook_.install())
  {
    log_.app(LogLevel::err, "Hook installation failed");
    return;
  }

  // ---- Read-only config from Settings --------------
  const std::string host = cfg_.kore.host;
  const auto& ports = cfg_.kore.ports;

  if (host.empty() || ports.empty())
  {
    log_.app(LogLevel::err, "Invalid Kore configuration (host/ports); aborting.");
    stop();
    return;
  }

  log_.app(LogLevel::info, "Kore candidates at " + host + " ports [" + join_ports(ports) + "]");

  // Reconnection policy
  ports::ReconnectPolicy pol;
  pol.initial = std::chrono::milliseconds(cfg_.kore.reconnect.initial_ms);
  pol.max = std::chrono::milliseconds(cfg_.kore.reconnect.max_ms);
  pol.backoff = cfg_.kore.reconnect.backoff;
  pol.jitter_p = cfg_.kore.reconnect.jitter_p;
  link_.set_reconnect_policy(pol);

  // Provide full candidate list (round-robin + backoff handled by link)
  link_.set_candidate_ports(ports);

  link_.connect(host, ports.front());

  running_ = true;
  log_.app(LogLevel::info, "BridgeService started.");
}

void BridgeService::stop()
{
  if (!running_) return;

  hook_.uninstall();
  link_.close();

  running_ = false;
  log_.app(LogLevel::info, "BridgeService stopped");
}

}  // namespace arkan::relay::application::services
