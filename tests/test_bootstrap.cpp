#include "infrastructure/config/Config_Toml.hpp"
#include "infrastructure/logging/Logger_Spdlog.hpp"
#include "application/services/Bootstrap.hpp"
#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

namespace infra_cfg  = arkan::relay::infrastructure::config;
namespace infra_log  = arkan::relay::infrastructure::logging;
namespace app_srv    = arkan::relay::application::services;
namespace fs = boost::filesystem;

static fs::path tmp_file(const std::string& name) {
  auto dir = fs::temp_directory_path() / "arkan-relay-bootstrap";
  fs::create_directories(dir);
  return dir / name;
}

TEST(Bootstrap, EndToEndCreatesConfigAndLogs) {
  auto cfg = tmp_file("boot.toml");
  if (fs::exists(cfg)) fs::remove(cfg);

  infra_cfg::Config_Toml cfg_impl;
  infra_log::Logger_Spdlog log_impl;

  app_srv::Bootstrap app{cfg_impl, log_impl};
  EXPECT_NO_THROW(app.run(cfg.string()));
  EXPECT_TRUE(fs::exists(cfg));
  EXPECT_TRUE(fs::exists(fs::path("logs") / "relay_app.log") || fs::exists("logs/relay_app.log"));
}
