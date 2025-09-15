#pragma once
#include "application/ports/IFrameCodec.hpp"
#include "application/ports/IHook.hpp"
#include "application/ports/IKoreLink.hpp"
#include "application/ports/ILogger.hpp"
#include "domain/Settings.hpp"
#include "shared/hex/Hex.hpp"

namespace arkan::relay::application::services
{

class BridgeService
{
 public:
  BridgeService(ports::IHook& hook, ports::IKoreLink& link, ports::IFrameCodec& codec,
                ports::ILogger& logger, const domain::Settings& s)
      : hook_(hook), link_(link), codec_(codec), log_(logger), cfg_(s)
  {
  }

  void start();
  void stop();

 private:
  ports::IHook& hook_;
  ports::IKoreLink& link_;
  ports::IFrameCodec& codec_;
  ports::ILogger& log_;
  const domain::Settings& cfg_;
  bool running_{false};
};

}  // namespace arkan::relay::application::services
