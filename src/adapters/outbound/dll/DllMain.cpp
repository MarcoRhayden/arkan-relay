#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <cstdio>
#include <memory>
#include <string>

#include "domain/Settings.hpp"
#include "infrastructure/config/Config_Toml.hpp"
#include "infrastructure/logging/Logger_Spdlog.hpp"
#include "infrastructure/link/KoreLink_Asio.hpp"
#include "infrastructure/codec/FrameCodec_Noop.hpp"
#include "infrastructure/hook/Hook_Win32.hpp"
#include "application/services/BridgeService.hpp"
#include "application/ports/ILogger.hpp"
#include "application/ports/IHook.hpp"
#include "application/ports/IKoreLink.hpp"
#include "application/ports/IFrameCodec.hpp"

using namespace arkan::relay;

static HANDLE g_thread = nullptr;
static HANDLE g_stop   = nullptr;

/*
 * Worker thread for the DLL:
 *  - Loads TOML config
 *  - (Optionally) Allocates a console if showConsole=true, before logger init
 *  - Initializes Winsock (WSAStartup)
 *  - Builds concrete infra objects (logger, link, codec, hook)
 *  - Wires and starts BridgeService
 *  - Waits on a stop event and then stops cleanly (WSACleanup/FreeConsole)
 */
static DWORD WINAPI ArkanRelayThread(LPVOID) {
  bool wsa_ok = false;
  bool console_open = false;

  // Load TOML config (creates defaults if missing)
  infrastructure::config::Config_Toml cfgProvider;
  domain::Settings s = cfgProvider.load_or_create("arkan-relay.toml");

  // If configured, create a console **before** logger. init so stdout sink is visible.
  if (s.showConsole) {
    // Create a new console for this process.
    if (AllocConsole()) {
      FILE* f;
      freopen_s(&f, "CONOUT$", "w", stdout);
      freopen_s(&f, "CONOUT$", "w", stderr);
      console_open = true;
    }
  }

  // Initialize Winsock early for any networking (Boost.Asio / raw sockets).
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2,2), &wsa) == 0) {
    wsa_ok = true;
  } else {
    MessageBoxA(nullptr, "WSAStartup failed", "Arkan Relay", MB_ICONERROR | MB_OK);
    // Continue without networking would be pointless; just wait for stop and exit.
    if (g_stop) WaitForSingleObject(g_stop, INFINITE);
    if (console_open) FreeConsole();
    return 0;
  }

  // Enforce required addresses for hook
  auto has = [](const std::optional<std::string>& v) { return v && !v->empty(); };
  if (!has(s.fnSendAddr) || !has(s.fnRecvAddr) || !has(s.fnSeedAddr) || !has(s.fnChecksumAddr)) {
    MessageBoxA(nullptr,
      "Missing required addresses in [advanced]:\n"
      "fnSendAddr, fnRecvAddr, fnSeedAddr, fnChecksumAddr.\n\n"
      "Arkan Relay DLL will not hook.",
      "Arkan Relay", MB_ICONERROR | MB_OK);

    if (g_stop) WaitForSingleObject(g_stop, INFINITE);
    if (wsa_ok) WSACleanup();
    if (console_open) FreeConsole();
    return 0;
  }

  // Concrete infrastructure objects
  infrastructure::logging::Logger_Spdlog logger;          // ILogger
  logger.init(s);
  logger.app(application::ports::LogLevel::info, "Arkan Relay (DLL) starting...");

  infrastructure::link::KoreLink_Asio     link(logger, s);   // IKoreLink
  infrastructure::codec::FrameCodec_Noop  codec;             // IFrameCodec (noop/passthrough)
  infrastructure::hook::Hook_Win32        hook(logger, s);   // IHook (Win32 send/recv trampolines)

  // BridgeService wiring
  // BridgeService(IHook&, IKoreLink&, IFrameCodec&, ILogger&, const Settings&)
  auto bridge = std::make_unique<application::services::BridgeService>(
      hook, link, codec, logger, s);

  // Start the bridge (registers callbacks, connects to Kore and installs the hook)
  bridge->start();
  logger.app(application::ports::LogLevel::info, "Arkan Relay bridge started.");

  // Wait here until DLL unload asks us to stop
  if (g_stop) {
    WaitForSingleObject(g_stop, INFINITE);
  }

  // Stop and clean up
  logger.app(application::ports::LogLevel::info, "Stopping Arkan Relay bridge...");
  bridge->stop();
  logger.app(application::ports::LogLevel::info, "Arkan Relay bridge stopped.");

  if (wsa_ok) WSACleanup();
  if (console_open) FreeConsole();
  return 0;
}

/*
 * DllMain:
 *  - On PROCESS_ATTACH: disables per-thread notifications, creates a stop event and spawns the worker thread.
 *  - On PROCESS_DETACH: signals the stop event (no blocking waits here to avoid loader lock issues).
 *
 *  Note: Heavy operations (like AllocConsole, WSAStartup, file I/O) are intentionally
 *  done in ArkanRelayThread instead of directly inside DllMain to avoid loader lock issues.
 */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
  switch (reason) {
    case DLL_PROCESS_ATTACH: {
      DisableThreadLibraryCalls(hModule);
      // Manual-reset event set to non-signaled initially
      g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
      g_thread = CreateThread(nullptr, 0, ArkanRelayThread, nullptr, 0, nullptr);
    } break;

    case DLL_PROCESS_DETACH: {
      if (g_stop) {
        SetEvent(g_stop);     // signal worker to stop
        CloseHandle(g_stop);  // safe to close handle now
        g_stop = nullptr;
      }
      // Do NOT wait here (avoids deadlocks in DllMain). OS will clean up thread handle on process exit.
    } break;

    default:
      break;
  }
  return TRUE;
}
