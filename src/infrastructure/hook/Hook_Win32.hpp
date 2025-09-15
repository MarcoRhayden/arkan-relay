#pragma once

#include <memory>

#include "application/ports/IHook.hpp"
#include "application/ports/ILogger.hpp"
#include "domain/Settings.hpp"

namespace arkan::relay::infrastructure::hook
{

namespace ports = ::arkan::relay::application::ports;
namespace domain = ::arkan::relay::domain;

class Hook_Win32 : public ports::IHook
{
 public:
  Hook_Win32(ports::ILogger& log, const domain::Settings& settings);
  ~Hook_Win32() override;  // calls uninstall()

  bool install() override;    // patch SEND/RECV slots + start watchdog
  void uninstall() override;  // restore originals + stop watchdog

  Hook_Win32(const Hook_Win32&) = delete;
  Hook_Win32& operator=(const Hook_Win32&) = delete;
  Hook_Win32(Hook_Win32&&) = delete;
  Hook_Win32& operator=(Hook_Win32&&) = delete;

 private:
  struct Impl;
  std::unique_ptr<Impl> p_;

  ports::ILogger& log_;
  const domain::Settings& cfg_;
};

}  // namespace arkan::relay::infrastructure::hook
