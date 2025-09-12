#include "infrastructure/logging/Logger_Spdlog.hpp"
#include "application/ports/ILogger.hpp"
#include "domain/Settings.hpp"
#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

using arkan::relay::infrastructure::logging::Logger_Spdlog;
using arkan::relay::application::ports::LogLevel;
using arkan::relay::domain::Settings;
namespace fs = boost::filesystem;

static fs::path tmp_dir(const std::string& name) {
  auto dir = fs::temp_directory_path() / ("arkan-relay-logs-" + name);
  fs::create_directories(dir);
  return dir;
}

TEST(LoggerSpdlog, CreatesFilesAndIsIdempotent) {
  Settings s;
  auto dir = tmp_dir("smoke");
  s.logsDir           = dir.string();
  s.appLogFilename    = "app.log";
  s.socketLogFilename = "socket.log";
  s.showConsole       = false;
  s.saveLog           = true;
  s.saveSocketLog     = true;

  Logger_Spdlog log;
  EXPECT_NO_THROW(log.init(s));
  EXPECT_NO_THROW(log.app(LogLevel::info, "hello app"));
  EXPECT_NO_THROW(log.sock(LogLevel::info, "hello sock"));

  EXPECT_TRUE(fs::exists(dir / "app.log"));
  EXPECT_TRUE(fs::exists(dir / "socket.log"));

  EXPECT_NO_THROW(log.init(s));
}
