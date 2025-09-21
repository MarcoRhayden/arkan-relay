#pragma once

#include <string>

#include "domain/Settings.hpp"

namespace arkan::relay::application::ports
{

enum class LogLevel
{
  trace,
  debug,
  info,
  warn,
  err,
  critical,
  off
};

struct ILogger
{
  virtual ~ILogger() = default;
  virtual void init(const arkan::relay::domain::Settings& s) = 0;
  virtual void app(LogLevel level, const std::string& msg) = 0;
  virtual void sock(LogLevel level, std::string_view msg) = 0;
};

}  // namespace arkan::relay::application::ports
