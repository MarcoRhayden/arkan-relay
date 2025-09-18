#pragma once
#include <atomic>
#include <cstdint>

namespace arkan::relay::application::services
{

struct CounterRef
{
  std::atomic<int>* ai = nullptr;
  std::atomic<uint32_t>* au = nullptr;

  void store(uint32_t v, std::memory_order mo = std::memory_order_seq_cst) noexcept
  {
    if (ai)
      ai->store(static_cast<int>(v), mo);
    else if (au)
      au->store(v, mo);
  }

  uint32_t load(std::memory_order mo = std::memory_order_seq_cst) const noexcept
  {
    if (ai) return static_cast<uint32_t>(ai->load(mo));
    if (au) return au->load(mo);
    return 0u;
  }
};

struct ChecksumState
{
  CounterRef counter;
  std::atomic<bool>& found1c0b;
  std::atomic<uint32_t>& low;
  std::atomic<uint32_t>& high;

  ChecksumState(std::atomic<int>& c, std::atomic<bool>& f, std::atomic<uint32_t>& lo,
                std::atomic<uint32_t>& hi) noexcept
      : counter{&c, nullptr}, found1c0b(f), low(lo), high(hi)
  {
  }

  ChecksumState(std::atomic<uint32_t>& c, std::atomic<bool>& f, std::atomic<uint32_t>& lo,
                std::atomic<uint32_t>& hi) noexcept
      : counter{nullptr, &c}, found1c0b(f), low(lo), high(hi)
  {
  }

  void reset_all() noexcept
  {
    counter.store(0, std::memory_order_relaxed);
    found1c0b.store(false, std::memory_order_relaxed);
    low.store(0, std::memory_order_relaxed);
    high.store(0, std::memory_order_relaxed);
  }

  void roll12() noexcept
  {
    const auto c = counter.load(std::memory_order_relaxed);
    counter.store((c + 1u) & 0x0FFFu, std::memory_order_relaxed);
  }
};

}  // namespace arkan::relay::application::services
