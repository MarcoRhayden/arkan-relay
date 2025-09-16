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

std::string join_ports(const std::vector<uint16_t>& v)
{
  std::ostringstream oss;
  for (size_t i = 0; i < v.size(); ++i)
  {
    if (i) oss << ", ";
    oss << v[i];
  }
  return oss.str();
}
}  // anonymous namespace

void Config_Toml::write_default(const fs::path& path, Settings& s)
{
  fs::create_directories(path.parent_path());
  std::ofstream out(path.string());
  out << "# arkan-relay.toml — Automatically generated initial configuration\n";
  out << "# Edit as needed and restart the application\n\n";

  out << "[general]\n";
  out << "ports = [" << join_ports(s.ports) << "]\n";
  out << "showConsole = " << (s.showConsole ? "true" : "false") << "\n";
  out << "saveLog = " << (s.saveLog ? "true" : "false") << "\n";
  out << "saveSocketLog = " << (s.saveSocketLog ? "true" : "false") << "\n\n";

  out << "[advanced]\n";
  out << "fnSeedAddr = \"\"\n";
  out << "fnChecksumAddr = \"\"\n";
  out << "fnSendAddr = \"\"\n";
  out << "fnRecvAddr = \"\"\n\n";

  out << "[logging]\n";
  out << "dir = \"" << kDefaultLogsDir << "\"\n";
  out << "app = \"" << kDefaultAppLog << "\"\n";
  out << "socket = \"" << kDefaultSocketLog << "\"\n";

  out << "\n[relay]\n";
  out << "ioThreads = " << s.relay.ioThreads << "\n";
  out << "recvBuffer = " << s.relay.recvBuffer << "\n";
  out << "sendBuffer = " << s.relay.sendBuffer << "\n";
  out << "maxSessions = " << s.relay.maxSessions << "\n";
  out << "framing = \"" << s.relay.framing << "\"\n";

  out << "\n[kore1]\n";
  out << "host = \"" << s.kore1.host << "\"\n";
  out << "port = " << s.kore1.port << "\n";

  out.close();

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
    write_default(path, s);
    return s;
  }

  // [general]
  if (auto gen = tbl["general"].as_table())
  {
    if (auto arr = (*gen)["ports"].as_array())
    {
      s.ports.clear();
      for (auto& v : *arr)
      {
        if (auto p = v.value<int64_t>()) s.ports.push_back(static_cast<uint16_t>(*p));
      }
      if (s.ports.empty()) s.ports = {6900, 6901, 6902};
    }
    if (auto v = (*gen)["showConsole"].value<bool>()) s.showConsole = *v;
    if (auto v = (*gen)["saveLog"].value<bool>()) s.saveLog = *v;
    if (auto v = (*gen)["saveSocketLog"].value<bool>()) s.saveSocketLog = *v;
  }

  // [advanced]
  if (auto adv = tbl["advanced"].as_table())
  {
    auto get_str = [&](const char* key) -> std::string
    {
      if (auto v = (*adv)[key].value<std::string>()) return *v;
      return {};
    };
    if (auto val = get_str("fnSeedAddr"); !val.empty()) s.fnSeedAddr = val;
    if (auto val = get_str("fnChecksumAddr"); !val.empty()) s.fnChecksumAddr = val;
    if (auto val = get_str("fnSendAddr"); !val.empty()) s.fnSendAddr = val;
    if (auto val = get_str("fnRecvAddr"); !val.empty()) s.fnRecvAddr = val;
  }

  // [logging]
  if (auto log = tbl["logging"].as_table())
  {
    if (auto v = (*log)["dir"].value<std::string>()) s.logsDir = *v;
    if (auto v = (*log)["app"].value<std::string>()) s.appLogFilename = *v;
    if (auto v = (*log)["socket"].value<std::string>()) s.socketLogFilename = *v;
  }

  // [relay]
  if (auto r = tbl["relay"].as_table())
  {
    if (auto v = (*r)["ioThreads"].value<int64_t>()) s.relay.ioThreads = static_cast<int>(*v);
    if (auto v = (*r)["recvBuffer"].value<int64_t>())
      s.relay.recvBuffer = static_cast<std::size_t>(*v);
    if (auto v = (*r)["sendBuffer"].value<int64_t>())
      s.relay.sendBuffer = static_cast<std::size_t>(*v);
    if (auto v = (*r)["maxSessions"].value<int64_t>())
      s.relay.maxSessions = static_cast<std::size_t>(*v);
    if (auto v = (*r)["framing"].value<std::string>()) s.relay.framing = *v;
  }

  // [kore1]
  if (auto k = tbl["kore1"].as_table())
  {
    if (auto v = (*k)["host"].value<std::string>()) s.kore1.host = *v;
    if (auto v = (*k)["port"].value<int64_t>()) s.kore1.port = static_cast<uint16_t>(*v);
  }

  if (s.logsDir.empty()) s.logsDir = kDefaultLogsDir;
  if (s.appLogFilename.empty()) s.appLogFilename = kDefaultAppLog;
  if (s.socketLogFilename.empty()) s.socketLogFilename = kDefaultSocketLog;

  return s;
}

}  // namespace arkan::relay::infrastructure::config
