#include "PortClaim.hpp"

#ifdef _WIN32

#include <algorithm>
#include <cctype>
#include <sstream>

namespace arkan::relay::infrastructure
{

// Build a stable kernel object name from host and port.
std::string PortClaim::make_name(const std::string& host, uint16_t port)
{
  std::ostringstream oss;
  oss << "ArkanRelay_PortClaim_" << host << "_" << port;
  std::string s = oss.str();

  for (char& c : s)
  {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') continue;
    c = '_';
  }
  return s;
}

bool PortClaim::claim(const std::string& host, uint16_t port)
{
  // if already claimed by this object, return true
  if (h_ != nullptr) return true;

  name_ = make_name(host, port);

  // create or open a named mutex.
  HANDLE h = ::CreateMutexA(nullptr, FALSE, name_.c_str());
  if (h == nullptr)
  {
    // Failed to create/open the kernel object.
    name_.clear();
    return false;
  }

  // try to acquire without blocking
  const DWORD wait = ::WaitForSingleObject(h, 0);
  if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED)
  {
    h_ = h;
    return true;
  }

  // Could not acquire
  ::CloseHandle(h);
  name_.clear();
  return false;
}

void PortClaim::release()
{
  if (h_ == nullptr) return;

  // Release ownership and close handle.
  ::ReleaseMutex(h_);
  ::CloseHandle(h_);
  h_ = nullptr;
  name_.clear();
}

}  // namespace arkan::relay::infrastructure

#endif
