#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#undef Filter

#include <gtest/gtest.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/ILogger.hpp"
#include "domain/Settings.hpp"
#include "infrastructure/hook/Hook_Win32.hpp"

using send_fp = int(WSAAPI*)(SOCKET, const char*, int, int);
using recv_fp = int(WSAAPI*)(SOCKET, char*, int, int);

static send_fp g_send_ptr = nullptr;
static recv_fp g_recv_ptr = nullptr;

static std::vector<unsigned char> g_last_buf;
static int WSAAPI orig_send(SOCKET, const char* buf, int len, int)
{
  g_last_buf.assign(reinterpret_cast<const unsigned char*>(buf),
                    reinterpret_cast<const unsigned char*>(buf) + len);
  return len;
}

static int WSAAPI orig_recv(SOCKET, char* buf, int len, int)
{
  (void)buf;
  return len;
}

static BYTE WINAPI test_seed(const BYTE* data, UINT len)
{
  unsigned sum = 0;
  for (UINT i = 0; i < len; ++i) sum += data[i];
  return static_cast<BYTE>(sum & 0xFF);
}

static BYTE WINAPI test_checksum(const BYTE* data, UINT len, UINT counter, UINT low, UINT high)
{
  unsigned sum = 0;
  for (UINT i = 0; i < len; ++i) sum += data[i];
  sum += counter + low + high;
  return static_cast<BYTE>(sum & 0xFF);
}

struct NullLogger : arkan::relay::application::ports::ILogger
{
  using LogLevel = arkan::relay::application::ports::LogLevel;

  void init(const arkan::relay::domain::Settings&) override {}

  void app(LogLevel, const std::string& m) override
  {
    last = m;
  }
  void sock(LogLevel, std::string_view m) override
  {
    last.assign(m.begin(), m.end());
  }

  std::string last;
};

static std::string to_hex(uintptr_t p)
{
  std::ostringstream oss;
  oss << "0x" << std::hex << std::uppercase << p;
  return oss.str();
}

// ============================================================

TEST(HookWin32, ApplyAndDetachPatchSlots)
{
  using namespace arkan::relay;

  g_send_ptr = &orig_send;
  g_recv_ptr = &orig_recv;

  domain::Settings s;
  s.fnSendAddr = to_hex(reinterpret_cast<uintptr_t>(&g_send_ptr));
  s.fnRecvAddr = to_hex(reinterpret_cast<uintptr_t>(&g_recv_ptr));
  s.fnSeedAddr = to_hex(reinterpret_cast<uintptr_t>(&test_seed));
  s.fnChecksumAddr = to_hex(reinterpret_cast<uintptr_t>(&test_checksum));

  NullLogger lg;
  infrastructure::hook::Hook_Win32 hook{lg, s};

  ASSERT_EQ(g_send_ptr, &orig_send);
  ASSERT_EQ(g_recv_ptr, &orig_recv);

  ASSERT_TRUE(hook.install());

  unsigned char pkt[] = {0x1C, 0x0B, 0xAA, 0x00};
  g_last_buf.clear();
  g_send_ptr(INVALID_SOCKET, reinterpret_cast<const char*>(pkt), 4, 0);

  ASSERT_EQ(g_last_buf.size(), 3u);

  BYTE expected = static_cast<BYTE>((0x1C + 0x0B) & 0xFF);
  EXPECT_EQ(g_last_buf.back(), expected);

  hook.uninstall();
  EXPECT_EQ(g_send_ptr, &orig_send);
  EXPECT_EQ(g_recv_ptr, &orig_recv);
}

#endif
