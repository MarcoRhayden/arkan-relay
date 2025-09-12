#pragma once
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <boost/asio.hpp>
#include "application/ports/IKoreLink.hpp"
#include "application/ports/ILogger.hpp"
#include "domain/Settings.hpp"

namespace arkan::relay::infrastructure::link {

class KoreLink_Asio : public arkan::relay::application::ports::IKoreLink {
    public:
        KoreLink_Asio(arkan::relay::application::ports::ILogger& logger,
                        const arkan::relay::domain::Settings& s);

        void connect(const std::string& host, uint16_t port) override;
        void close() override;
        void send_frame(char kind, std::span<const std::byte> payload) override;
        void on_frame(FrameHandler h) override { handler_ = std::move(h); }

    private:
        using tcp = boost::asio::ip::tcp;

        void do_read_header_();
        void do_read_body_(char kind, std::size_t len);
        void run_io_(int threads);

        arkan::relay::application::ports::ILogger& log_;
        const arkan::relay::domain::Settings& cfg_;

        boost::asio::io_context io_;
        std::unique_ptr<tcp::socket> sock_;
        std::vector<std::thread> workers_;
        std::atomic<bool> running_{false};

        // rx temp
        std::array<std::byte, 3> header_{};
        std::vector<std::byte>   body_;

        FrameHandler handler_;
};

} // namespace arkan::relay::infrastructure::link
