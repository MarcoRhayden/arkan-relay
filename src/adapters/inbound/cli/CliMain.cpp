#include <iostream>
#include "infrastructure/config/Config_Toml.hpp"
#include "infrastructure/logging/Logger_Spdlog.hpp"
#include "application/services/Bootstrap.hpp"

int main(int argc, char** argv) {
  std::string configPath = (argc > 1) ? argv[1] : std::string{"arkan-relay.toml"};

  arkan::relay::infrastructure::config::Config_Toml   cfg_impl;
  arkan::relay::infrastructure::logging::Logger_Spdlog log_impl;

  // Application
  arkan::relay::application::services::Bootstrap app{cfg_impl, log_impl};
  app.run(configPath);

  std::cout << "Arkan Relay CLI up. Edit " << configPath << " and run again.\n";
  return 0;
}
