#pragma once
#include <windows.h>

#include <cstdint>

namespace arkan::relay::infrastructure::win32
{

// RAII VirtualProtect: only changes protection when needed, restores on exit.
class PageGuard
{
 public:
  PageGuard(void* base, size_t size, DWORD want);
  ~PageGuard();
  bool ok() const
  {
    return ok_;
  }
  bool changed() const
  {
    return changed_;
  }

 private:
  void* base_;
  size_t size_;
  DWORD old_{};
  bool changed_{false};
  bool ok_{false};
};

inline DWORD page_class(DWORD p)
{
  return p & 0xFF;
}

inline bool is_writable_class(DWORD baseProt)
{
  return baseProt == PAGE_READWRITE || baseProt == PAGE_WRITECOPY ||
         baseProt == PAGE_EXECUTE_READWRITE || baseProt == PAGE_EXECUTE_WRITECOPY;
}

bool is_readable(uintptr_t addr);

template <typename FP>
inline FP read_typed(uintptr_t slot)
{
  return *reinterpret_cast<FP*>(slot);
}

template <typename FP>
inline void write_typed(uintptr_t slot, FP value)
{
  *reinterpret_cast<FP*>(slot) = value;
}

}  // namespace arkan::relay::infrastructure::win32
