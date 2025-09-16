#pragma once
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <string_view>

#include "application/ports/ILogger.hpp"
#include "domain/Settings.hpp"

namespace arkan::relay::infrastructure::logging
{

class Logger_Spdlog final : public arkan::relay::application::ports::ILogger
{
 public:
  void init(const arkan::relay::domain::Settings& s) override;

  void app(arkan::relay::application::ports::LogLevel level, const std::string& msg) override;

  void sock(arkan::relay::application::ports::LogLevel level, std::string_view msg) override;

  void set_level(arkan::relay::application::ports::LogLevel level);

 private:
  std::shared_ptr<spdlog::logger> app_;
  std::shared_ptr<spdlog::logger> sock_;

  // Helpers
  static spdlog::level::level_enum map_level(arkan::relay::application::ports::LogLevel l);
};

}  // namespace arkan::relay::infrastructure::logging
