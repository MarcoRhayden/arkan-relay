#include "infrastructure/link/KoreLink_Asio.hpp"

#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using arkan::relay::infrastructure::link::KoreLink_Asio;

namespace arkan::relay::infrastructure::link
{

// -------------------- tiny debug helper --------------------
void KoreLink_Asio::dbg(const char* s)
{
#ifdef _WIN32
  ::OutputDebugStringA(s);
#else
  (void)s;
#endif
}

void KoreLink_Asio::log_connected(uint16_t port) const
{
  char b[192];
  std::snprintf(b, sizeof(b), "[KoreLink] connected to %s:%u (attempt=%u)\n", host_.c_str(),
                (unsigned)port, attempt_);
  log_.sock(arkan::relay::application::ports::LogLevel::info, b);
}

void KoreLink_Asio::log_switch_port(uint16_t oldp, uint16_t newp) const
{
  char b[192];
  std::snprintf(b, sizeof(b), "[KoreLink] reconnect: switching port %u -> %u\n", (unsigned)oldp,
                (unsigned)newp);
  dbg(b);
}

void KoreLink_Asio::log_reconnect_in(long long ms) const
{
  char b[192];
  std::snprintf(b, sizeof(b), "[KoreLink] reconnect in %lld ms (attempt=%u)\n", ms, attempt_ + 1);
  dbg(b);
}

// -------------------- xorshift32 (no <random>) --------------------
uint32_t KoreLink_Asio::xorshift32_(uint32_t& s)
{
  // state must be non-zero
  if (s == 0) s = 0xA5A5A5A5u;
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

double KoreLink_Asio::unit_01_(uint32_t& s)
{
  // 24-bit mantissa-ish for [0,1)
  const uint32_t r = xorshift32_(s) & 0xFFFFFFu;
  return static_cast<double>(r) / static_cast<double>(0x1000000u);
}

// -------------------- ctor/dtor --------------------
KoreLink_Asio::KoreLink_Asio(application::ports::ILogger& log)
    : work_(std::in_place, boost::asio::make_work_guard(io_)),
      strand_{io_.get_executor()},
      resolver_(strand_),
      socket_(strand_),
      ping_timer_(strand_),
      reconn_timer_(strand_),
      log_(log)
{
  io_thread_ = std::thread([this] { io_.run(); });
}

KoreLink_Asio::~KoreLink_Asio()
{
  close();
  if (io_thread_.joinable()) io_thread_.join();
}

// -------------------- config setters --------------------
void KoreLink_Asio::set_reconnect_policy(const arkan::relay::application::ports::ReconnectPolicy& p)
{
  boost::asio::post(strand_,
                    [this, p]
                    {
                      policy_ = p;
                      cur_delay_ = policy_.initial;
                      attempt_ = 0;
                    });
}

void KoreLink_Asio::set_candidate_ports(std::vector<uint16_t> ports)
{
  boost::asio::post(
      strand_,
      [this, ports = std::move(ports)]() mutable
      {
        candidate_ports_ = std::move(ports);
        candidate_ports_.erase(std::remove(candidate_ports_.begin(), candidate_ports_.end(), 0),
                               candidate_ports_.end());
        std::sort(candidate_ports_.begin(), candidate_ports_.end());
        candidate_ports_.erase(std::unique(candidate_ports_.begin(), candidate_ports_.end()),
                               candidate_ports_.end());
        port_idx_ = 0;
      });
}

// -------------------- public API --------------------
void KoreLink_Asio::connect(const std::string& host, uint16_t port)
{
  boost::asio::post(strand_,
                    [this, host, port]
                    {
                      host_ = host;
                      single_port_ = port;
                      closing_ = false;
                      io_.restart();
                      if (!work_) work_.emplace(boost::asio::make_work_guard(io_));

                      cur_delay_ = policy_.initial;
                      attempt_ = 0;

                      start_connect();
                    });
}

void KoreLink_Asio::close()
{
  boost::asio::post(strand_,
                    [this]
                    {
                      closing_ = true;
                      connected_ = false;

                      ping_timer_.cancel();
                      reconn_timer_.cancel();
                      resolver_.cancel();

                      boost::system::error_code ec;
                      if (socket_.is_open()) socket_.close(ec);

                      if (work_) work_->reset();
                      io_.stop();
                    });
}

// -------------------- helpers --------------------
uint16_t KoreLink_Asio::current_port_nolock() const
{
  if (!candidate_ports_.empty())
  {
    return candidate_ports_[port_idx_ % candidate_ports_.size()];
  }
  return single_port_;
}

// -------------------- connect/read/write --------------------
void KoreLink_Asio::start_connect()
{
  if (closing_) return;

  const auto port = current_port_nolock();

  resolver_.async_resolve(
      host_, std::to_string(port),
      [this, port](const boost::system::error_code& ec, tcp::resolver::results_type results)
      {
        if (ec)
        {
          schedule_reconnect();
          return;
        }

        boost::asio::async_connect(
            socket_, results,
            [this, port](const boost::system::error_code& ec2, const tcp::endpoint&)
            {
              if (ec2)
              {
                schedule_reconnect();
                return;
              }

              connected_ = true;
              log_connected(port);

              // reset attempts/backoff on success
              attempt_ = 0;
              cur_delay_ = policy_.initial;

              do_read_header();
              schedule_ping();
              flush_sendq();
            });
      });
}

void KoreLink_Asio::schedule_reconnect()
{
  connected_ = false;
  if (closing_) return;

  // close socket
  boost::system::error_code ec;
  if (socket_.is_open()) socket_.close(ec);

  // round-robin - advance to the next candidate port (if any), and log it
  if (!candidate_ports_.empty())
  {
    const uint16_t oldp = candidate_ports_[port_idx_ % candidate_ports_.size()];
    port_idx_ = (port_idx_ + 1) % candidate_ports_.size();
    const uint16_t newp = candidate_ports_[port_idx_];
    if (newp != oldp) log_switch_port(oldp, newp);
  }

  // backoff exponencial + jitter
  auto next = cur_delay_;
  if (attempt_ == 0)
  {
    next = policy_.initial;
  }
  else
  {
    const long long scaled_ll =
        static_cast<long long>(std::llround(cur_delay_.count() * policy_.backoff));
    const long long clamped = std::min(scaled_ll, policy_.max.count());
    next = std::chrono::milliseconds(clamped);
  }

  // jitter (±jitter_p) com xorshift32
  if (policy_.jitter_p > 0.0)
  {
    static thread_local uint32_t s = 0xD00DFEEDu;
    const double u = unit_01_(s);  // [0,1)
    const double low = 1.0 - policy_.jitter_p;
    const double high = 1.0 + policy_.jitter_p;
    const double factor = low + (high - low) * u;

    const long long jittered = static_cast<long long>(factor * static_cast<double>(next.count()));
    next = std::chrono::milliseconds(jittered);
  }

  cur_delay_ = next;
  log_reconnect_in(static_cast<long long>(cur_delay_.count()));
  ++attempt_;

  reconn_timer_.expires_after(cur_delay_);
  reconn_timer_.async_wait(
      [this](const boost::system::error_code& e)
      {
        if (!e && !closing_) start_connect();
      });
}

void KoreLink_Asio::schedule_ping()
{
  if (closing_ || !connected_) return;

  ping_timer_.expires_after(std::chrono::milliseconds(5000));
  ping_timer_.async_wait(
      [this](const boost::system::error_code& ec)
      {
        if (ec || closing_ || !connected_) return;

        const auto hdr = make_header('K', 0);
        std::vector<std::byte> buf;
        buf.reserve(3);
        buf.insert(buf.end(), hdr.begin(), hdr.end());
        send_q_.push_back(std::move(buf));

        flush_sendq();
        schedule_ping();
      });
}

void KoreLink_Asio::send_frame(char kind, std::span<const std::byte> payload)
{
  std::vector<std::byte> buf;

  if (payload.size() > std::numeric_limits<uint16_t>::max())
  {
    log_.sock(arkan::relay::application::ports::LogLevel::err,
              "[KoreLink] send_frame: payload too large, rejecting (max 65535)");
    return;
  }

  const auto h = make_header(kind, payload.size());

  log_.sock(arkan::relay::application::ports::LogLevel::info,
            std::string("[KoreLink] enqueue kind=") + std::string(1, kind) +
                " len=" + std::to_string(payload.size()));

  if (kind == 'S')
  {
    log_.sock(
        arkan::relay::application::ports::LogLevel::warn,
        std::string("[KoreLink] dropping 'S' frame (forwarding SEND to Kore is disabled). len=") +
            std::to_string(payload.size()));
    return;
  }

  buf.reserve(3 + payload.size());
  buf.insert(buf.end(), h.begin(), h.end());
  buf.insert(buf.end(), payload.begin(), payload.end());

  boost::asio::post(strand_,
                    [this, msg = std::move(buf)]() mutable
                    {
                      send_q_.push_back(std::move(msg));
                      flush_sendq();
                    });
}

void KoreLink_Asio::flush_sendq()
{
  if (closing_ || !connected_ || sending_ || send_q_.empty()) return;

  sending_ = true;
  auto msg = std::move(send_q_.front());
  send_q_.pop_front();

  log_.sock(arkan::relay::application::ports::LogLevel::debug,
            "[KoreLink] flush_sendq -> writing len=" + std::to_string(msg.size()));

  auto owned = std::make_shared<std::vector<std::byte>>(std::move(msg));

  boost::asio::async_write(
      socket_, boost::asio::buffer(*owned),
      [this, owned](const boost::system::error_code& ec, std::size_t bytes_transferred) mutable
      {
        try
        {
          sending_ = false;
          if (ec)
          {
            log_.sock(arkan::relay::application::ports::LogLevel::err,
                      "[KoreLink] async_write error: " + ec.message() +
                          " queued_len=" + std::to_string(owned->size()));
            schedule_reconnect();
            return;
          }

          log_.sock(arkan::relay::application::ports::LogLevel::debug,
                    "[KoreLink] async_write wrote " + std::to_string(bytes_transferred) +
                        " bytes (queued_len=" + std::to_string(owned->size()) + ")");

          if (!send_q_.empty()) flush_sendq();
        }
        catch (const std::exception& ex)
        {
          log_.sock(arkan::relay::application::ports::LogLevel::err,
                    std::string("[KoreLink] async_write handler exception: ") + ex.what());
          schedule_reconnect();
        }
      });
}

void KoreLink_Asio::do_read_header()
{
  if (closing_) return;
  auto buf = boost::asio::buffer(hdr_.data(), hdr_.size());
  boost::asio::async_read(socket_, buf,
                          [this](const boost::system::error_code& ec, std::size_t)
                          {
                            if (ec)
                            {
                              schedule_reconnect();
                              return;
                            }
                            const uint16_t len = le16(hdr_[1], hdr_[2]);
                            log_.sock(
                                arkan::relay::application::ports::LogLevel::info,
                                "[KoreLink] read header kind=" +
                                    std::to_string((int)std::to_integer<unsigned char>(hdr_[0])) +
                                    " len=" + std::to_string(len));

                            do_read_body(len);
                          });
}

void KoreLink_Asio::do_read_body(std::size_t body_len)
{
  if (closing_) return;

  // special-case zero-length body: handle immediately (no async_read with 0 bytes)
  if (body_len == 0)
  {
    if (on_frame_)
    {
      const char kind = static_cast<char>(hdr_[0]);
      on_frame_(kind, std::span<const std::byte>());  // empty span
    }
    // continue reading next header
    do_read_header();
    return;
  }

  body_.assign(body_len, std::byte{0});
  boost::asio::async_read(
      socket_, boost::asio::buffer(body_.data(), body_.size()),
      [this, body_len](const boost::system::error_code& ec, std::size_t)
      {
        try
        {
          if (ec)
          {
            schedule_reconnect();
            return;
          }
          if (on_frame_)
          {
            const char kind = static_cast<char>(hdr_[0]);
            on_frame_(kind, std::span<const std::byte>(body_.data(), body_len));
          }
          do_read_header();
        }
        catch (const std::exception& ex)
        {
          // ensure exceptions don't escape the asio handler
          log_.sock(arkan::relay::application::ports::LogLevel::err,
                    std::string("[KoreLink] do_read_body handler exception: ") + ex.what());
          schedule_reconnect();
        }
      });
}

// -------------------- framing helpers --------------------
std::array<std::byte, 3> KoreLink_Asio::make_header(char kind, std::size_t len)
{
  const uint16_t L = static_cast<uint16_t>(len);
  return {std::byte{static_cast<unsigned char>(kind)},
          std::byte{static_cast<unsigned char>(L & 0xFF)},
          std::byte{static_cast<unsigned char>((L >> 8) & 0xFF)}};
}

uint16_t KoreLink_Asio::le16(std::byte lo, std::byte hi)
{
  return static_cast<uint16_t>(static_cast<unsigned>(lo) | (static_cast<unsigned>(hi) << 8));
}

}  // namespace arkan::relay::infrastructure::link
