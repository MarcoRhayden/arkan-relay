#pragma once
#include <string>
#include "domain/Settings.hpp"

namespace arkan::relay::application::ports {

struct IConfigProvider {
    virtual ~IConfigProvider() = default;
    virtual arkan::relay::domain::Settings load_or_create(const std::string& path) = 0;
};

} // namespace arkan::relay::application::ports