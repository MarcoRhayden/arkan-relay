#include "win32/SlotWatchdog.hpp"

namespace arkan::relay::infrastructure::win32
{

SlotWatchdog::~SlotWatchdog()
{
  stop();
}

void SlotWatchdog::stop()
{
  stop_.store(true, std::memory_order_release);
  if (th_.joinable()) th_.join();
  running_.store(false, std::memory_order_release);
}

}  // namespace arkan::relay::infrastructure::win32
