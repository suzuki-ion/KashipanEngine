#include "ExportCrashSceneBackup.h"

#include <Windows.h>

#include "Scene/SceneManager.h"
#include "Core/ProjectPaths.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Passkeys.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void ExportCrashSceneBackup(PasskeyForCrashHandler passkey) {
    LogScope scope;
    try {
        const SceneManager *sceneManager = SceneManager::GetActiveInstance(passkey);
        const Scene *scene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
        if (!scene) {
            Log(Translation("engine.crashhandler.crash.export.scene.noscene"), LogSeverity::Warning);
            return;
        }

        Log(Translation("engine.crashhandler.crash.export.scene.start"), LogSeverity::Error);

        const std::string dumpDirectory = ProjectPaths::InEngineRoot("Dumps");
        CreateDirectories(dumpDirectory);
        auto time = GetNowTime();
        std::string filePath = dumpDirectory + "/CrashSceneBackup_";
        filePath += std::to_string(time.year) + "-";
        filePath += std::to_string(time.month) + "-";
        filePath += std::to_string(time.day) + "_";
        filePath += std::to_string(time.hour) + "-";
        filePath += std::to_string(time.minute) + "-";
        filePath += std::to_string(time.second) + ".json";

        // クラッシュハンドラ内では const_cast は行わず、const な SaveToJSON のみを使用する
        const JSON sceneJson = scene->SaveToJSON();
        if (SaveJSON(sceneJson, filePath)) {
            Log(Translation("engine.crashhandler.crash.export.scene.end") + filePath, LogSeverity::Error);
        } else {
            Log(Translation("engine.crashhandler.crash.export.scene.failed") + filePath, LogSeverity::Error);
        }
    } catch (...) {
        // クラッシュ処理中の例外はこれ以上の被害を避けるため握りつぶす
    }
}

} // namespace KashipanEngine
