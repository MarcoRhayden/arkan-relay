#include "infrastructure/config/Config_Toml.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <exception>
#include <fstream>
#include <sstream>
#include <toml++/toml.hpp>
#include <vector>

using arkan::relay::domain::Settings;
namespace fs = boost::filesystem;

namespace arkan::relay::infrastructure::config
{

namespace
{
constexpr const char* kDefaultLogsDir = "logs";
constexpr const char* kDefaultAppLog = "relay_app.log";
constexpr const char* kDefaultSocketLog = "relay_socket.log";
constexpr const char* kDefaultHost = "127.0.0.1";
constexpr uint16_t kDefaultPorts[] = {5293, 5294, 5295};

static std::string join_ports(const std::vector<uint16_t>& v)
{
  std::ostringstream oss;
  for (size_t i = 0; i < v.size(); ++i)
  {
    if (i) oss << ", ";
    oss << v[i];
  }
  return oss.str();
}

static void ensure_non_empty_ports(std::vector<uint16_t>& ports)
{
  if (!ports.empty()) return;
  ports.assign(std::begin(kDefaultPorts), std::end(kDefaultPorts));
}
}  // namespace

void Config_Toml::write_default(const fs::path& path, Settings& s)
{
  fs::create_directories(path.parent_path());
  std::ofstream out(path.string());

  // Header
  out << "# arkan-relay.toml — Auto-generated initial configuration\n"
         "# Edit as needed and restart the application\n\n";

  // [advanced]
  out << "[advanced]\n";
  out << "fnSeedAddr     = \"\"\n";
  out << "fnChecksumAddr = \"\"\n";
  out << "fnSendAddr     = \"\"\n";
  out << "fnRecvAddr     = \"\"\n\n";

  // [logging]
  out << "[logging]\n";
  out << "showConsole       = " << (s.showConsole ? "true" : "false") << "\n";
  out << "saveLog           = " << (s.saveLog ? "true" : "false") << "\n";
  out << "saveSocketLog     = " << (s.saveSocketLog ? "true" : "false") << "\n";
  out << "logsDir           = \"" << kDefaultLogsDir << "\"\n";
  out << "appLogFilename    = \"" << kDefaultAppLog << "\"\n";
  out << "socketLogFilename = \"" << kDefaultSocketLog << "\"\n\n";

  // [kore]
  // prefer values already in Settings, otherwise use defaults
  if (s.kore.host.empty()) s.kore.host = kDefaultHost;
  ensure_non_empty_ports(s.kore.ports);

  out << "[kore]\n";
  out << "host  = \"" << s.kore.host << "\"\n";
  out << "ports = [" << join_ports(s.kore.ports) << "]\n\n";

  // [kore.reconnect] (defaults)
  out << "[kore.reconnect]\n";
  out << "initial_ms = 500\n";
  out << "max_ms     = 30000\n";
  out << "backoff    = 2.0\n";
  out << "jitter_p   = 0.2\n";

  out.close();

  // mirror useful defaults back to Settings
  s.logsDir = kDefaultLogsDir;
  s.appLogFilename = kDefaultAppLog;
  s.socketLogFilename = kDefaultSocketLog;
}

Settings Config_Toml::load_or_create(const std::string& configPath)
{
  Settings s;
  s.configPath = configPath;
  const fs::path path{configPath};

  if (!fs::exists(path))
  {
    // create with new layout
    write_default(path, s);
    return s;
  }

  toml::table tbl;
  try
  {
    tbl = toml::parse_file(path.string());
  }
  catch (const std::exception&)
  {
    // if parsing fails, recreate with defaults in the new layout
    write_default(path, s);
    return s;
  }

  // ---------------------------
  // [advanced]
  // ---------------------------
  if (auto adv = tbl["advanced"].as_table())
  {
    auto get_str = [&](const char* key) -> std::string
    {
      if (auto v = (*adv)[key].value<std::string>()) return *v;
      return {};
    };
    if (auto v = get_str("fnSeedAddr"); !v.empty()) s.fnSeedAddr = v;
    if (auto v = get_str("fnChecksumAddr"); !v.empty()) s.fnChecksumAddr = v;
    if (auto v = get_str("fnSendAddr"); !v.empty()) s.fnSendAddr = v;
    if (auto v = get_str("fnRecvAddr"); !v.empty()) s.fnRecvAddr = v;
  }

  // ---------------------------
  // [logging]
  // ---------------------------
  if (auto log = tbl["logging"].as_table())
  {
    if (auto v = (*log)["showConsole"].value<bool>()) s.showConsole = *v;
    if (auto v = (*log)["saveLog"].value<bool>()) s.saveLog = *v;
    if (auto v = (*log)["saveSocketLog"].value<bool>()) s.saveSocketLog = *v;
    if (auto v = (*log)["logsDir"].value<std::string>()) s.logsDir = *v;
    if (auto v = (*log)["appLogFilename"].value<std::string>()) s.appLogFilename = *v;
    if (auto v = (*log)["socketLogFilename"].value<std::string>()) s.socketLogFilename = *v;
  }

  if (s.logsDir.empty()) s.logsDir = kDefaultLogsDir;
  if (s.appLogFilename.empty()) s.appLogFilename = kDefaultAppLog;
  if (s.socketLogFilename.empty()) s.socketLogFilename = kDefaultSocketLog;

  // ---------------------------
  // [kore]
  // ---------------------------
  if (auto k = tbl["kore"].as_table())
  {
    if (auto v = (*k)["host"].value<std::string>()) s.kore.host = *v;

    if (auto arr = (*k)["ports"].as_array())
    {
      s.kore.ports.clear();
      for (auto& e : *arr)
      {
        if (auto p = e.value<int64_t>()) s.kore.ports.push_back(static_cast<uint16_t>(*p));
      }
    }
  }

  // fallback defaults if not provided
  if (s.kore.host.empty()) s.kore.host = kDefaultHost;
  ensure_non_empty_ports(s.kore.ports);

  // ---------------------------
  // [kore.reconnect]
  // ---------------------------
  auto read_reconnect = [&](const toml::table* rc)
  {
    if (!rc) return;

    if (auto v = (*rc)["initial_ms"].value<int64_t>())
      s.kore.reconnect.initial_ms = static_cast<int>(*v);

    if (auto v = (*rc)["max_ms"].value<int64_t>()) s.kore.reconnect.max_ms = static_cast<int>(*v);

    if (auto v = (*rc)["backoff"].value<double>()) s.kore.reconnect.backoff = *v;

    if (auto v = (*rc)["jitter_p"].value<double>()) s.kore.reconnect.jitter_p = *v;
  };

  if (auto r = tbl["kore"].as_table())
  {
    if (auto rt = (*r)["reconnect"].as_table()) read_reconnect(rt);
  }

  return s;
}

}  // namespace arkan::relay::infrastructure::config
