#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <cstddef>

#include "infrastructure/link/KoreLink_Asio.hpp"
#include "application/ports/ILogger.hpp"
#include "domain/Settings.hpp"

using tcp = boost::asio::ip::tcp;

// ----------------------------- Helpers -----------------------------
static inline void put_u16_le(std::byte* dst, uint16_t v) {
  dst[0] = static_cast<std::byte>(v & 0xFF);
  dst[1] = static_cast<std::byte>((v >> 8) & 0xFF);
}

static inline uint16_t get_u16_le(const std::byte* p) {
  return static_cast<uint16_t>(std::to_integer<unsigned char>(p[0]))
       | (static_cast<uint16_t>(std::to_integer<unsigned char>(p[1])) << 8);
}

static std::vector<std::byte> bytes_from(const std::string& s) {
  return std::vector<std::byte>(reinterpret_cast<const std::byte*>(s.data()),
                                reinterpret_cast<const std::byte*>(s.data()) + s.size());
}

// ----------------------------- Test Logger -----------------------------
struct TestLogger : arkan::relay::application::ports::ILogger {
  void init(const arkan::relay::domain::Settings&) override {}
  void app(arkan::relay::application::ports::LogLevel, const std::string&) override {}
  void sock(arkan::relay::application::ports::LogLevel, const std::string&) override {}
};

// ----------------------------- Fake Kore1 server -----------------------------
class FakeKoreServer {
public:
  FakeKoreServer()
  : io_(), acceptor_(io_), socket_(io_) {}

  // start on loopback:0 (ephemeral), returns bound port
  uint16_t start() {
    tcp::endpoint ep{boost::asio::ip::make_address("127.0.0.1"), 0};
    acceptor_.open(ep.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(ep);
    acceptor_.listen();

    port_ = acceptor_.local_endpoint().port();
    th_accept_ = std::thread([this]{ acceptor_.accept(socket_); read_loop_(); });
    return port_;
  }

  void stop() {
    try { socket_.close(); } catch(...) {}
    try { acceptor_.close(); } catch(...) {}
    if (th_accept_.joinable()) th_accept_.join();
  }

  // send a frame to the connected client
  void send_frame(char kind, std::span<const std::byte> payload) {
    std::vector<std::byte> buf;
    const uint16_t n = static_cast<uint16_t>(payload.size());
    buf.resize(1 + 2 + n);
    buf[0] = static_cast<std::byte>(kind);
    put_u16_le(buf.data() + 1, n);
    if (n) std::memcpy(buf.data() + 3, payload.data(), n);
    boost::asio::write(socket_, boost::asio::buffer(buf));
  }

  // wait for a frame received from client
  bool wait_pop(char& kind, std::vector<std::byte>& payload, std::chrono::milliseconds to=std::chrono::milliseconds(1000)) {
    std::unique_lock<std::mutex> lk(m_);
    if(!cv_.wait_for(lk, to, [&]{ return !q_.empty(); })) return false;
    auto v = std::move(q_.front()); q_.pop();
    kind = v.first; payload = std::move(v.second);
    return true;
  }

private:
  void read_loop_() {
    try {
      for (;;) {
        std::array<std::byte,3> hdr{};
        boost::asio::read(socket_, boost::asio::buffer(hdr.data(), hdr.size()));
        char k = static_cast<char>(std::to_integer<unsigned char>(hdr[0]));
        uint16_t n = get_u16_le(hdr.data()+1);
        std::vector<std::byte> body(n);
        if (n) boost::asio::read(socket_, boost::asio::buffer(body.data(), body.size()));
        {
          std::lock_guard<std::mutex> lk(m_);
          q_.emplace(k, std::move(body));
        }
        cv_.notify_one();
      }
    } catch (...) {
      // socket closed
    }
  }

  boost::asio::io_context io_;
  tcp::acceptor acceptor_;
  tcp::socket   socket_;
  uint16_t      port_{0};
  std::thread   th_accept_;

  std::mutex m_;
  std::condition_variable cv_;
  std::queue<std::pair<char, std::vector<std::byte>>> q_;
};

// ----------------------------- Tests -----------------------------

TEST(KoreLinkAsio, SendsFramesToServer) {
  FakeKoreServer server;
  const uint16_t port = server.start();

  arkan::relay::domain::Settings s;
  s.kore1.host = "127.0.0.1";
  s.kore1.port = port;
  s.relay.ioThreads = 1;

  TestLogger logger;
  arkan::relay::infrastructure::link::KoreLink_Asio link(logger, s);
  link.connect(s.kore1.host, s.kore1.port);

  auto payload = bytes_from("hello");
  link.send_frame('S', payload);

  char kind; std::vector<std::byte> got;
  ASSERT_TRUE(server.wait_pop(kind, got));
  EXPECT_EQ(kind, 'S');
  EXPECT_EQ(got.size(), payload.size());
  EXPECT_TRUE(std::equal(got.begin(), got.end(), payload.begin(), payload.end()));

  link.close();
  server.stop();
}

TEST(KoreLinkAsio, ReceivesFramesFromServer) {
  FakeKoreServer server;
  const uint16_t port = server.start();

  arkan::relay::domain::Settings s;
  s.kore1.host = "127.0.0.1";
  s.kore1.port = port;
  s.relay.ioThreads = 1;

  TestLogger logger;
  arkan::relay::infrastructure::link::KoreLink_Asio link(logger, s);

  std::mutex m; std::condition_variable cv; bool got=false; char kind='?';
  std::vector<std::byte> data;

  link.on_frame([&](char k, std::span<const std::byte> p){
    std::lock_guard<std::mutex> lk(m);
    kind = k;
    data.assign(p.begin(), p.end());
    got = true;
    cv.notify_one();
  });

  link.connect(s.kore1.host, s.kore1.port);

  auto payload = bytes_from("from-server");
  server.send_frame('R', payload);

  {
    std::unique_lock<std::mutex> lk(m);
    ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(1000), [&]{return got;}));
  }

  EXPECT_EQ(kind, 'R');
  EXPECT_EQ(data.size(), payload.size());
  EXPECT_TRUE(std::equal(data.begin(), data.end(), payload.begin(), payload.end()));

  link.close();
  server.stop();
}
