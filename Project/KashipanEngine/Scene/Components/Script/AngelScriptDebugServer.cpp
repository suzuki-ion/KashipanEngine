#include "Scene/Components/Script/AngelScriptDebugServer.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <angelscript.h>

#include "Debug/Logger.h"
#include "Utilities/FileIO/JSON.h"

#pragma comment(lib, "Ws2_32.lib")

namespace KashipanEngine {

namespace {

constexpr int kScriptThreadId = 1;

std::string NormalizeSlashesAndCase(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

/// @brief 実行側の相対パスとVS Code側の絶対パスを同じキーへ変換する
std::string MakeSourceKey(const std::string &path) {
    std::string normalized = NormalizeSlashesAndCase(path);
    constexpr std::string_view assetsMarker = "/assets/";
    const auto markerPos = normalized.find(assetsMarker);
    if (markerPos != std::string::npos) {
        return normalized.substr(markerPos + 1);
    }
    if (normalized.starts_with("assets/")) {
        return normalized;
    }
    return normalized;
}

std::string GetFileName(const std::string &path) {
    const auto slashPos = path.find_last_of("/\\");
    return slashPos == std::string::npos ? path : path.substr(slashPos + 1);
}

std::string GetSectionName(asIScriptContext *context, const asUINT stackLevel, int *line = nullptr) {
    const char *section = nullptr;
    const int currentLine = context->GetLineNumber(stackLevel, nullptr, &section);
    if (line) {
        *line = currentLine;
    }
    return section ? section : "";
}

} // namespace

struct AngelScriptDebugServer::Impl final {
    enum class StepMode {
        None,
        Into,
        Over,
        Out,
    };

    ~Impl() {
        Stop();
    }

    bool Start(const std::uint16_t port) {
        if (running_.load()) {
            return true;
        }

        WSADATA wsaData{};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            Log("AngelScript DAP: WSAStartupに失敗しました", LogSeverity::Error);
            return false;
        }
        winsockInitialized_ = true;

        listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket_ == INVALID_SOCKET) {
            Log("AngelScript DAP: ソケットの作成に失敗しました", LogSeverity::Error);
            CleanupWinsock();
            return false;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);

        if (bind(listenSocket_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR ||
            listen(listenSocket_, 1) == SOCKET_ERROR) {
            Log("AngelScript DAP: localhost:" + std::to_string(port) + " の待受開始に失敗しました",
                LogSeverity::Error);
            closesocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET;
            CleanupWinsock();
            return false;
        }

        running_.store(true);
        serverThread_ = std::jthread([this](const std::stop_token stopToken) {
            ServerLoop(stopToken);
        });
        Log("AngelScript DAP: localhost:" + std::to_string(port) + " で待受を開始しました");
        return true;
    }

    void Stop() {
        if (!running_.exchange(false)) {
            CleanupWinsock();
            return;
        }

        clientConfigured_.store(false);
        ResumeExecution();

        if (serverThread_.joinable()) {
            serverThread_.request_stop();
        }

        {
            std::scoped_lock lock(socketMutex_);
            CloseSocket(clientSocket_);
            CloseSocket(listenSocket_);
        }

        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        CleanupWinsock();
    }

    void AttachContext(asIScriptContext *context) {
        if (!context) {
            return;
        }
        const int result = context->SetLineCallback(
            asFUNCTION(DebugLineCallback), this, asCALL_CDECL);
        if (result < 0) {
            Log("AngelScript DAP: ラインコールバックの設定に失敗しました", LogSeverity::Warning);
        }
    }

    static void DebugLineCallback(asIScriptContext *context, void *userData) {
        if (!userData) {
            return;
        }
        static_cast<Impl *>(userData)->OnLine(context);
    }

private:
    static void CloseSocket(SOCKET &socketHandle) {
        if (socketHandle == INVALID_SOCKET) {
            return;
        }
        shutdown(socketHandle, SD_BOTH);
        closesocket(socketHandle);
        socketHandle = INVALID_SOCKET;
    }

    void CleanupWinsock() {
        if (winsockInitialized_) {
            WSACleanup();
            winsockInitialized_ = false;
        }
    }

    void ServerLoop(const std::stop_token stopToken) {
        const SOCKET listenSocket = listenSocket_;
        while (running_.load() && !stopToken.stop_requested()) {
            SOCKET acceptedSocket = accept(listenSocket, nullptr, nullptr);
            if (acceptedSocket == INVALID_SOCKET) {
                if (running_.load()) {
                    Log("AngelScript DAP: クライアント接続の待受に失敗しました", LogSeverity::Warning);
                }
                break;
            }

            {
                std::scoped_lock lock(socketMutex_);
                clientSocket_ = acceptedSocket;
            }
            serverSequence_.store(1);
            clientConfigured_.store(false);
            pauseRequested_.store(false);
            ClearClientState();
            Log("AngelScript DAP: VS Codeが接続しました");

            ClientLoop(stopToken, acceptedSocket);

            clientConfigured_.store(false);
            ResumeExecution();
            {
                // 通常のDAP切断では送信完了後にFINで閉じ、クライアント側へ不要な
                // ECONNRESETを返さない。Stop時だけCloseSocketで強制的に待受を解除する。
                std::scoped_lock sendLock(sendMutex_);
                std::scoped_lock socketLock(socketMutex_);
                if (clientSocket_ == acceptedSocket) {
                    closesocket(clientSocket_);
                    clientSocket_ = INVALID_SOCKET;
                }
            }
            ClearClientState();
            if (running_.load()) {
                Log("AngelScript DAP: VS Codeが切断しました");
            }
        }
    }

    void ClientLoop(const std::stop_token stopToken, const SOCKET socketHandle) {
        std::string receiveBuffer;
        receiveBuffer.reserve(8192);
        bool closeRequested = false;

        while (running_.load() && !stopToken.stop_requested() && !closeRequested) {
            char chunk[4096];
            const int received = recv(socketHandle, chunk, static_cast<int>(sizeof(chunk)), 0);
            if (received <= 0) {
                break;
            }
            receiveBuffer.append(chunk, static_cast<std::size_t>(received));

            while (TryTakeMessage(receiveBuffer, closeRequested)) {
                if (closeRequested) {
                    break;
                }
            }
        }
    }

    bool TryTakeMessage(std::string &buffer, bool &closeRequested) {
        constexpr std::string_view headerTerminator = "\r\n\r\n";
        const auto headerEnd = buffer.find(headerTerminator);
        if (headerEnd == std::string::npos) {
            return false;
        }

        const std::string header = buffer.substr(0, headerEnd);
        constexpr std::string_view contentLengthName = "content-length:";
        const std::string normalizedHeader = NormalizeSlashesAndCase(header);
        const auto lengthPos = normalizedHeader.find(contentLengthName);
        if (lengthPos == std::string::npos) {
            Log("AngelScript DAP: Content-Lengthの無いメッセージを受信しました", LogSeverity::Warning);
            buffer.erase(0, headerEnd + headerTerminator.size());
            return !buffer.empty();
        }

        const auto valueBegin = header.find_first_not_of(" \t", lengthPos + contentLengthName.size());
        const auto valueEnd = header.find_first_of("\r\n", valueBegin);
        std::size_t contentLength = 0;
        try {
            contentLength = static_cast<std::size_t>(std::stoull(
                header.substr(valueBegin, valueEnd - valueBegin)));
        } catch (const std::exception &) {
            Log("AngelScript DAP: Content-Lengthが不正です", LogSeverity::Warning);
            buffer.clear();
            return false;
        }

        const std::size_t bodyBegin = headerEnd + headerTerminator.size();
        if (buffer.size() < bodyBegin + contentLength) {
            return false;
        }

        const std::string body = buffer.substr(bodyBegin, contentLength);
        buffer.erase(0, bodyBegin + contentLength);

        try {
            const JSON message = JSON::parse(body);
            closeRequested = HandleMessage(message);
        } catch (const std::exception &exception) {
            Log("AngelScript DAP: JSONメッセージの解析に失敗しました: " + std::string(exception.what()),
                LogSeverity::Warning);
        }
        return !buffer.empty();
    }

    bool HandleMessage(const JSON &message) {
        if (message.value("type", std::string{}) != "request") {
            return false;
        }

        const std::string command = message.value("command", std::string{});
        if (command == "initialize") {
            JSON capabilities = {
                {"supportsConfigurationDoneRequest", true},
                {"supportsTerminateRequest", false},
                {"supportsRestartRequest", false},
                {"supportsEvaluateForHovers", false},
                {"supportsSetVariable", false},
                {"supportsStepBack", false},
                {"supportsConditionalBreakpoints", false},
                {"supportsHitConditionalBreakpoints", false},
                {"supportsLogPoints", false},
                {"exceptionBreakpointFilters", JSON::array()},
            };
            SendResponse(message, capabilities);
            SendEvent("initialized");
        } else if (command == "attach") {
            SendResponse(message);
        } else if (command == "configurationDone") {
            clientConfigured_.store(true);
            SendResponse(message);
        } else if (command == "setBreakpoints") {
            HandleSetBreakpoints(message);
        } else if (command == "setExceptionBreakpoints") {
            SendResponse(message);
        } else if (command == "threads") {
            SendResponse(message, {
                {"threads", JSON::array({{{"id", kScriptThreadId}, {"name", "AngelScript"}}})},
            });
        } else if (command == "stackTrace") {
            HandleStackTrace(message);
        } else if (command == "scopes") {
            SendResponse(message, {{"scopes", JSON::array()}});
        } else if (command == "variables") {
            SendResponse(message, {{"variables", JSON::array()}});
        } else if (command == "continue") {
            SendResponse(message, {{"allThreadsContinued", true}});
            ResumeExecution();
            SendEvent("continued", {{"threadId", kScriptThreadId}, {"allThreadsContinued", true}});
        } else if (command == "pause") {
            pauseRequested_.store(true);
            SendResponse(message);
        } else if (command == "stepIn") {
            BeginStep(StepMode::Into);
            SendResponse(message);
            ResumeExecution();
            SendEvent("continued", {{"threadId", kScriptThreadId}, {"allThreadsContinued", true}});
        } else if (command == "next") {
            BeginStep(StepMode::Over);
            SendResponse(message);
            ResumeExecution();
            SendEvent("continued", {{"threadId", kScriptThreadId}, {"allThreadsContinued", true}});
        } else if (command == "stepOut") {
            BeginStep(StepMode::Out);
            SendResponse(message);
            ResumeExecution();
            SendEvent("continued", {{"threadId", kScriptThreadId}, {"allThreadsContinued", true}});
        } else if (command == "disconnect" || command == "terminate") {
            SendResponse(message);
            clientConfigured_.store(false);
            ResumeExecution();
            return true;
        } else {
            SendErrorResponse(message, "技術検証版では未対応のDAP要求です: " + command);
        }
        return false;
    }

    void HandleSetBreakpoints(const JSON &request) {
        const JSON &arguments = request.value("arguments", JSON::object());
        const JSON &source = arguments.value("source", JSON::object());
        const std::string sourcePath = source.value("path", std::string{});
        const std::string sourceKey = MakeSourceKey(sourcePath);

        std::unordered_set<int> lines;
        JSON responseBreakpoints = JSON::array();
        const JSON requestedBreakpoints = arguments.value("breakpoints", JSON::array());
        for (const auto &breakpoint : requestedBreakpoints) {
            const int line = breakpoint.value("line", 0);
            if (line <= 0) {
                continue;
            }
            lines.insert(line);
            responseBreakpoints.push_back({
                {"verified", true},
                {"line", line},
                {"source", source},
            });
        }

        {
            std::scoped_lock lock(stateMutex_);
            breakpoints_[sourceKey] = std::move(lines);
            clientSourcePaths_[sourceKey] = sourcePath;
        }
        SendResponse(request, {{"breakpoints", std::move(responseBreakpoints)}});
    }

    void HandleStackTrace(const JSON &request) {
        JSON frames = JSON::array();
        int totalFrames = 0;

        {
            std::scoped_lock lock(stateMutex_);
            if (stoppedContext_) {
                const asUINT callstackSize = stoppedContext_->GetCallstackSize();
                totalFrames = static_cast<int>(callstackSize);
                for (asUINT level = 0; level < callstackSize; ++level) {
                    int line = 1;
                    const std::string section = GetSectionName(stoppedContext_, level, &line);
                    const std::string sourcePath = FindClientSourcePathLocked(section);
                    asIScriptFunction *function = stoppedContext_->GetFunction(level);
                    const std::string functionName = function
                        ? function->GetDeclaration(true, true, true)
                        : std::string{"<unknown>"};

                    frames.push_back({
                        {"id", static_cast<int>(level) + 1},
                        {"name", functionName},
                        {"line", std::max(line, 1)},
                        {"column", 1},
                        {"source", {
                            {"name", GetFileName(sourcePath.empty() ? section : sourcePath)},
                            {"path", sourcePath.empty() ? section : sourcePath},
                        }},
                    });
                }
            }
        }

        SendResponse(request, {
            {"stackFrames", std::move(frames)},
            {"totalFrames", totalFrames},
        });
    }

    void BeginStep(const StepMode mode) {
        std::scoped_lock lock(stateMutex_);
        stepMode_ = mode;
        if (!stoppedContext_) {
            stepDepth_ = 0;
            stepSource_.clear();
            stepLine_ = 0;
            return;
        }
        stepDepth_ = stoppedContext_->GetCallstackSize();
        stepSource_ = MakeSourceKey(GetSectionName(stoppedContext_, 0, &stepLine_));
    }

    void ResumeExecution() {
        {
            std::scoped_lock lock(stateMutex_);
            resumeRequested_ = true;
        }
        resumeCondition_.notify_all();
    }

    void OnLine(asIScriptContext *context) {
        if (!context || !clientConfigured_.load()) {
            return;
        }

        int line = 0;
        const std::string section = GetSectionName(context, 0, &line);
        const std::string sourceKey = MakeSourceKey(section);
        const asUINT callstackSize = context->GetCallstackSize();
        std::string stopReason;

        {
            std::scoped_lock lock(stateMutex_);
            if (pauseRequested_.exchange(false)) {
                stopReason = "pause";
            } else {
                const auto breakpointFile = breakpoints_.find(sourceKey);
                if (breakpointFile != breakpoints_.end() && breakpointFile->second.contains(line)) {
                    stopReason = "breakpoint";
                }
            }

            if (stopReason.empty() && stepMode_ != StepMode::None) {
                const bool movedToAnotherLine = sourceKey != stepSource_ || line != stepLine_;
                if (stepMode_ == StepMode::Into && movedToAnotherLine) {
                    stopReason = "step";
                } else if (stepMode_ == StepMode::Over &&
                    callstackSize <= stepDepth_ && movedToAnotherLine) {
                    stopReason = "step";
                } else if (stepMode_ == StepMode::Out && callstackSize < stepDepth_) {
                    stopReason = "step";
                }
            }

            if (stopReason.empty()) {
                return;
            }

            stepMode_ = StepMode::None;
            stoppedContext_ = context;
            resumeRequested_ = false;
        }

        SendEvent("stopped", {
            {"reason", stopReason},
            {"description", stopReason == "breakpoint"
                ? "AngelScript breakpoint"
                : "AngelScript execution paused"},
            {"threadId", kScriptThreadId},
            {"allThreadsStopped", true},
        });

        std::unique_lock lock(stateMutex_);
        resumeCondition_.wait(lock, [this] {
            return resumeRequested_ || !running_.load() || !clientConfigured_.load();
        });
        if (stoppedContext_ == context) {
            stoppedContext_ = nullptr;
        }
        resumeRequested_ = false;
    }

    std::string FindClientSourcePathLocked(const std::string &section) const {
        const auto found = clientSourcePaths_.find(MakeSourceKey(section));
        return found == clientSourcePaths_.end() ? std::string{} : found->second;
    }

    void ClearClientState() {
        std::scoped_lock lock(stateMutex_);
        breakpoints_.clear();
        clientSourcePaths_.clear();
        stoppedContext_ = nullptr;
        resumeRequested_ = false;
        stepMode_ = StepMode::None;
        stepDepth_ = 0;
        stepSource_.clear();
        stepLine_ = 0;
    }

    void SendResponse(const JSON &request, JSON body = JSON::object()) {
        JSON response = {
            {"seq", serverSequence_.fetch_add(1)},
            {"type", "response"},
            {"request_seq", request.value("seq", 0)},
            {"success", true},
            {"command", request.value("command", std::string{})},
        };
        if (!body.empty()) {
            response["body"] = std::move(body);
        }
        SendMessage(response);
    }

    void SendErrorResponse(const JSON &request, const std::string &message) {
        SendMessage({
            {"seq", serverSequence_.fetch_add(1)},
            {"type", "response"},
            {"request_seq", request.value("seq", 0)},
            {"success", false},
            {"command", request.value("command", std::string{})},
            {"message", message},
        });
    }

    void SendEvent(const std::string &eventName, JSON body = JSON::object()) {
        JSON event = {
            {"seq", serverSequence_.fetch_add(1)},
            {"type", "event"},
            {"event", eventName},
        };
        if (!body.empty()) {
            event["body"] = std::move(body);
        }
        SendMessage(event);
    }

    void SendMessage(const JSON &message) {
        const std::string jsonText = message.dump();
        const std::string framedMessage =
            "Content-Length: " + std::to_string(jsonText.size()) + "\r\n\r\n" + jsonText;

        std::scoped_lock sendLock(sendMutex_);
        SOCKET socketHandle = INVALID_SOCKET;
        {
            std::scoped_lock socketLock(socketMutex_);
            socketHandle = clientSocket_;
        }
        if (socketHandle == INVALID_SOCKET) {
            return;
        }

        std::size_t sentTotal = 0;
        while (sentTotal < framedMessage.size()) {
            const std::size_t remaining = framedMessage.size() - sentTotal;
            const int sendSize = static_cast<int>(std::min<std::size_t>(
                remaining, static_cast<std::size_t>(std::numeric_limits<int>::max())));
            const int sent = send(socketHandle, framedMessage.data() + sentTotal, sendSize, 0);
            if (sent <= 0) {
                break;
            }
            sentTotal += static_cast<std::size_t>(sent);
        }
    }

    std::jthread serverThread_;
    std::atomic_bool running_ = false;
    std::atomic_bool clientConfigured_ = false;
    std::atomic_bool pauseRequested_ = false;
    std::atomic_int serverSequence_ = 1;
    bool winsockInitialized_ = false;

    std::mutex socketMutex_;
    std::mutex sendMutex_;
    SOCKET listenSocket_ = INVALID_SOCKET;
    SOCKET clientSocket_ = INVALID_SOCKET;

    std::mutex stateMutex_;
    std::condition_variable resumeCondition_;
    std::unordered_map<std::string, std::unordered_set<int>> breakpoints_;
    std::unordered_map<std::string, std::string> clientSourcePaths_;
    asIScriptContext *stoppedContext_ = nullptr;
    bool resumeRequested_ = false;
    StepMode stepMode_ = StepMode::None;
    asUINT stepDepth_ = 0;
    std::string stepSource_;
    int stepLine_ = 0;
};

AngelScriptDebugServer::AngelScriptDebugServer()
    : impl_(std::make_unique<Impl>()) {
}

AngelScriptDebugServer::~AngelScriptDebugServer() = default;

bool AngelScriptDebugServer::Start(const std::uint16_t port) {
    return impl_->Start(port);
}

void AngelScriptDebugServer::Stop() {
    impl_->Stop();
}

void AngelScriptDebugServer::AttachContext(asIScriptContext *context) const {
    impl_->AttachContext(context);
}

} // namespace KashipanEngine
