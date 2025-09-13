#include "infrastructure/link/KoreLink_Asio.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

using arkan::relay::application::ports::LogLevel;

namespace arkan::relay::infrastructure::link {
  using tcp = boost::asio::ip::tcp;

  /*
  * Framing helpers
  * Protocol: [ kind:1 byte ] [ length:2 bytes LE ] [ payload:length bytes ]
  * - kind: 'R', 'S', or 'K' (keep-alive)
  * - length: payload size in bytes (uint16, little-endian)
  */
  static inline void put_u16_le(std::byte* dst, uint16_t v) {
    dst[0] = static_cast<std::byte>(v & 0xFF);
    dst[1] = static_cast<std::byte>((v >> 8) & 0xFF);
  }

  static inline uint16_t get_u16_le(const std::byte* p) {
    return static_cast<uint16_t>(std::to_integer<unsigned char>(p[0]))
        | (static_cast<uint16_t>(std::to_integer<unsigned char>(p[1])) << 8);
  }

  /*
  * KoreLink_Asio
  * -------------
  * Purpose:
  *   - Maintain a client TCP connection to Kore1.
  *   - Send framed messages ('R'/'S'/'K') asynchronously.
  *   - Receive framed messages and dispatch them via a user-provided handler.
  */
  KoreLink_Asio::KoreLink_Asio(arkan::relay::application::ports::ILogger& logger,
                              const arkan::relay::domain::Settings& s)
    : log_(logger), cfg_(s) {}

  /*
  * connect(host, port)
  * -------------------
  * 1) Synchronously resolve and connect to Kore1 (blocks until connect finishes).
  *    - If you prefer fully non-blocking setup, switch to async_connect.
  * 2) Start the async read loop by reading the 3-byte header first.
  * 3) Spin up the io_context worker threads.
  */
  void KoreLink_Asio::connect(const std::string& host, uint16_t port) {
    if (running_) return;
    running_ = true;

    sock_ = std::make_unique<tcp::socket>(io_);

    // Resolve and connect (sync) — acceptable since this is one-time setup.
    auto resolver = std::make_unique<tcp::resolver>(io_);
    auto results = resolver->resolve(host, std::to_string(port));
    boost::asio::connect(*sock_, results);

    log_.app(LogLevel::info, "KoreLink connected to " + host + ":" + std::to_string(port));

    // Kick off the async read loop (header → body → header → ...)
    do_read_header_();

    // Run the io_context on N threads for concurrency with other components.
    run_io_(cfg_.relay.ioThreads);
  }

  /*
  * run_io_(threads)
  * ----------------
  * Launches `threads` background workers that call io_.run().
  * - If `threads` < 1, we force it to 1.
  * - Threads stop when io_.stop() is called and all work drains.
  */
  void KoreLink_Asio::run_io_(int threads) {
    if (threads < 1) threads = 1;

    workers_.reserve(static_cast<size_t>(threads));
    for (int i = 0; i < threads; ++i) {
      workers_.emplace_back([this]{ io_.run(); });
    }
  }

  /*
  * close()
  * -------
  * Gracefully shuts down the link:
  * - Marks running_ = false to prevent further operations.
  * - Closes the socket (ignoring errors).
  * - Stops the io_context and joins all worker threads.
  */
  void KoreLink_Asio::close() {
    if (!running_) return;
    running_ = false;

    boost::system::error_code ec;
    if (sock_) sock_->close(ec);

    io_.stop();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();

    log_.app(LogLevel::info, "KoreLink closed");
  }

  /*
  * send_frame(kind, payload)
  * -------------------------
  * Builds a frame [kind][lenLE][payload] into a temporary buffer
  * and schedules an async_write on the socket.
  */
  void KoreLink_Asio::send_frame(char kind, std::span<const std::byte> payload) {
    if (!sock_ || !sock_->is_open()) return;

    const uint16_t n = static_cast<uint16_t>(payload.size());
    auto buf = std::make_shared<std::vector<std::byte>>(1 + 2 + n);

    (*buf)[0] = static_cast<std::byte>(kind);
    put_u16_le(buf->data() + 1, n);
    if (n) std::memcpy(buf->data() + 3, payload.data(), n);

    boost::asio::async_write(*sock_, boost::asio::buffer(*buf),
      [this, buf](const boost::system::error_code& ec, std::size_t) {
        if (ec) log_.app(LogLevel::err, "KoreLink write error: " + ec.message());
      });
  }

  /*
  * do_read_header_()
  * -----------------
  * Reads exactly 3 bytes: [kind][lenLE].
  * On success, parses the length and continues to do_read_body_(kind, len).
  * On error, logs it and aborts the loop (caller may decide to reconnect).
  */
  void KoreLink_Asio::do_read_header_() {
    if (!running_) return;

    auto self = this;
    boost::asio::async_read(*sock_, boost::asio::buffer(header_.data(), header_.size()),
      [this, self](const boost::system::error_code& ec, std::size_t){
        if (ec) {
          log_.app(LogLevel::err, "KoreLink read hdr error: " + ec.message());
          return;
        }
        const char kind = static_cast<char>(std::to_integer<unsigned char>(header_[0]));
        const uint16_t len = get_u16_le(header_.data() + 1);
        do_read_body_(kind, len);
      });
  }

  /*
  * do_read_body_(kind, len)
  * ------------------------
  * Allocates a body buffer of size `len` and reads it fully (async).
  * On success:
  *   - Dispatches the frame to the user-provided handler(kind, payload).
  *   - Restarts the header read to continue the loop.
  * On error:
  *   - Logs the error and stops the loop (caller may close()/reconnect).
  */
  void KoreLink_Asio::do_read_body_(char kind, std::size_t len) {
    body_.assign(len, std::byte{0});

    auto self = this;
    boost::asio::async_read(*sock_, boost::asio::buffer(body_.data(), body_.size()),
      [this, self, kind](const boost::system::error_code& ec, std::size_t){
        if (ec) {
          log_.app(LogLevel::err, "KoreLink read body error: " + ec.message());
          return;
        }

        if (handler_) {
          handler_(kind, std::span<const std::byte>(body_.data(), body_.size()));
        }

        do_read_header_();
      });
  }

} // namespace arkan::relay::infrastructure::link
