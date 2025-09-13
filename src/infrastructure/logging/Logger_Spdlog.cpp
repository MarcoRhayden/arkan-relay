#include "infrastructure/logging/Logger_Spdlog.hpp"

#if defined(_WIN32)
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h> 
#include <spdlog/sinks/msvc_sink.h>          
#include <spdlog/async.h>                   
#include <spdlog/async_logger.h>

#include <boost/filesystem.hpp>
#include <system_error>
#include <vector>
#include <chrono>

namespace fs = boost::filesystem;
using arkan::relay::application::ports::LogLevel;

namespace arkan::relay::infrastructure::logging {

// -------------------------------------------------------------------------------------------------
// map_level
//  - Converts our app-level enum (ports::LogLevel) to spdlog's native level enum.
//  - Central place to keep mapping consistent across app(), sock(), and set_level().
// -------------------------------------------------------------------------------------------------
spdlog::level::level_enum Logger_Spdlog::map_level(LogLevel l) {
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

// -------------------------------------------------------------------------------------------------
// configure_console_colors_
//  - Applies per-level colors ONLY on the console sink.
//  - On Windows, uses wincolor sink with WinAPI attributes (WORD).
//  - On POSIX, uses ansi sink with ANSI escape sequences.
// -------------------------------------------------------------------------------------------------
#if defined(_WIN32)
static void configure_console_colors_(const std::shared_ptr<spdlog::sinks::wincolor_stdout_sink_mt>& sink) {
  const WORD GRAY     = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
  const WORD BGRAY    = GRAY | FOREGROUND_INTENSITY;
  const WORD GREEN    = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
  const WORD CYAN     = FOREGROUND_GREEN | FOREGROUND_BLUE  | FOREGROUND_INTENSITY;
  const WORD YELLOW   = FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
  const WORD RED      = FOREGROUND_RED   | FOREGROUND_INTENSITY;
  const WORD MAGENTA  = FOREGROUND_RED   | FOREGROUND_BLUE  | FOREGROUND_INTENSITY;

  sink->set_color(spdlog::level::trace,    BGRAY);
  sink->set_color(spdlog::level::debug,    CYAN);
  sink->set_color(spdlog::level::info,     GREEN);
  sink->set_color(spdlog::level::warn,     YELLOW);
  sink->set_color(spdlog::level::err,      RED);
  sink->set_color(spdlog::level::critical, MAGENTA);
}
#else
static void configure_console_colors_(const std::shared_ptr<spdlog::sinks::ansicolor_stdout_sink_mt>& sink) {
  const std::string BGRAY   = "\x1b[90m"; // bright black (gray)
  const std::string CYAN    = "\x1b[36m";
  const std::string GREEN   = "\x1b[32m";
  const std::string YELLOW  = "\x1b[33m";
  const std::string RED     = "\x1b[31m";
  const std::string MAGENTA = "\x1b[35m";

  sink->set_color(spdlog::level::trace,    BGRAY);
  sink->set_color(spdlog::level::debug,    CYAN);
  sink->set_color(spdlog::level::info,     GREEN);
  sink->set_color(spdlog::level::warn,     YELLOW);
  sink->set_color(spdlog::level::err,      RED);
  sink->set_color(spdlog::level::critical, MAGENTA);
}
#endif

// -------------------------------------------------------------------------------------------------
// init(settings)
//  - Creates rotating file sinks for "app" and "socket" channels (no color).
//  - Optionally attaches a colored console sink (stdout) if settings.showConsole is true.
//  - Also routes to OutputDebugString via msvc sink (handy for DebugView).
//  - Builds async loggers (optional) to avoid blocking hot paths under high volume.
//  - Sets patterns so ONLY console shows colors (files/MSVC see clean text).
// -------------------------------------------------------------------------------------------------
void Logger_Spdlog::init(const arkan::relay::domain::Settings& s) {
  const fs::path dir      = s.logsDir.empty() ? fs::path{"logs"} : fs::path{s.logsDir};
  const fs::path appPath  = dir / (s.appLogFilename.empty()    ? "relay_app.log"    : s.appLogFilename);
  const fs::path sockPath = dir / (s.socketLogFilename.empty() ? "relay_socket.log" : s.socketLogFilename);

  boost::system::error_code ec;
  fs::create_directories(dir, ec);

  // Re-init safety: drop old named loggers if they exist
  if (auto prev = spdlog::get("app"))    spdlog::drop(prev->name());
  if (auto prev = spdlog::get("socket")) spdlog::drop(prev->name());

  // File sinks — always on, no colors
  auto app_file  = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(appPath.string(), 5 * 1024 * 1024, 3);
  auto sock_file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(sockPath.string(), 5 * 1024 * 1024, 3);
  app_file->set_level(spdlog::level::trace);
  sock_file->set_level(spdlog::level::trace);

  // Optional console sink — colored by level (ONLY console has color)
#if defined(_WIN32)
  std::shared_ptr<spdlog::sinks::wincolor_stdout_sink_mt> console_sink;
#else
  std::shared_ptr<spdlog::sinks::ansicolor_stdout_sink_mt> console_sink;
#endif

  if (s.showConsole) {
#if defined(_WIN32)
    console_sink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
#else
    console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
#endif
    configure_console_colors_(console_sink);
    console_sink->set_level(spdlog::level::debug); // tune console verbosity
  }

  // MSVC Output sink — useful with DebugView/IDE
  auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
  msvc_sink->set_level(spdlog::level::debug);

  // Assemble sink lists
  std::vector<spdlog::sink_ptr> app_sinks{app_file, msvc_sink};
  std::vector<spdlog::sink_ptr> sock_sinks{sock_file, msvc_sink};
  if (console_sink) {
    app_sinks.push_back(console_sink);
    sock_sinks.push_back(console_sink);
  }

  // (Optional) Async logging: smoother under heavy logging
  const bool use_async = true;
  const size_t qsize   = 8192;
  const size_t workers = 1;

  if (use_async) {
    try { spdlog::init_thread_pool(qsize, workers); } catch (...) { /* already initialized */ }
    app_  = std::make_shared<spdlog::async_logger>("app",    app_sinks.begin(),  app_sinks.end(),
               spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    sock_ = std::make_shared<spdlog::async_logger>("socket", sock_sinks.begin(), sock_sinks.end(),
               spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    spdlog::register_logger(app_);
    spdlog::register_logger(sock_);
  } else {
    app_  = std::make_shared<spdlog::logger>("app",    app_sinks.begin(),  app_sinks.end());
    sock_ = std::make_shared<spdlog::logger>("socket", sock_sinks.begin(), sock_sinks.end());
    spdlog::register_logger(app_);
    spdlog::register_logger(sock_);
  }

  // Unified pattern: ONLY console renders colors between %^ and %$.
  const char* pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] %^[%l]%$ %v";
  app_->set_pattern(pattern);
  sock_->set_pattern(pattern);

  // Logger levels (global); per-sink levels still apply.
  app_->set_level(spdlog::level::trace);
  sock_->set_level(spdlog::level::trace);

  // Flush policy
  app_->flush_on(spdlog::level::err);
  sock_->flush_on(spdlog::level::err);
  spdlog::flush_every(std::chrono::seconds(2));
}

// -------------------------------------------------------------------------------------------------
// app(level, msg)
//  - Logs a message on the "app" channel.
//  - Console colors: per-level on the [LEVEL] token; files/MSVC remain clean.
// -------------------------------------------------------------------------------------------------
void Logger_Spdlog::app(LogLevel level, const std::string& msg) {
  if (app_) app_->log(map_level(level), msg);
}

// -------------------------------------------------------------------------------------------------
// sock(level, msg)
//  - Logs a message on the "socket" channel.
//  - Same color behavior as app().
// -------------------------------------------------------------------------------------------------
void Logger_Spdlog::sock(LogLevel level, std::string_view msg) {
  if (sock_) sock_->log(map_level(level), msg);
}

// -------------------------------------------------------------------------------------------------
// set_level(level)
//  - Adjusts the global level of both loggers at runtime (trace/debug/info/...).
// -------------------------------------------------------------------------------------------------
void Logger_Spdlog::set_level(LogLevel level) {
  auto lv = map_level(level);
  if (app_)  app_->set_level(lv);
  if (sock_) sock_->set_level(lv);
}

} // namespace arkan::relay::infrastructure::logging
