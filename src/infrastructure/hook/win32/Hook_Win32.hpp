#pragma once

#include <winsock2.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <memory>
#include <mutex>
#include <span>
#include <sstream>
#include <vector>

#include "application/ports/IHook.hpp"
#include "application/ports/ILogger.hpp"
#include "domain/Settings.hpp"
#include "shared/hex/Hex.hpp"

namespace arkan::relay::infrastructure::hook
{

namespace ports = ::arkan::relay::application::ports;
namespace domain = ::arkan::relay::domain;

// -----------------------------------------------------------------------------
// payload queued for injection: bytes + whether to append checksum
// -----------------------------------------------------------------------------
struct InjectMsg
{
  std::vector<uint8_t> data;
  bool needs_checksum = true;

  // retry/backoff helpers
  uint8_t attempts = 0;
  std::chrono::steady_clock::time_point next_try = std::chrono::steady_clock::time_point::min();
};

// -----------------------------------------------------------------------------
// Hook_Win32
// -----------------------------------------------------------------------------
class Hook_Win32 : public ports::IHook
{
 public:
  Hook_Win32(ports::ILogger& log, const domain::Settings& settings);
  ~Hook_Win32() override;  // calls uninstall()

  bool install() override;    // patch SEND/RECV slots + start watchdog
  void uninstall() override;  // restore originals + stop watchdog

  // IHook — injection + socket tracking
  bool try_inject_send(Bytes b) override;
  bool try_inject_send(Bytes b, bool needs_checksum);
  bool try_inject_recv(Bytes b) override;
  void notify_socket(SOCKET s) override;
  void emit_send(Bytes) override;
  void emit_recv(Bytes) override;

  // Non-copyable
  Hook_Win32(const Hook_Win32&) = delete;
  Hook_Win32& operator=(const Hook_Win32&) = delete;
  Hook_Win32(Hook_Win32&&) = delete;
  Hook_Win32& operator=(Hook_Win32&&) = delete;

 private:
  struct Impl;
  std::unique_ptr<Impl> p_;

  // constants for requeue/backoff
  static constexpr unsigned MAX_INJECT_ATTEMPTS = 5u;
  static constexpr std::chrono::milliseconds BACKOFF_BASE_MS{200};  // base backoff (ms) * attempts

  // injection queues (Kore -> client)
  static constexpr size_t kDrainBatchMax = 64;

  // Logging + config
  ports::ILogger& log_;
  const domain::Settings& cfg_;

  // ---- Injection queue (send) ----------------------------------------------
  std::mutex inj_send_mtx_;
  std::deque<InjectMsg> inj_send_q_;

  // constants
  static constexpr uint8_t DEFAULT_CHECKSUM_BYTE = 0x69u;
  static constexpr size_t HEX_DUMP_LIMIT = 64;
  // spacing between injected sends
  static constexpr std::chrono::milliseconds INJECT_SPACING_MS{33};

  // Drain pending injected sends using Trampolines::send
  void drain_injected_sends_();

  // internal helper that allows specifying checksum behavior
  // not part of interface; used internally by wrapper
  bool try_inject_send_internal(Bytes b, bool needs_checksum);

  // helper: requeue message with backoff (thread-safe)
  void requeue_with_backoff(InjectMsg item, const char* reason);
};

}  // namespace arkan::relay::infrastructure::hook
