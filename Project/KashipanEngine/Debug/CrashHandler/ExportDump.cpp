#include "ExportDump.h"
#include "Utilities/TimeUtils.h"
#include "Core/ProjectPaths.h"
#include "Utilities/FileIO/Directory.h"

#include <DbgHelp.h>
#include <strsafe.h>

#pragma comment(lib, "Dbghelp.lib")

namespace KashipanEngine {

void ExportDumpFile(EXCEPTION_POINTERS *exceptionPointers) {
    LogScope scope;
    Log(Translation("engine.crashhandler.crash.export.dump.start"), LogSeverity::Error);
    
    auto time = GetNowTime();
    const std::string dumpDirectory = ProjectPaths::InEngineRoot("Dumps");
    CreateDirectories(dumpDirectory);
    std::string dumpFilePath = dumpDirectory + "/";
    dumpFilePath += std::to_string(time.year) + "-";
    dumpFilePath += std::to_string(time.month) + "-";
    dumpFilePath += std::to_string(time.day) + "_";
    dumpFilePath += std::to_string(time.hour) + "-";
    dumpFilePath += std::to_string(time.minute) + "-";
    dumpFilePath += std::to_string(time.second) + ".dmp";

    HANDLE hFile = CreateFileA(
        dumpFilePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        0,
        CREATE_ALWAYS,
        0,
        0
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        Log(Translation("engine.crashhandler.crash.export.dump.failed") + dumpFilePath, LogSeverity::Error);
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION dumpExceptionInfo{};
    dumpExceptionInfo.ThreadId = GetCurrentThreadId();
    dumpExceptionInfo.ExceptionPointers = exceptionPointers;
    dumpExceptionInfo.ClientPointers = TRUE;
    
    MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        MiniDumpWithDataSegs,
        exceptionPointers ? &dumpExceptionInfo : nullptr,
        nullptr,
        nullptr
    );
    CloseHandle(hFile);

    Log(Translation("engine.crashhandler.crash.export.dump.end") + dumpFilePath, LogSeverity::Error);
}

} // namespace KashipanEngine