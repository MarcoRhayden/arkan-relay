#pragma once
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <vector>
#include <span>
#include <optional>
#include "domain/Settings.hpp"
#include "application/ports/ILogger.hpp"
#include "application/ports/IHook.hpp"      

#ifndef WSAAPI
#define WSAAPI __stdcall
#endif

namespace arkan::relay::infrastructure::hook {

    class Hook_Win32 : public application::ports::IHook {
        public:
            Hook_Win32(application::ports::ILogger& log, const domain::Settings& s);
            ~Hook_Win32() override { detach(); }

            bool apply() override;
            void detach() override;

            // handlers
            void on_client_send(std::span<const std::byte> data) override;
            void on_client_recv(std::span<const std::byte> data) override;

        private:
            using send_fp = int (WSAAPI*)(SOCKET, const char*, int, int);
            using recv_fp = int (WSAAPI*)(SOCKET, char*, int, int);

            static int WSAAPI hooked_send(SOCKET s, const char* buf, int len, int flags);
            static int WSAAPI hooked_recv(SOCKET s, char* buf, int len, int flags);

            static BYTE __stdcall call_seed(const BYTE* data, size_t len);
            static BYTE __stdcall call_checksum(const BYTE* data, size_t len, uint32_t counter, uint32_t low, uint32_t high);

            static bool patch_pointer(uintptr_t address, uintptr_t new_value, uintptr_t* old_value);

            static inline std::atomic<int>     s_counter{0};
            static inline std::atomic<bool>    s_found1c0b{false};
            static inline std::atomic<uint32_t> s_high{0}, s_low{0};

            static inline send_fp s_original_send = nullptr;
            static inline recv_fp s_original_recv = nullptr;

            uintptr_t addr_send_ptr_     = 0;
            uintptr_t addr_recv_ptr_     = 0;
            uintptr_t addr_seed_         = 0;
            uintptr_t addr_checksum_     = 0;

            application::ports::ILogger&   log_;
            const domain::Settings&        cfg_;
    };

} // namespace arkan::relay::infrastructure::hook
