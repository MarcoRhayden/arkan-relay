#pragma once
#include <string>
#include <sstream>
#include <optional>
#include <type_traits>
#include "application/ports/IConfigProvider.hpp"
#include "application/ports/ILogger.hpp"

namespace arkan::relay::application::services {

struct Bootstrap {
  ports::IConfigProvider& cfg;
  ports::ILogger&         log;

  template <typename T>
  static inline bool has_text(const T& v) {
    if constexpr (std::is_same_v<T, std::optional<std::string>>)
      return v.has_value() && !v->value().empty();
    else
      return !v.empty();
  }
  template <typename T>
  static inline const std::string& as_text(const T& v) {
    if constexpr (std::is_same_v<T, std::optional<std::string>>)
      return v.value();
    else
      return v;
  }
  static inline const char* b2s(bool b) { return b ? "true" : "false"; }

  void run(const std::string& configPath) {
    auto s = cfg.load_or_create(configPath);
    log.init(s);

    using ports::LogLevel;

    log.app(LogLevel::info, "Arkan Relay started (Etapa 1)");
    log.app(LogLevel::info, std::string("Config file: ") + s.configPath);

    std::ostringstream ports_str;
    for (size_t i = 0; i < s.ports.size(); ++i) {
      if (i) ports_str << ", ";
      ports_str << s.ports[i];
    }
    log.app(LogLevel::info, std::string("Ports: ") + ports_str.str());

    std::ostringstream flags;
    flags << "Console: " << b2s(s.showConsole)
          << " | saveLog: " << b2s(s.saveLog)
          << " | saveSocketLog: " << b2s(s.saveSocketLog);
    log.app(LogLevel::info, flags.str());

    if (has_text(s.fnSeedAddr))     log.app(LogLevel::info, std::string("fnSeedAddr     = ") + as_text(s.fnSeedAddr));
    if (has_text(s.fnChecksumAddr)) log.app(LogLevel::info, std::string("fnChecksumAddr = ") + as_text(s.fnChecksumAddr));
    if (has_text(s.fnSendAddr))     log.app(LogLevel::info, std::string("fnSendAddr     = ") + as_text(s.fnSendAddr));
    if (has_text(s.fnRecvAddr))     log.app(LogLevel::info, std::string("fnRecvAddr     = ") + as_text(s.fnRecvAddr));
  }
};

} // namespace arkan::relay::application::services
