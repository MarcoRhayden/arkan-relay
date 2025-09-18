#pragma once
#include "application/services/protocol/ProtocolScanner.hpp"

namespace arkan::relay::application::services
{
// singleton instance scanner.
const IProtocolScanner& DefaultProtocolScanner();
}  // namespace arkan::relay::application::services
