#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <boost/asio.hpp>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "application/ports/IKoreLink.hpp"
#include "application/ports/ILogger.hpp"

namespace arkan::relay::infrastructure::link
{

class KoreLink_Asio final : public arkan::relay::application::ports::IKoreLink
{
 public:
  using tcp = boost::asio::ip::tcp;

  KoreLink_Asio(arkan::relay::application::ports::ILogger& log);
  ~KoreLink_Asio() override;

  // IKoreLink
  void connect(const std::string& host, uint16_t port) override;
  void close() override;
  void send_frame(char kind, std::span<const std::byte> payload) override;
  void on_frame(std::function<void(char, std::span<const std::byte>)> cb) override
  {
    on_frame_ = std::move(cb);
  }

  void set_candidate_ports(std::vector<uint16_t> ports) override;
  void set_reconnect_policy(const arkan::relay::application::ports::ReconnectPolicy& p) override;

 private:
  // life cycle
  void start_connect();
  void schedule_reconnect();
  void do_read_header();
  void do_read_body(std::size_t body_len);
  void flush_sendq();
  void schedule_ping();

  // helpers
  static std::array<std::byte, 3> make_header(char kind, std::size_t len);
  static uint16_t le16(std::byte lo, std::byte hi);
  uint16_t current_port_nolock() const;

  // Log
  application::ports::ILogger& log_;

  // telemetry helpers (no-throw, no-alloc)
  static void dbg(const char* s);
  void log_connected(uint16_t port) const;
  void log_switch_port(uint16_t oldp, uint16_t newp) const;
  void log_reconnect_in(long long ms) const;

  // simple xorshift32 RNG for jitter (avoid <random>)
  static uint32_t xorshift32_(uint32_t& s);
  static double unit_01_(uint32_t& s);

 private:
  // asio
  boost::asio::io_context io_;
  boost::asio::strand<boost::asio::io_context::executor_type> strand_{io_.get_executor()};
  std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_;
  std::thread io_thread_;
  tcp::resolver resolver_{strand_};
  tcp::socket socket_{strand_};
  boost::asio::steady_timer ping_timer_{strand_};
  boost::asio::steady_timer reconn_timer_{strand_};

  // config/state
  std::string host_;
  uint16_t single_port_{0};
  std::atomic<bool> closing_{false};
  std::atomic<bool> connected_{false};

  // reconnection policy
  arkan::relay::application::ports::ReconnectPolicy policy_{};
  std::chrono::milliseconds cur_delay_{policy_.initial};
  unsigned attempt_{0};

  // round-robin
  std::vector<uint16_t> candidate_ports_;
  std::size_t port_idx_{0};

  // framing
  std::array<std::byte, 3> hdr_{};
  std::vector<std::byte> body_;

  // send queue (single-threaded by strand_)
  std::deque<std::vector<std::byte>> send_q_;
  bool sending_{false};

  // callback
  std::function<void(char, std::span<const std::byte>)> on_frame_;
};

}  // namespace arkan::relay::infrastructure::link
