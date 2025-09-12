#include "infrastructure/logging/Logger_Spdlog.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <boost/filesystem.hpp>
#include <system_error>
#include <vector>

namespace fs = boost::filesystem;
using arkan::relay::application::ports::LogLevel;

namespace {
    spdlog::level::level_enum map_level(LogLevel l) {
      switch (l) {
        case LogLevel::trace:    return spdlog::level::trace;
        case LogLevel::debug:    return spdlog::level::debug;
        case LogLevel::info:     return spdlog::level::info;
        case LogLevel::warn:     return spdlog::level::warn;
        case LogLevel::err:      return spdlog::level::err;
        case LogLevel::critical: return spdlog::level::critical;
        case LogLevel::off:      return spdlog::level::off;
      }
      return spdlog::level::info;
    }
}

namespace arkan::relay::infrastructure::logging {

  void Logger_Spdlog::init(const arkan::relay::domain::Settings& s) {
    const fs::path dir      = s.logsDir.empty() ? fs::path{"logs"} : fs::path{s.logsDir};
    const fs::path appPath  = dir / (s.appLogFilename.empty()    ? "relay_app.log"    : s.appLogFilename);
    const fs::path sockPath = dir / (s.socketLogFilename.empty() ? "relay_socket.log" : s.socketLogFilename);

    boost::system::error_code ec;
    fs::create_directories(dir, ec);

    if (auto prev = spdlog::get("app"))    spdlog::drop(prev->name());
    if (auto prev = spdlog::get("socket")) spdlog::drop(prev->name());

    auto app_sink  = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(appPath.string(), 5 * 1024 * 1024, 3);
    auto sock_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(sockPath.string(), 5 * 1024 * 1024, 3);

    std::vector<spdlog::sink_ptr> app_sinks{app_sink};
    if (s.showConsole) app_sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    app_  = std::make_shared<spdlog::logger>("app", app_sinks.begin(), app_sinks.end());
    sock_ = std::make_shared<spdlog::logger>("socket", sock_sink);

    app_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    sock_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
  }

  void Logger_Spdlog::app(LogLevel level, const std::string& msg) {
    if (app_)  app_->log(map_level(level), msg);
  }

  void Logger_Spdlog::sock(LogLevel level, const std::string& msg) {
    if (sock_) sock_->log(map_level(level), msg);
  }

} // namespace arkan::relay::infrastructure::logging
