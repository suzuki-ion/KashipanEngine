#include "Scene/Components/Script/AngelScriptDebugServer.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
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
#include <add_on/scriptarray/scriptarray.h>

#include "Debug/Logger.h"
#include "Utilities/FileIO/JSON.h"

#pragma comment(lib, "Ws2_32.lib")

namespace KashipanEngine {

namespace {

constexpr int kScriptThreadId = 1;

std::string NormalizeSlashesAndCase(std::string path) {
    LogScope scope;
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
    LogScope scope;
    std::string normalized = NormalizeSlashesAndCase(path);
    constexpr std::string_view sourceRoots[] = {
        "/assets/",
        "/editortools/",
    };
    for (const auto marker : sourceRoots) {
        const auto markerPos = normalized.find(marker);
        if (markerPos != std::string::npos) {
            return normalized.substr(markerPos + 1);
        }
    }
    if (normalized.starts_with("assets/") || normalized.starts_with("editortools/")) {
        return normalized;
    }
    return normalized;
}

std::string GetFileName(const std::string &path) {
    LogScope scope;
    const auto slashPos = path.find_last_of("/\\");
    return slashPos == std::string::npos ? path : path.substr(slashPos + 1);
}

std::string GetSectionName(asIScriptContext *context, const asUINT stackLevel, int *line = nullptr) {
    LogScope scope;
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

    enum class VariableReferenceKind {
        Locals,
        Globals,
        ValueChildren,
    };

    struct VariableReference final {
        VariableReferenceKind kind = VariableReferenceKind::Locals;
        asIScriptContext *context = nullptr;
        asIScriptModule *module = nullptr;
        asUINT stackLevel = 0;
        void *address = nullptr;
        int typeId = asTYPEID_VOID;
        bool directObject = false;
    };

    ~Impl() {
        LogScope scope;
        Stop();
    }

    bool Start(const std::uint16_t port) {
        LogScope scope;
        if (running_.load()) {
            return true;
        }

        WSADATA wsaData{};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            Log(Translation("engine.script.dap.failed.wsastartup"), LogSeverity::Error);
            return false;
        }
        winsockInitialized_ = true;

        listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket_ == INVALID_SOCKET) {
            Log(Translation("engine.script.dap.failed.createsocket"), LogSeverity::Error);
            CleanupWinsock();
            return false;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);

        if (bind(listenSocket_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR ||
            listen(listenSocket_, 1) == SOCKET_ERROR) {
            Log(Translation("engine.script.dap.failed.listen") + "localhost:" + std::to_string(port),
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
        Log(Translation("engine.script.dap.listening") + "localhost:" + std::to_string(port));
        return true;
    }

    void Stop() {
        LogScope scope;
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
        LogScope scope;
        if (!context) {
            return;
        }
        const int result = context->SetLineCallback(
            asFUNCTION(DebugLineCallback), this, asCALL_CDECL);
        if (result < 0) {
            Log(Translation("engine.script.dap.failed.linecallback"), LogSeverity::Warning);
        }
    }

    static void DebugLineCallback(asIScriptContext *context, void *userData) {
        LogScope scope;
        if (!userData) {
            return;
        }
        static_cast<Impl *>(userData)->OnLine(context);
    }

private:
    static void CloseSocket(SOCKET &socketHandle) {
        LogScope scope;
        if (socketHandle == INVALID_SOCKET) {
            return;
        }
        shutdown(socketHandle, SD_BOTH);
        closesocket(socketHandle);
        socketHandle = INVALID_SOCKET;
    }

    void CleanupWinsock() {
        LogScope scope;
        if (winsockInitialized_) {
            WSACleanup();
            winsockInitialized_ = false;
        }
    }

    void ServerLoop(const std::stop_token stopToken) {
        LogScope scope;
        const SOCKET listenSocket = listenSocket_;
        while (running_.load() && !stopToken.stop_requested()) {
            SOCKET acceptedSocket = accept(listenSocket, nullptr, nullptr);
            if (acceptedSocket == INVALID_SOCKET) {
                if (running_.load()) {
                    Log(Translation("engine.script.dap.failed.accept"), LogSeverity::Warning);
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
            Log(Translation("engine.script.dap.client.connected"));

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
                Log(Translation("engine.script.dap.client.disconnected"));
            }
        }
    }

    void ClientLoop(const std::stop_token stopToken, const SOCKET socketHandle) {
        LogScope scope;
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
        LogScope scope;
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
            Log(Translation("engine.script.dap.message.nocontentlength"), LogSeverity::Warning);
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
            Log(Translation("engine.script.dap.message.invalidcontentlength"), LogSeverity::Warning);
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
            Log(Translation("engine.script.dap.message.parse.failed") + exception.what(),
                LogSeverity::Warning);
        }
        return !buffer.empty();
    }

    bool HandleMessage(const JSON &message) {
        LogScope scope;
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
            HandleScopes(message);
        } else if (command == "variables") {
            HandleVariables(message);
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
        LogScope scope;
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
        LogScope scope;
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

    void HandleScopes(const JSON &request) {
        LogScope scope;
        JSON scopes = JSON::array();
        const JSON &arguments = request.value("arguments", JSON::object());
        const int frameId = arguments.value("frameId", 0);

        {
            std::scoped_lock lock(stateMutex_);
            if (stoppedContext_ && frameId > 0) {
                const asUINT stackLevel = static_cast<asUINT>(frameId - 1);
                if (stackLevel < stoppedContext_->GetCallstackSize()) {
                    const int localsReference = AddVariableReferenceLocked({
                        .kind = VariableReferenceKind::Locals,
                        .context = stoppedContext_,
                        .stackLevel = stackLevel,
                    });
                    scopes.push_back({
                        {"name", "ローカル"},
                        {"presentationHint", "locals"},
                        {"variablesReference", localsReference},
                        {"expensive", false},
                    });

                    const int thisTypeId = stoppedContext_->GetThisTypeId(stackLevel);
                    void *thisPointer = stoppedContext_->GetThisPointer(stackLevel);
                    if (thisTypeId != asTYPEID_VOID && thisPointer) {
                        const int thisReference = AddVariableReferenceLocked({
                            .kind = VariableReferenceKind::ValueChildren,
                            .context = stoppedContext_,
                            .stackLevel = stackLevel,
                            .address = thisPointer,
                            .typeId = thisTypeId,
                            .directObject = true,
                        });
                        scopes.push_back({
                            {"name", "this"},
                            {"presentationHint", "locals"},
                            {"variablesReference", thisReference},
                            {"expensive", false},
                        });
                    }

                    asIScriptFunction *function = stoppedContext_->GetFunction(stackLevel);
                    asIScriptModule *module = function ? function->GetModule() : nullptr;
                    if (module && module->GetGlobalVarCount() > 0) {
                        const int globalsReference = AddVariableReferenceLocked({
                            .kind = VariableReferenceKind::Globals,
                            .context = stoppedContext_,
                            .module = module,
                            .stackLevel = stackLevel,
                        });
                        scopes.push_back({
                            {"name", "グローバル"},
                            {"presentationHint", "globals"},
                            {"variablesReference", globalsReference},
                            {"expensive", false},
                        });
                    }
                }
            }
        }

        SendResponse(request, {{"scopes", std::move(scopes)}});
    }

    void HandleVariables(const JSON &request) {
        LogScope scope;
        JSON variables = JSON::array();
        const JSON &arguments = request.value("arguments", JSON::object());
        const int referenceId = arguments.value("variablesReference", 0);

        {
            std::scoped_lock lock(stateMutex_);
            const auto found = variableReferences_.find(referenceId);
            if (found != variableReferences_.end() && stoppedContext_ &&
                found->second.context == stoppedContext_) {
                const VariableReference reference = found->second;
                if (reference.kind == VariableReferenceKind::Locals) {
                    AppendLocalVariablesLocked(reference, variables);
                } else if (reference.kind == VariableReferenceKind::Globals) {
                    AppendGlobalVariablesLocked(reference, variables);
                } else {
                    AppendValueChildrenLocked(reference, variables);
                }
            }
        }

        SendResponse(request, {{"variables", std::move(variables)}});
    }

    int AddVariableReferenceLocked(VariableReference reference) {
        LogScope scope;
        const int referenceId = nextVariableReference_++;
        variableReferences_.emplace(referenceId, std::move(reference));
        return referenceId;
    }

    static void *ResolveObjectPointer(void *address, const int typeId, const bool directObject) {
        LogScope scope;
        if (!address) {
            return nullptr;
        }
        if (directObject) {
            return address;
        }
        if ((typeId & asTYPEID_OBJHANDLE) != 0) {
            return *static_cast<void **>(address);
        }
        return address;
    }

    static std::string GetTypeName(asIScriptEngine *engine, const int typeId) {
        LogScope scope;
        if (!engine) {
            return {};
        }
        const char *declaration = engine->GetTypeDeclaration(typeId, true);
        return declaration ? declaration : "";
    }

    static bool IsArrayType(asITypeInfo *typeInfo) {
        LogScope scope;
        return typeInfo && std::string_view(typeInfo->GetName()) == "array" &&
            (typeInfo->GetFlags() & asOBJ_TEMPLATE) != 0;
    }

    static bool IsStringType(asITypeInfo *typeInfo) {
        LogScope scope;
        return typeInfo && std::string_view(typeInfo->GetName()) == "string";
    }

    static std::string FormatInteger(const std::int64_t value) {
        LogScope scope;
        return std::to_string(value);
    }

    std::string FormatValueLocked(
        asIScriptEngine *engine,
        void *address,
        const int typeId,
        const bool directObject = false) const {
        LogScope scope;
        if (!address) {
            return "<利用不可>";
        }

        switch (typeId) {
        case asTYPEID_BOOL:
            return *static_cast<const bool *>(address) ? "true" : "false";
        case asTYPEID_INT8:
            return FormatInteger(*static_cast<const std::int8_t *>(address));
        case asTYPEID_INT16:
            return FormatInteger(*static_cast<const std::int16_t *>(address));
        case asTYPEID_INT32:
            return FormatInteger(*static_cast<const std::int32_t *>(address));
        case asTYPEID_INT64:
            return FormatInteger(*static_cast<const std::int64_t *>(address));
        case asTYPEID_UINT8:
            return std::to_string(*static_cast<const std::uint8_t *>(address));
        case asTYPEID_UINT16:
            return std::to_string(*static_cast<const std::uint16_t *>(address));
        case asTYPEID_UINT32:
            return std::to_string(*static_cast<const std::uint32_t *>(address));
        case asTYPEID_UINT64:
            return std::to_string(*static_cast<const std::uint64_t *>(address));
        case asTYPEID_FLOAT: {
            std::ostringstream stream;
            stream << std::setprecision(7) << *static_cast<const float *>(address);
            return stream.str();
        }
        case asTYPEID_DOUBLE: {
            std::ostringstream stream;
            stream << std::setprecision(15) << *static_cast<const double *>(address);
            return stream.str();
        }
        default:
            break;
        }

        asITypeInfo *typeInfo = engine ? engine->GetTypeInfoById(typeId) : nullptr;
        if (typeInfo && typeInfo->GetEnumValueCount() > 0) {
            const int value = *static_cast<const int *>(address);
            for (asUINT index = 0; index < typeInfo->GetEnumValueCount(); ++index) {
                int enumValue = 0;
                const char *enumName = typeInfo->GetEnumValueByIndex(index, &enumValue);
                if (enumValue == value && enumName) {
                    return std::string(enumName) + " (" + std::to_string(value) + ")";
                }
            }
            return std::to_string(value);
        }

        void *objectPointer = ResolveObjectPointer(address, typeId, directObject);
        if (!objectPointer) {
            return "null";
        }
        if (IsStringType(typeInfo)) {
            return "\"" + *static_cast<const std::string *>(objectPointer) + "\"";
        }
        if (IsArrayType(typeInfo)) {
            const auto *array = static_cast<const CScriptArray *>(objectPointer);
            return "array[" + std::to_string(array->GetSize()) + "]";
        }

        const std::string typeName = GetTypeName(engine, typeId);
        return typeName.empty() ? "{object}" : "{" + typeName + "}";
    }

    bool CanExpandValueLocked(
        asIScriptEngine *engine,
        void *address,
        const int typeId,
        const bool directObject = false) const {
        LogScope scope;
        void *objectPointer = ResolveObjectPointer(address, typeId, directObject);
        if (!objectPointer || !engine) {
            return false;
        }
        asITypeInfo *typeInfo = engine->GetTypeInfoById(typeId);
        if (!typeInfo || IsStringType(typeInfo)) {
            return false;
        }
        return IsArrayType(typeInfo) ||
            (typeInfo->GetFlags() & asOBJ_SCRIPT_OBJECT) != 0 ||
            typeInfo->GetPropertyCount() > 0;
    }

    void AppendVariableLocked(
        JSON &variables,
        const std::string &name,
        asIScriptContext *context,
        void *address,
        const int typeId,
        const bool directObject = false) {
        LogScope scope;
        asIScriptEngine *engine = context ? context->GetEngine() : nullptr;
        int childReference = 0;
        if (CanExpandValueLocked(engine, address, typeId, directObject)) {
            childReference = AddVariableReferenceLocked({
                .kind = VariableReferenceKind::ValueChildren,
                .context = context,
                .address = address,
                .typeId = typeId,
                .directObject = directObject,
            });
        }

        JSON variable = {
            {"name", name},
            {"value", FormatValueLocked(engine, address, typeId, directObject)},
            {"type", GetTypeName(engine, typeId)},
            {"variablesReference", childReference},
        };
        if (childReference != 0) {
            asITypeInfo *typeInfo = engine->GetTypeInfoById(typeId);
            if (IsArrayType(typeInfo)) {
                auto *array = static_cast<CScriptArray *>(
                    ResolveObjectPointer(address, typeId, directObject));
                variable["indexedVariables"] = static_cast<int>(array->GetSize());
            } else if (typeInfo) {
                variable["namedVariables"] = static_cast<int>(typeInfo->GetPropertyCount());
            }
        }
        variables.push_back(std::move(variable));
    }

    void AppendLocalVariablesLocked(const VariableReference &reference, JSON &variables) {
        LogScope scope;
        asIScriptContext *context = reference.context;
        const int variableCount = context->GetVarCount(reference.stackLevel);
        for (int index = 0; index < variableCount; ++index) {
            if (!context->IsVarInScope(static_cast<asUINT>(index), reference.stackLevel)) {
                continue;
            }
            const char *name = nullptr;
            int typeId = asTYPEID_VOID;
            if (context->GetVar(
                    static_cast<asUINT>(index), reference.stackLevel, &name, &typeId) < 0 ||
                !name || name[0] == '\0') {
                continue;
            }
            void *address = context->GetAddressOfVar(
                static_cast<asUINT>(index), reference.stackLevel, false, true);
            AppendVariableLocked(variables, name, context, address, typeId);
        }
    }

    void AppendGlobalVariablesLocked(const VariableReference &reference, JSON &variables) {
        LogScope scope;
        if (!reference.module) {
            return;
        }
        const asUINT variableCount = reference.module->GetGlobalVarCount();
        for (asUINT index = 0; index < variableCount; ++index) {
            const char *name = nullptr;
            int typeId = asTYPEID_VOID;
            if (reference.module->GetGlobalVar(index, &name, nullptr, &typeId) < 0 ||
                !name || name[0] == '\0') {
                continue;
            }
            AppendVariableLocked(
                variables,
                name,
                reference.context,
                reference.module->GetAddressOfGlobalVar(index),
                typeId);
        }
    }

    void AppendValueChildrenLocked(const VariableReference &reference, JSON &variables) {
        LogScope scope;
        asIScriptContext *context = reference.context;
        asIScriptEngine *engine = context ? context->GetEngine() : nullptr;
        if (!engine) {
            return;
        }
        asITypeInfo *typeInfo = engine->GetTypeInfoById(reference.typeId);
        void *objectPointer = ResolveObjectPointer(
            reference.address, reference.typeId, reference.directObject);
        if (!typeInfo || !objectPointer) {
            return;
        }

        if (IsArrayType(typeInfo)) {
            auto *array = static_cast<CScriptArray *>(objectPointer);
            const int elementTypeId = array->GetElementTypeId();
            for (asUINT index = 0; index < array->GetSize(); ++index) {
                AppendVariableLocked(
                    variables,
                    "[" + std::to_string(index) + "]",
                    context,
                    array->At(index),
                    elementTypeId);
            }
            return;
        }

        if ((typeInfo->GetFlags() & asOBJ_SCRIPT_OBJECT) != 0) {
            auto *scriptObject = static_cast<asIScriptObject *>(objectPointer);
            for (asUINT index = 0; index < scriptObject->GetPropertyCount(); ++index) {
                const char *name = scriptObject->GetPropertyName(index);
                AppendVariableLocked(
                    variables,
                    name ? name : ("[" + std::to_string(index) + "]"),
                    context,
                    scriptObject->GetAddressOfProperty(index),
                    scriptObject->GetPropertyTypeId(index));
            }
            return;
        }

        auto *objectBytes = static_cast<std::byte *>(objectPointer);
        for (asUINT index = 0; index < typeInfo->GetPropertyCount(); ++index) {
            const char *name = nullptr;
            int propertyTypeId = asTYPEID_VOID;
            int offset = 0;
            bool isReference = false;
            int compositeOffset = 0;
            bool compositeIndirect = false;
            if (typeInfo->GetProperty(
                    index, &name, &propertyTypeId, nullptr, nullptr, &offset, &isReference,
                    nullptr, &compositeOffset, &compositeIndirect) < 0) {
                continue;
            }

            void *propertyAddress = objectBytes + compositeOffset;
            if (compositeIndirect) {
                propertyAddress = *static_cast<void **>(propertyAddress);
            }
            if (!propertyAddress) {
                continue;
            }
            propertyAddress = static_cast<std::byte *>(propertyAddress) + offset;
            if (isReference) {
                propertyAddress = *static_cast<void **>(propertyAddress);
            }
            AppendVariableLocked(
                variables,
                name ? name : ("[" + std::to_string(index) + "]"),
                context,
                propertyAddress,
                propertyTypeId);
        }
    }

    void BeginStep(const StepMode mode) {
        LogScope scope;
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
        LogScope scope;
        {
            std::scoped_lock lock(stateMutex_);
            resumeRequested_ = true;
            variableReferences_.clear();
            nextVariableReference_ = 1;
        }
        resumeCondition_.notify_all();
    }

    void OnLine(asIScriptContext *context) {
        LogScope scope;
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
            variableReferences_.clear();
            nextVariableReference_ = 1;
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
        LogScope scope;
        const auto found = clientSourcePaths_.find(MakeSourceKey(section));
        return found == clientSourcePaths_.end() ? std::string{} : found->second;
    }

    void ClearClientState() {
        LogScope scope;
        std::scoped_lock lock(stateMutex_);
        breakpoints_.clear();
        clientSourcePaths_.clear();
        stoppedContext_ = nullptr;
        resumeRequested_ = false;
        stepMode_ = StepMode::None;
        stepDepth_ = 0;
        stepSource_.clear();
        stepLine_ = 0;
        variableReferences_.clear();
        nextVariableReference_ = 1;
    }

    void SendResponse(const JSON &request, JSON body = JSON::object()) {
        LogScope scope;
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
        LogScope scope;
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
        LogScope scope;
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
        LogScope scope;
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
    std::unordered_map<int, VariableReference> variableReferences_;
    int nextVariableReference_ = 1;
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
    LogScope scope;
    return impl_->Start(port);
}

void AngelScriptDebugServer::Stop() {
    LogScope scope;
    impl_->Stop();
}

void AngelScriptDebugServer::AttachContext(asIScriptContext *context) const {
    LogScope scope;
    impl_->AttachContext(context);
}

AngelScriptDebugServer &GetProcessAngelScriptDebugServer() {
    LogScope scope;
    static AngelScriptDebugServer server;
    return server;
}

} // namespace KashipanEngine
