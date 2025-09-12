// Logger_Spdlog.hpp
#pragma once
#include <memory>
#include <string>
#include "application/ports/ILogger.hpp"

namespace spdlog { class logger; }

namespace arkan::relay::infrastructure::logging {

class Logger_Spdlog : public arkan::relay::application::ports::ILogger {
public:
  void init(const arkan::relay::domain::Settings& s) override;
  void app (arkan::relay::application::ports::LogLevel level, const std::string& msg) override;
  void sock(arkan::relay::application::ports::LogLevel level, const std::string& msg) override;
private:
  std::shared_ptr<spdlog::logger> app_;
  std::shared_ptr<spdlog::logger> sock_;
};

} // namespace arkan::relay::infrastructure::logging
