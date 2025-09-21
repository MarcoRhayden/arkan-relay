#pragma once
#include <string>

#include "application/ports/IConfigProvider.hpp"

namespace boost
{
namespace filesystem
{
class path;
}
}  // namespace boost

namespace arkan::relay::infrastructure::config
{

class Config_Toml : public arkan::relay::application::ports::IConfigProvider
{
 public:
  arkan::relay::domain::Settings load_or_create(const std::string& path) override;

 private:
  static void write_default(const boost::filesystem::path& path, arkan::relay::domain::Settings& s);
};

}  // namespace arkan::relay::infrastructure::config
