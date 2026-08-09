#include "CrashHandler.h"
#include "Debug/CrashHandler/ExportCrashSceneBackup.h"
#include "Debug/CrashHandler/ExportDump.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

LONG __stdcall CrashHandler(EXCEPTION_POINTERS *exceptionInfo) {
    LogScope scope;
    Log(Translation("engine.crashhandler.crash.detected"), LogSeverity::Critical);
    ExportCrashSceneBackup(PasskeyForCrashHandler{});
    ForceShutdownLogger({});
    ExportDumpFile(exceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace KashipanEngine