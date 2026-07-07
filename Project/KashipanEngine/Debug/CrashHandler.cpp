#include "CrashHandler.h"
#include "Debug/CrashHandler/ExportCrashSceneBackup.h"
#include "Debug/CrashHandler/ExportDump.h"

namespace KashipanEngine {

LONG __stdcall CrashHandler(EXCEPTION_POINTERS *exceptionInfo) {
    LogScope scope;
    Log(Translation("engine.crashhandler.crash.detected"), LogSeverity::Critical);
    ExportDumpFile(exceptionInfo);
    ExportCrashSceneBackup(PasskeyForCrashHandler{});

    ForceShutdownLogger({});
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace KashipanEngine