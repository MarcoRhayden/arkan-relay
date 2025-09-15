#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#include "application/ports/IHook.hpp"
#include "win32/MemoryAccess.hpp"
#include "win32/SlotPatcher.hpp"

namespace arkan::relay::infrastructure::win32
{

class SlotWatchdog
{
 public:
  SlotWatchdog() = default;
  ~SlotWatchdog();

  template <typename FPSend, typename FPRecv>
  void start(uintptr_t send_slot, FPSend send_hook, uintptr_t recv_slot, FPRecv recv_hook)
  {
    if (running_.load(std::memory_order_acquire)) return;

    stop_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);

    th_ = std::thread(
        [this, send_slot, recv_slot, send_hook, recv_hook]
        {
          while (!stop_.load(std::memory_order_acquire))
          {
            if (is_readable(send_slot))
            {
              FPSend cur = read_typed<FPSend>(send_slot);
              if (cur != send_hook)
              {
                SlotPatcher::force<FPSend>(send_slot, send_hook);
                ::OutputDebugStringA("[Hook_Win32] watchdog re-fixed SEND slot");
              }
            }
            if (is_readable(recv_slot))
            {
              FPRecv cur = read_typed<FPRecv>(recv_slot);
              if (cur != recv_hook)
              {
                SlotPatcher::force<FPRecv>(recv_slot, recv_hook);
                ::OutputDebugStringA("[Hook_Win32] watchdog re-fixed RECV slot");
              }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
          }
          running_.store(false, std::memory_order_release);
        });
  }

  void stop();

 private:
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_{false};
  std::thread th_;
};

}  // namespace arkan::relay::infrastructure::win32
