#include "infrastructure/config/Config_Toml.hpp"
#include "domain/Settings.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <fstream>

using arkan::relay::infrastructure::config::Config_Toml;
using arkan::relay::domain::Settings;
namespace fs = std::filesystem;

static fs::path tmp_file(const std::string& name) {
  auto dir = fs::temp_directory_path() / "arkan-relay-tests";
  fs::create_directories(dir);
  return dir / name;
}

TEST(ConfigToml, CreatesWithDefaultsWhenMissing) {
  auto cfg = tmp_file("missing.toml");
  if (fs::exists(cfg)) fs::remove(cfg);

  Config_Toml impl;                               
  Settings s = impl.load_or_create(cfg.string());

  EXPECT_TRUE(fs::exists(cfg));
  EXPECT_FALSE(s.ports.empty());
  EXPECT_FALSE(s.appLogFilename.empty());
  EXPECT_FALSE(s.socketLogFilename.empty());
  EXPECT_FALSE(s.logsDir.empty());
}

TEST(ConfigToml, ReadsCustomValues) {
  auto cfg = tmp_file("custom.toml");
  {
    std::ofstream out(cfg.string());
    out << "[general]\nports=[7000,7001]\nshowConsole=true\nsaveLog=false\nsaveSocketLog=false\n"
           "\n[logging]\ndir=\"logs-x\"\napp=\"a.log\"\nsocket=\"s.log\"\n";
  }

  Config_Toml impl;
  Settings s = impl.load_or_create(cfg.string());

  ASSERT_EQ(s.ports.size(), 2u);
  EXPECT_EQ(s.ports[0], 7000);
  EXPECT_EQ(s.ports[1], 7001);
  EXPECT_TRUE(s.showConsole);
  EXPECT_FALSE(s.saveLog);
  EXPECT_FALSE(s.saveSocketLog);
  EXPECT_EQ(s.logsDir, "logs-x");
  EXPECT_EQ(s.appLogFilename, "a.log");
  EXPECT_EQ(s.socketLogFilename, "s.log");
}

TEST(ConfigToml, FallbackWhenSectionMissing) {
  auto cfg = tmp_file("no-logging.toml");
  {
    std::ofstream out(cfg.string());
    out << "[general]\nports=[6500]\n";
  }

  Config_Toml impl;
  Settings s = impl.load_or_create(cfg.string());

  EXPECT_FALSE(s.logsDir.empty());
  EXPECT_FALSE(s.appLogFilename.empty());
  EXPECT_FALSE(s.socketLogFilename.empty());
}
