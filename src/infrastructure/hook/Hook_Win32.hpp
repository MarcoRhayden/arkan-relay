#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <vector>
#include <span>
#include <optional>
#include <cstddef>
#include <cstdint>

#include "domain/Settings.hpp"
#include "application/ports/ILogger.hpp"
#include "application/ports/IHook.hpp"

#ifndef WSAAPI
#define WSAAPI __stdcall
#endif

namespace arkan::relay::infrastructure::hook {

namespace ports  = ::arkan::relay::application::ports;
namespace domain = ::arkan::relay::domain;

class Hook_Win32 : public ports::IHook {
    public:
        Hook_Win32(ports::ILogger& log, const domain::Settings& s);
        ~Hook_Win32() override { uninstall(); }

        bool install() override;
        void uninstall() override;

        private:
            using send_fp = int  (WSAAPI*)(SOCKET, const char*, int, int);
            using recv_fp = int  (WSAAPI*)(SOCKET, char*, int, int);
            using seed_fp = BYTE (WINAPI*)(const BYTE*, UINT);
            using sum_fp  = BYTE (WINAPI*)(const BYTE*, UINT, UINT, UINT, UINT);

            static int  WSAAPI hooked_send(SOCKET s, const char* buf, int len, int flags);
            static int  WSAAPI hooked_recv(SOCKET s, char* buf, int len, int flags);

            static BYTE call_seed(const BYTE* data, size_t len);
            static BYTE call_checksum(const BYTE* data, size_t len,
                                        uint32_t counter, uint32_t low, uint32_t high);

            static bool patch_pointer(uintptr_t address, uintptr_t new_value, uintptr_t* old_value);

            static inline std::atomic<int>       s_counter{0};
            static inline std::atomic<bool>      s_found1c0b{false};
            static inline std::atomic<uint32_t>  s_high{0}, s_low{0};

            static inline send_fp s_original_send = nullptr;
            static inline recv_fp s_original_recv = nullptr;
            static inline seed_fp s_seed_fp       = nullptr;
            static inline sum_fp  s_checksum_fp   = nullptr;

            // pointer to current instance (to access on_send/on_recv callbacks)
            static inline std::atomic<Hook_Win32*> s_self{nullptr};

            // 32-bit addresses (for slots and functions in client memory)
            uintptr_t addr_send_ptr_  = 0;
            uintptr_t addr_recv_ptr_  = 0;
            uintptr_t addr_seed_      = 0;
            uintptr_t addr_checksum_  = 0;

            ports::ILogger&         log_;
            const domain::Settings& cfg_;
    };

} // namespace arkan::relay::infrastructure::hook
