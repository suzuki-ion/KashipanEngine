#include "CrashHandler.h"
#include "Debug/CrashHandler/ExportCrashSceneBackup.h"
#include "Debug/CrashHandler/ExportDump.h"
#include "Utilities/Translation.h"

#include <DbgHelp.h>
#include <sstream>
#pragma comment(lib, "Dbghelp.lib")

namespace KashipanEngine {

namespace {
// TEMP DIAGNOSTIC (2026-08-11): 起動時PostEffect.MotionBlur.Velocityパイプライン作成クラッシュの
// 原因調査のためだけに追加。原因特定後に削除すること
void LogCrashDiagnostics(EXCEPTION_POINTERS *exceptionInfo) {
    if (!exceptionInfo || !exceptionInfo->ExceptionRecord) return;
    auto *record = exceptionInfo->ExceptionRecord;

    std::stringstream ss;
    ss << "[TEMP DIAG] ExceptionCode=0x" << std::hex << record->ExceptionCode
       << " ExceptionAddress=0x" << record->ExceptionAddress;
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
        ss << " AccessType=" << std::dec << record->ExceptionInformation[0]
           << " TargetAddress=0x" << std::hex << record->ExceptionInformation[1];
    }
    Log(ss.str(), LogSeverity::Critical);

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (SymInitialize(process, nullptr, TRUE)) {
        alignas(SYMBOL_INFO) char symBuffer[sizeof(SYMBOL_INFO) + 256]{};
        SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(symBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 addr = reinterpret_cast<DWORD64>(record->ExceptionAddress);
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD lineDisplacement = 0;

        std::stringstream ss2;
        ss2 << "[TEMP DIAG] Faulting symbol: ";
        if (SymFromAddr(process, addr, &displacement, symbol)) {
            ss2 << symbol->Name << " +0x" << std::hex << displacement;
        } else {
            ss2 << "(unresolved, GetLastError=" << std::dec << GetLastError() << ")";
        }
        if (SymGetLineFromAddr64(process, addr, &lineDisplacement, &line)) {
            ss2 << " at " << line.FileName << ":" << std::dec << line.LineNumber;
        }
        Log(ss2.str(), LogSeverity::Critical);

        // 簡易スタックウォーク（x64）
        CONTEXT context = *exceptionInfo->ContextRecord;
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        std::stringstream ssStack;
        ssStack << "[TEMP DIAG] Stack:";
        for (int i = 0; i < 24; ++i) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context,
                nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
            if (frame.AddrPC.Offset == 0) break;
            DWORD64 frameAddr = frame.AddrPC.Offset;
            DWORD64 frameDisp = 0;
            ssStack << "\n  #" << i << " 0x" << std::hex << frameAddr << " ";
            if (SymFromAddr(process, frameAddr, &frameDisp, symbol)) {
                ssStack << symbol->Name << " +0x" << std::hex << frameDisp;
            } else {
                ssStack << "(unresolved)";
            }
        }
        Log(ssStack.str(), LogSeverity::Critical);
        SymCleanup(process);
    }
}
} // namespace

LONG __stdcall CrashHandler(EXCEPTION_POINTERS *exceptionInfo) {
    LogScope scope;
    Log(Translation("engine.crashhandler.crash.detected"), LogSeverity::Critical);
    LogCrashDiagnostics(exceptionInfo);
    ExportCrashSceneBackup(PasskeyForCrashHandler{});
    ForceShutdownLogger({});
    ExportDumpFile(exceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace KashipanEngine