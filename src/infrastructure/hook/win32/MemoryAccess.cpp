#include "win32/MemoryAccess.hpp"

namespace arkan::relay::infrastructure::win32
{

PageGuard::PageGuard(void* base, size_t size, DWORD want) : base_(base), size_(size)
{
  MEMORY_BASIC_INFORMATION mbi{};
  if (::VirtualQuery(base_, &mbi, sizeof(mbi)) != sizeof(mbi)) return;

  DWORD prot = mbi.Protect;
  DWORD baseProt = page_class(prot);
  if (is_writable_class(baseProt) && is_writable_class(page_class(want)))
  {
    ok_ = true;
    changed_ = false;
    return;  // already writable enough
  }

  if (::VirtualProtect(mbi.BaseAddress, mbi.RegionSize, want, &old_))
  {
    ok_ = true;
    changed_ = true;
  }
}

PageGuard::~PageGuard()
{
  if (ok_ && changed_)
  {
    DWORD tmp = 0;
    ::VirtualProtect(base_, size_, old_, &tmp);
  }
}

bool is_readable(uintptr_t addr)
{
  if (!addr) return false;

  MEMORY_BASIC_INFORMATION mbi{};
  if (::VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) != sizeof(mbi))
    return false;
  if (mbi.State != MEM_COMMIT) return false;

  const DWORD p = page_class(mbi.Protect);
  return p == PAGE_READWRITE || p == PAGE_READONLY || p == PAGE_WRITECOPY ||
         p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

}  // namespace arkan::relay::infrastructure::win32
