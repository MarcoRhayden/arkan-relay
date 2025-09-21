/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⡀⠀⠀⠀⢀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣤⣶⣾⣿⡉⢤⣍⡓⢶⣶⣦⣤⣉⠒⠤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⣿⣿⣿⣿⣷⡀⠙⣿⣷⣌⠻⣿⣿⣿⣶⣌⢳⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣄⠈⢿⣿⡆⠹⣿⣿⣿⣿⣷⣿⡀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠹⣿⡄⢻⣿⣿⣿⣿⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠿⠿⣿⣿⣷⣽⣷⢸⣿⡿⣿⡿⠿⠿⣆⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀⠀⠀⠐⠾⢭⣭⡼⠟⠃⣤⡆⠘⢟⢺⣦⡀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢛⣥⣶⠾⠿⠛⠳⠶⢬⡁⠀⠀⠘⣃⠤⠤⠤⢍⠻⡄⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⡿⣫⣾⡿⢋⣥⣶⣿⠿⢿⣿⣿⣿⠩⠭⢽⣷⡾⢿⣿⣦⢱⡹⡀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⡟⠈⠛⠏⠰⢿⣿⣿⣧⣤⣼⣿⣿⣿⡏⠩⠽⣿⣀⣼⣿⣿⢻⣷⢡⠀⠀⠀
⠀⠀⠀⠀⠀⢰⣿⣿⣿⣿⣿⣿⢁⣿⣷⣦⡀⠀⠉⠙⠛⠛⠛⠋⠁⠙⢻⡆⠀⢌⣉⠉⠉⠀⠸⣿⣇⠆⠀⠀
⠀⠀⠀⠀⢀⣾⣿⣿⣿⣿⣿⡇⢸⣿⣿⣿⣿⠷⣄⢠⣶⣾⣿⣿⣿⣿⣿⠁⠀⠀⢿⣿⣿⣿⣷⠈⣿⠸⡀⠀
⠀⠀⠀⠀⣼⣿⣿⣿⣿⣿⣿⠀⣌⡛⠿⣿⣿⠀⠈⢧⢿⣿⡿⠟⠋⠉⣠⣤⣤⣤⣄⠙⢿⣿⠏⣰⣿⡇⢇⠀
⠀⠀⠀⣼⣿⣿⣿⣿⣿⣿⡇⢸⣿⣿⣶⣤⣙⠣⢀⠈⠘⠏⠀⠀⢀⣴⢹⡏⢻⡏⣿⣷⣄⠉⢸⣿⣿⣷⠸⡄
⠀⠀⣸⣿⣿⣿⣿⣿⣿⣿⠁⣾⣟⣛⠛⠛⠻⠿⠶⠬⠔⠀⣠⡶⠋⠿⠈⠷⠸⠇⠻⠏⠻⠆⣀⢿⣿⣿⡄⢇
⠀⢰⣿⣿⣿⣿⠿⠿⠛⠋⣰⣿⣿⣿⣿⠿⠿⠿⠒⠒⠂⠀⢨⡤⢶⣶⣶⣶⣶⣶⣶⣶⣶⠆⠃⣀⣿⣿⡇⣸
⢀⣿⣿⠿⠋⣡⣤⣶⣾⣿⣿⣿⡟⠁⠀⣠⣤⣴⣶⣶⣾⣿⣿⣷⡈⢿⣿⣿⣿⣿⠿⠛⣡⣴⣿⣿⣿⣿⠟⠁
⣼⠋⢁⣴⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣎⠻⠟⠋⣠⣴⣿⣿⣿⣿⠿⠋⠁⠀⠀
⢿⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⣴⠀⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⣠⣾⣿⠿⠿⠟⠋⠁⠀⠀⠀⠀⠀
⠀⠉⠛⠛⠿⠿⠿⢿⣿⣿⣿⣵⣾⣿⣧⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
*/

//
//  ⚔ Forged in chaos. Delivered with precision.
//
//  Author : Rhayden - Arkan Software
//  Email  : Rhayden@arkansoftware.com
//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <fcntl.h>
#include <io.h>
#include <spdlog/spdlog.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "application/ports/IFrameCodec.hpp"
#include "application/ports/IHook.hpp"
#include "application/ports/IKoreLink.hpp"
#include "application/ports/ILogger.hpp"
#include "application/services/BridgeService.hpp"
#include "domain/Settings.hpp"
#include "infrastructure/codec/FrameCodec_Noop.hpp"
#include "infrastructure/config/Config_Toml.hpp"
#include "infrastructure/hook/win32/Hook_Win32.hpp"
#include "infrastructure/link/KoreLink_Asio.hpp"
#include "infrastructure/logging/Logger_Spdlog.hpp"

using namespace arkan::relay;

// -------------------------------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------------------------------
static HANDLE g_thread = nullptr;
static HANDLE g_stop = nullptr;
static bool g_allocConsole = false;

// -------------------------------------------------------------------------------------------------
// EnsureConsole_New
// - Always create a brand-new console window for the DLL (never reuse parent's).
// - Wire Win32 STD handles and rebind CRT FILE*s to those handles.
// - Make stdout/stderr unbuffered and enable iostreams unit buffering.
// - Disable Quick Edit to avoid "frozen" output when selecting text.
// -------------------------------------------------------------------------------------------------
static void EnsureConsole_New()
{
  static bool done = false;
  if (done) return;

  if (GetConsoleWindow() != nullptr)
  {
    FreeConsole();
  }
  if (!AllocConsole())
  {
    OutputDebugStringA("ArkanRelay: AllocConsole failed; no console available\r\n");
    done = true;
    return;
  }
  g_allocConsole = true;

  // UTF-8 in/out
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  // Open raw console handles
  HANDLE hOut =
      CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  HANDLE hErr =
      CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  HANDLE hIn =
      CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hOut) SetStdHandle(STD_OUTPUT_HANDLE, hOut);
  if (hErr) SetStdHandle(STD_ERROR_HANDLE, hErr);
  if (hIn) SetStdHandle(STD_INPUT_HANDLE, hIn);

  // Rebind CRT FILE*s to those Win32 handles
  auto bind_file = [](HANDLE h, const char* mode, FILE** target, int oflag)
  {
    int fd = _open_osfhandle(reinterpret_cast<intptr_t>(h), oflag);
    if (fd != -1)
    {
      FILE* f = _fdopen(fd, mode);
      if (f && target) *target = f;
    }
  };

  FILE* outF = nullptr;
  FILE* errF = nullptr;
  FILE* inF = nullptr;
  bind_file(hOut, "w", &outF, _O_TEXT);
  bind_file(hErr, "w", &errF, _O_TEXT);
  bind_file(hIn, "r", &inF, _O_TEXT);

  if (outF) *stdout = *outF;
  if (errF) *stderr = *errF;
  if (inF) *stdin = *inF;

  _setmode(_fileno(stdout), _O_TEXT);
  _setmode(_fileno(stderr), _O_TEXT);
  _setmode(_fileno(stdin), _O_TEXT);

  // Unbuffered C stdio + auto-flushing iostreams
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  std::ios::sync_with_stdio(true);
  std::cout.setf(std::ios::unitbuf);
  std::wcout.setf(std::ios::unitbuf);

  // Avoid Quick Edit "pause"
  if (hIn)
  {
    DWORD mode = 0;
    if (GetConsoleMode(hIn, &mode))
    {
      mode |= ENABLE_EXTENDED_FLAGS;
      mode &= ~ENABLE_QUICK_EDIT_MODE;
      SetConsoleMode(hIn, mode);
    }
  }

  SetConsoleTitleW(L"Arkan Relay Console");
  ShowWindow(GetConsoleWindow(), SW_SHOW);
}

// -------------------------------------------------------------------------------------------------
// CrashFilter: log SEH faults to DebugView/Debugger
// -------------------------------------------------------------------------------------------------
static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep)
{
  char buf[256];
  std::snprintf(buf, sizeof(buf), "ArkanRelay: CRASH code=0x%08lX at=%p\r\n",
                ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
  OutputDebugStringA(buf);
  return EXCEPTION_EXECUTE_HANDLER;
}

// -------------------------------------------------------------------------------------------------
// RunRelayCore: main worker function executed on our thread
// -------------------------------------------------------------------------------------------------
static DWORD RunRelayCore()
{
  // Give the host a little time to initialize subsystems
  ::Sleep(1500);

  // --- Config ---
  infrastructure::config::Config_Toml cfgProvider;
  domain::Settings s = cfgProvider.load_or_create("arkan-relay.toml");

  auto has = [](const std::optional<std::string>& v) { return v && !v->empty(); };
  if (!has(s.fnSendAddr) || !has(s.fnRecvAddr) || !has(s.fnSeedAddr) || !has(s.fnChecksumAddr))
  {
    OutputDebugStringA(
        "ArkanRelay: missing required addresses (send/recv/seed/checksum) - aborting\r\n");
    return 0;
  }

  // --- Logger ---
  infrastructure::logging::Logger_Spdlog logger;
  logger.init(s);

  // Force aggressive flushing so logs show up immediately in the console
  spdlog::flush_on(spdlog::level::info);
  spdlog::flush_every(std::chrono::milliseconds(200));

  logger.app(application::ports::LogLevel::info, "Arkan Relay (DLL) starting...");

  // --- Infrastructure ---
  logger.app(application::ports::LogLevel::debug, "Creating link/codec/hook...");
  infrastructure::link::KoreLink_Asio link(logger);
  infrastructure::codec::FrameCodec_Noop codec;
  infrastructure::hook::Hook_Win32 hook(logger, s);

  // --- Service ---
  logger.app(application::ports::LogLevel::debug, "Wiring BridgeService...");
  auto bridge =
      std::make_unique<application::services::BridgeService>(hook, link, codec, logger, s);

  logger.app(application::ports::LogLevel::info, "Bridge starting (will install hook)...");
  bridge->start();

  // --- Wait for shutdown ---
  if (g_stop)
  {
    WaitForSingleObject(g_stop, INFINITE);
  }

  // --- Teardown ---
  logger.app(application::ports::LogLevel::info, "Stopping bridge...");
  bridge->stop();
  logger.app(application::ports::LogLevel::info, "Bridge stopped. Bye.");

  return 0;
}

// -------------------------------------------------------------------------------------------------
// Thread entry (SEH-wrapped)
// -------------------------------------------------------------------------------------------------
static DWORD WINAPI ArkanRelayThread(LPVOID)
{
  __try
  {
    return RunRelayCore();
  }
  __except (CrashFilter(GetExceptionInformation()))
  {
    return 0;
  }
}

// -------------------------------------------------------------------------------------------------
// DllMain: minimal loader entry
// -------------------------------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
  switch (reason)
  {
    case DLL_PROCESS_ATTACH:
    {
      DisableThreadLibraryCalls(hModule);

      // Always use a fresh console for the DLL and wire stdio properly
      EnsureConsole_New();

      // Manual-reset event used to request shutdown
      g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);

      // Launch worker thread
      g_thread = CreateThread(nullptr, 0, ArkanRelayThread, nullptr, 0, nullptr);
    }
    break;

    case DLL_PROCESS_DETACH:
    {
      // Signal shutdown and release event
      if (g_stop)
      {
        SetEvent(g_stop);
        CloseHandle(g_stop);
        g_stop = nullptr;
      }

      // Free the console only if we allocated it
      if (g_allocConsole)
      {
        std::fflush(stdout);
        std::fflush(stderr);
        FreeConsole();
        g_allocConsole = false;
      }
    }
    break;
  }
  return TRUE;
}
