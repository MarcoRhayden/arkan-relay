#include <windows.h>
#include "domain/Settings.hpp"
#include "infrastructure/config/Config_Toml.hpp"
#include "infrastructure/logging/Logger_Spdlog.hpp"
#include "infrastructure/link/KoreLink_Asio.hpp"
#include "application/services/BridgeService.hpp"
#include "infrastructure/hook/Hook_Win32.hpp"

using namespace arkan::relay;

static DWORD WINAPI ArkanRelayThread(LPVOID) {
  // Load config
  infrastructure::config::Config_Toml cfg;
  auto s = cfg.load_or_create("arkan-relay.toml");

  // Enforce required hook addresses
  auto has = [](const std::optional<std::string>& v){ return v && !v->empty(); };
  if (!has(s.fnSendAddr) || !has(s.fnRecvAddr) || !has(s.fnSeedAddr) || !has(s.fnChecksumAddr)) {
    MessageBoxA(nullptr,
      "Missing required addresses in [advanced] (fnSendAddr, fnRecvAddr, fnSeedAddr, fnChecksumAddr).\n"
      "Arkan Relay DLL will not hook.",
      "Arkan Relay", MB_ICONERROR | MB_OK);
    return 0;
  }

  // Logger
  infrastructure::logging::Logger_Spdlog logger;
  logger.init(s);
  logger.app(application::ports::LogLevel::info, "Arkan Relay (DLL) starting...");

  // Link to Kore over TCP (R/S/K framing no-op for now)
  infrastructure::link::KoreLink_Asio link(logger, s);
  link.connect("127.0.0.1", s.ports.front()); // 6900 default

  // FrameCodec e Hook
  application::codecs::FrameCodec_Noop codec;
  infrastructure::hook::Hook_Win32 hook(logger, s);

  // Bridge service
  application::services::BridgeService bridge(logger, codec, hook, link);
  if (!hook.apply()) {
    logger.app(application::ports::LogLevel::err, "Hook_Win32.apply() failed; detaching.");
    return 0;
  }
  logger.app(application::ports::LogLevel::info, "Hook applied. Running...");

  return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    HANDLE h = CreateThread(nullptr, 0, ArkanRelayThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
  }
  return TRUE;
}
