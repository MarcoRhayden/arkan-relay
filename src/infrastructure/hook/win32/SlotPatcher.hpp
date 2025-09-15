#pragma once
#include <windows.h>

#include <cstdint>
#include <cstdio>

#include "application/ports/ILogger.hpp"
#include "win32/MemoryAccess.hpp"

namespace arkan::relay::infrastructure::win32
{

class SlotPatcher
{
 public:
  // Waits until slot becomes non-null, captures original, writes hook.
  // Returns true on success; logs helpful info on failure.
  template <typename FP>
  static bool wait_and_patch(uintptr_t slot, FP hook, FP& original,
                             arkan::relay::application::ports::ILogger& log,
                             unsigned total_ms = 60000, unsigned step_ms = 50)
  {
    using arkan::relay::application::ports::LogLevel;
    if (!slot || !hook) return false;

    const unsigned tries = step_ms ? (total_ms / step_ms) : 0;

    for (unsigned i = 0; i <= tries; ++i)
    {
      MEMORY_BASIC_INFORMATION mbi{};
      if (::VirtualQuery(reinterpret_cast<LPCVOID>(slot), &mbi, sizeof(mbi)) != sizeof(mbi))
      {
        log.app(LogLevel::err, "VirtualQuery failed for slot");
        return false;
      }

      FP cur = read_typed<FP>(slot);
      if (cur)
      {
        const DWORD prot = mbi.Protect;
        const DWORD baseProt = page_class(prot);
        const bool writable = is_writable_class(baseProt);

        PageGuard guard(mbi.BaseAddress, mbi.RegionSize,
                        (prot & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE)) | PAGE_READWRITE);
        if (!guard.ok())
        {
          char b[128];
          std::snprintf(b, sizeof(b), "VirtualProtect->RW failed (GLE=%lu)", GetLastError());
          log.app(LogLevel::err, b);
          return false;
        }

        original = cur;
        write_typed<FP>(slot, hook);

        // Restore protection if we changed it
        (void)writable;

        FP now = read_typed<FP>(slot);
        if (now == hook)
        {
          char msg[160];
          std::snprintf(msg, sizeof(msg), "Patched slot @ 0x%p (orig=%p hook=%p)",
                        reinterpret_cast<void*>(slot), (void*)original, (void*)hook);
          log.app(LogLevel::debug, msg);
          return true;
        }
        // someone flipped it immediately — retry
      }

      if (step_ms && i < tries) ::Sleep(step_ms);
    }

    char b[128];
    std::snprintf(b, sizeof(b), "Timeout waiting slot @ 0x%p to become non-null",
                  reinterpret_cast<void*>(slot));
    log.app(arkan::relay::application::ports::LogLevel::err, b);
    return false;
  }

  // Force a value (used by watchdog and uninstall).
  template <typename FP>
  static void force(uintptr_t slot, FP value)
  {
    if (!slot || !value) return;
    MEMORY_BASIC_INFORMATION mbi{};
    if (::VirtualQuery(reinterpret_cast<LPCVOID>(slot), &mbi, sizeof(mbi)) != sizeof(mbi)) return;

    PageGuard guard(
        mbi.BaseAddress, mbi.RegionSize,
        (mbi.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE)) | PAGE_READWRITE);
    if (!guard.ok()) return;

    write_typed<FP>(slot, value);
  }
};

}  // namespace arkan::relay::infrastructure::win32
