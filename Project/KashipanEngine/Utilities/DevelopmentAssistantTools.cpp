#include "Utilities/DevelopmentAssistantTools.h"
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")
#include <json.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace KashipanEngine::DevelopmentAI {
namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr size_t BodyLimit = 262144;
Json Error(const std::string &message) { return {{"ok", false}, {"error", message}}; }
struct Socket {
    SOCKET value = INVALID_SOCKET;
    ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
};
void Send(SOCKET socket, int status, const std::string &body) {
    const auto response = "HTTP/1.1 " + std::to_string(status) + " Response\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    size_t offset = 0;
    while (offset < response.size()) {
        const int n = send(socket, response.data() + offset, static_cast<int>(response.size() - offset), 0);
        if (n <= 0) return;
        offset += n;
    }
}
}
struct EditorTools::Impl {
    struct Call {
        std::string name, arguments, result;
        Clock::time_point deadline = Clock::now() + std::chrono::seconds(10);
        bool done = false;
    };
    std::string url, token, host;
    Json catalog;
    SOCKET listener = INVALID_SOCKET;
    std::thread worker;
    std::atomic<bool> stopped{false};
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::shared_ptr<Call>> pending;
    bool winsock = false;
    ~Impl() { Stop(); if (winsock) WSACleanup(); }
    void Stop() {
        stopped = true;
        changed.notify_all();
        if (worker.joinable()) worker.join(); // select/read/queue waits are all bounded.
        if (listener != INVALID_SOCKET) { closesocket(listener); listener = INVALID_SOCKET; }
        std::lock_guard lock(mutex);
        pending.clear();
    }
    Json Dispatch(const Json &request) {
        const auto id = request.value("id", Json{});
        auto response = [&](Json result) { return Json{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}}; };
        const auto method = request.value("method", std::string{});
        if (request.value("jsonrpc", std::string{}) != "2.0" || !request.is_object()) throw std::runtime_error("Invalid JSON-RPC");
        if (method == "initialize") {
            const auto version = request.at("params").value("protocolVersion", std::string{});
            return response({{"protocolVersion", version == "2024-11-05" || version == "2025-06-18" ? version : "2025-03-26"},
                {"capabilities", {{"tools", Json::object()}}}, {"serverInfo", {{"name", "KashipanEngine Editor"}, {"version", "1"}}}});
        }
        if (method == "ping") return response(Json::object());
        if (method == "tools/list") return response({{"tools", catalog}});
        if (method == "tools/call") {
            const auto &params = request.at("params");
            auto call = std::make_shared<Call>();
            call->name = params.at("name").get<std::string>();
            const auto arguments = params.value("arguments", Json::object());
            if (!arguments.is_object()) throw std::runtime_error("Arguments must be an object");
            call->arguments = arguments.dump();
            {
                std::unique_lock lock(mutex);
                if (stopped || pending.size() >= 16) call->result = Error("Editor API unavailable").dump();
                else {
                    pending.push_back(call);
                    changed.wait_until(lock, call->deadline, [&] { return stopped || call->done; });
                    if (!call->done) {
                        // Expired calls must never execute when the editor resumes.
                        call->done = true;
                        call->result = Error("Editor request cancelled or timed out").dump();
                    }
                }
            }
            auto result = Json::parse(call->result);
            auto images = result.value("_mcp_images", Json::array()); result.erase("_mcp_images");
            auto content = Json::array({{{"type", "text"}, {"text", result.dump()}}});
            for (const auto &item : images) if (item.value("type", std::string{}) == "image" && item.value("mimeType", std::string{}) == "image/png" && item.at("data").get_ref<const std::string &>().size() <= 3 * 1024 * 1024) content.push_back(item);
            return response({{"content", content},
                {"isError", !result.value("ok", false)}});
        }
        return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", -32601}, {"message", "Unknown method"}}}};
    }
    void Serve(SOCKET socket) {
        const DWORD timeout = 1500;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
        std::string input;
        const auto deadline = Clock::now() + std::chrono::seconds(3);
        size_t end = std::string::npos;
        while (!stopped && Clock::now() < deadline && input.size() < 16384) {
            char data[2048];
            const int n = recv(socket, data, sizeof(data), 0);
            if (n <= 0) return;
            input.append(data, n);
            end = input.find("\r\n\r\n");
            if (end != std::string::npos) break;
        }
        if (end == std::string::npos || stopped) { Send(socket, 400, "{}"); return; }
        const auto first = input.find("\r\n");
        std::map<std::string, std::string> headers;
        size_t pos = first + 2;
        while (pos < end) {
            const auto next = input.find("\r\n", pos), colon = input.find(':', pos);
            if (colon == std::string::npos || colon >= next) { Send(socket, 400, "{}"); return; }
            auto key = input.substr(pos, colon - pos);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            auto value = input.substr(colon + 1, next - colon - 1);
            value.erase(0, value.find_first_not_of(" \t"));
            if (!headers.emplace(key, value).second) { Send(socket, 400, "{}"); return; }
            pos = next + 2;
        }
        // Reject browser origins/DNS rebinding and authenticate even read-only discovery.
        if (headers["host"] != host || headers.contains("origin")) { Send(socket, 403, "{}"); return; }
        if (headers["authorization"] != "Bearer " + token) { Send(socket, 401, "{}"); return; }
        if (input.substr(0, first) != "POST /mcp HTTP/1.1") { Send(socket, 405, "{}"); return; }
        if (headers.contains("transfer-encoding") || !headers.contains("content-length") ||
            !headers["content-type"].starts_with("application/json")) { Send(socket, 400, "{}"); return; }
        const auto &lengthText = headers["content-length"];
        if (lengthText.empty() || lengthText.size() > 7 || lengthText.find_first_not_of("0123456789") != std::string::npos) { Send(socket, 413, "{}"); return; }
        const size_t length = std::stoul(lengthText);
        if (length > BodyLimit) { Send(socket, 413, "{}"); return; }
        std::string body = input.substr(end + 4);
        while (body.size() < length && !stopped && Clock::now() < deadline) {
            char data[4096];
            const int n = recv(socket, data, static_cast<int>(std::min<size_t>(sizeof(data), length - body.size())), 0);
            if (n <= 0) return;
            body.append(data, n);
        }
        if (stopped || body.size() != length) { Send(socket, 400, "{}"); return; }
        try {
            auto request = Json::parse(body);
            if (!request.is_object()) throw std::runtime_error("Expected object");
            if (!request.contains("id")) { Send(socket, 202, ""); return; }
            auto response = Dispatch(request).dump();
            if (response.size() > 4 * 1024 * 1024) response = Json{{"jsonrpc", "2.0"}, {"id", request["id"]},
                {"error", {{"code", -32603}, {"message", "Result too large; use pagination"}}}}.dump();
            Send(socket, 200, response);
        } catch (...) { Send(socket, 400, R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid request"}})"); }
    }
};
EditorTools::EditorTools(std::string catalog) : impl_(std::make_unique<Impl>()) {
    impl_->catalog = Json::parse(catalog);
    if (!impl_->catalog.is_array()) throw std::runtime_error("Invalid tool catalog");
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("Winsock unavailable");
    impl_->winsock = true;
    unsigned char bytes[32];
    if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) throw std::runtime_error("Random token unavailable");
    for (auto value : bytes) { impl_->token += "0123456789abcdef"[value >> 4]; impl_->token += "0123456789abcdef"[value & 15]; }
    impl_->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    const BOOL exclusive = TRUE;
    setsockopt(impl_->listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusive), sizeof(exclusive));
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (impl_->listener == INVALID_SOCKET || bind(impl_->listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || listen(impl_->listener, 8))
        throw std::runtime_error("Editor API listener unavailable");
    int size = sizeof(address);
    if (getsockname(impl_->listener, reinterpret_cast<sockaddr *>(&address), &size)) throw std::runtime_error("Editor API port unavailable");
    impl_->host = "127.0.0.1:" + std::to_string(ntohs(address.sin_port));
    impl_->url = "http://" + impl_->host + "/mcp";
    impl_->worker = std::thread([state = impl_.get()] {
        while (!state->stopped) {
            fd_set read; FD_ZERO(&read); FD_SET(state->listener, &read);
            timeval wait{0, 100000};
            if (select(0, &read, nullptr, nullptr, &wait) <= 0) continue;
            Socket client; client.value = accept(state->listener, nullptr, nullptr);
            if (client.value != INVALID_SOCKET) { try { state->Serve(client.value); } catch (...) { } }
        }
    });
}
EditorTools::~EditorTools() = default;
const std::string &EditorTools::Url() const { return impl_->url; }
const std::string &EditorTools::Token() const { return impl_->token; }
void EditorTools::Stop() { impl_->Stop(); }
void EditorTools::Revoke() { impl_->stopped = true; impl_->changed.notify_all(); }
void EditorTools::Pump(const std::function<std::string(const std::string &, const std::string &)> &handler) {
    // Hold the queue lock through execution: timeout/revocation cannot race a committed edit.
    std::lock_guard lock(impl_->mutex);
    for (unsigned i = 0; i < 4 && !impl_->pending.empty(); ++i) {
        auto call = impl_->pending.front(); impl_->pending.pop_front();
        if (call->done || impl_->stopped || Clock::now() >= call->deadline) continue;
        try { call->result = handler(call->name, call->arguments); }
        catch (const std::exception &e) { call->result = Error(e.what()).dump(); }
        catch (...) { call->result = Error("Editor operation failed").dump(); }
        call->done = true;
    }
    impl_->changed.notify_all();
}
}
