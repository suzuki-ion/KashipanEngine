#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Include before Windows.h (Winsock 1 and Winsock 2 cannot coexist).
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include "Debug/Profiler.h"
#include <json.hpp>
#include <atomic>
#include <thread>
#include <cstdlib>

#pragma comment(lib, "Ws2_32.lib")

namespace KashipanEngine {

/// One read-only loopback client. All serialization and socket I/O run on a
/// worker. There is only one pending packet; slow clients disconnect after 2 s.
class ProfilerServer final {
public:
    static constexpr unsigned short DefaultPort = 47180;
    ProfilerServer() : ProfilerServer(EnvironmentPort()) {}
    explicit ProfilerServer(unsigned short port) {
        if (!port) { status_ = "[Profiler] TCP disabled"; return; }
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return;
        winsock_ = true;
        listener_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener_ == INVALID_SOCKET) return;
        const BOOL exclusive = TRUE;
        if (setsockopt(listener_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<const char *>(&exclusive), sizeof(exclusive)) != 0) return;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        u_long nonblocking = 1;
        if (bind(listener_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0 ||
            listen(listener_, 1) != 0 || ioctlsocket(listener_, FIONBIO, &nonblocking) != 0) return;
        try {
            worker_ = std::thread([this] { Run(); });
            status_ = "[Profiler] CPU viewer: 127.0.0.1:" + std::to_string(port);
        } catch (...) { status_ = "[Profiler] Worker could not start"; }
    }
    ~ProfilerServer() {
        stopping_ = true;
        if (worker_.joinable()) worker_.join();
        if (listener_ != INVALID_SOCKET) closesocket(listener_);
        if (winsock_) WSACleanup();
    }
    ProfilerServer(const ProfilerServer &) = delete;
    ProfilerServer &operator=(const ProfilerServer &) = delete;
    const std::string &Status() const { return status_; }

private:
    static unsigned short EnvironmentPort() {
        char value[32]{};
        const DWORD size = GetEnvironmentVariableA("KASHIPAN_PROFILER_PORT", value, sizeof(value));
        if (!size) return DefaultPort;
        if (size >= sizeof(value)) return 0;
        char *end = nullptr;
        const unsigned long port = std::strtoul(value, &end, 10);
        if (end == value || *end || port > 65535) return 0;
        return static_cast<unsigned short>(port);
    }
    static std::string Packet() {
        const auto snapshot = Profiler::GetInstance().GetSnapshot();
        nlohmann::json rows = nlohmann::json::array();
        for (const auto &row : snapshot.results) {
            rows.push_back({{"name", row.name}, {"milliseconds", row.milliseconds},
                {"averageMs", row.averageMs}, {"peakMs", row.peakMs}, {"calls", row.calls}});
        }
        return nlohmann::json{{"type", "kashipan.cpu"}, {"version", 1},
            {"pid", GetCurrentProcessId()}, {"frameId", snapshot.frameId},
            {"frameMs", snapshot.frameMs}, {"fps", snapshot.fps},
            {"frames", snapshot.frames}, {"sampleCount", snapshot.sampleCount},
            {"results", std::move(rows)}}.dump(-1, ' ', false,
                nlohmann::json::error_handler_t::replace) + "\n";
    }
    void Run() noexcept {
        using Clock = std::chrono::steady_clock;
        SOCKET client = INVALID_SOCKET;
        std::string pending;
        std::size_t offset = 0;
        auto next = Clock::now(), deadline = next;
        while (!stopping_) {
            try {
                if (client == INVALID_SOCKET) {
                    client = accept(listener_, nullptr, nullptr);
                    if (client != INVALID_SOCKET) {
                        u_long nonblocking = 1;
                        if (ioctlsocket(client, FIONBIO, &nonblocking) != 0) {
                            closesocket(client); client = INVALID_SOCKET;
                        }
                        next = Clock::now();
                    }
                }
                if (client != INVALID_SOCKET) {
                    // Also detect a disconnected client while no frames advance.
                    char probe;
                    const int received = recv(client, &probe, 1, MSG_PEEK);
                    if (received >= 0 || WSAGetLastError() != WSAEWOULDBLOCK)
                        throw std::runtime_error("client closed or sent unsupported input");
                    const auto now = Clock::now();
                    if (pending.empty() && now >= next) {
                        pending = Packet(); offset = 0;
                        if (pending.size() > 1024 * 1024) throw std::runtime_error("packet too large");
                        next = now + std::chrono::milliseconds(100);
                        deadline = now + std::chrono::seconds(2);
                    }
                    if (!pending.empty()) {
                        const int sent = send(client, pending.data() + offset,
                            static_cast<int>(pending.size() - offset), 0);
                        if (sent > 0) {
                            offset += static_cast<std::size_t>(sent);
                            if (offset == pending.size()) pending.clear();
                        } else if (sent == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
                            throw std::runtime_error("send failed");
                        }
                        if (!pending.empty() && now >= deadline) throw std::runtime_error("slow client");
                    }
                }
            } catch (...) {
                if (client != INVALID_SOCKET) closesocket(client);
                client = INVALID_SOCKET;
                pending.clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (client != INVALID_SOCKET) closesocket(client);
    }
    std::atomic<bool> stopping_{false};
    SOCKET listener_ = INVALID_SOCKET;
    bool winsock_ = false;
    std::thread worker_;
    std::string status_ = "[Profiler] TCP unavailable (port may already be in use)";
};
} // namespace KashipanEngine
