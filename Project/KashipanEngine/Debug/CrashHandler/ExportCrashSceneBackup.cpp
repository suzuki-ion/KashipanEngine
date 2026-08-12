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

        // 次回起動時にこのプロジェクトを開いた際に検知・復元できるよう、プロジェクト単位の
        // 固定ファイル名で保存する（存在すること自体を「復元待ちがある」マーカーとして使う）
        const std::string recoveryDirectory = ProjectPaths::InProjectRoot("CrashRecovery");
        CreateDirectories(recoveryDirectory);
        const std::string filePath = recoveryDirectory + "/PendingCrashScene.json";

        // クラッシュハンドラ内では const_cast は行わず、const な SaveToJSON のみを使用する。
        // クラッシュ原因そのものを含んでいる可能性があるクラッシュ瞬間の生シーンではなく、
        // SceneManagerが1秒間隔で保持している直近スナップショットを優先して使う。
        // まだスナップショットが無い場合（シーン切替直後1秒未満など）のみ生シーンへフォールバックする
        const JSON &bufferedSnapshot = sceneManager->GetPreCrashSnapshot(passkey);
        JSON sceneJson = bufferedSnapshot.empty() ? scene->SaveToJSON() : bufferedSnapshot;
        // 次回起動時の確認モーダルに表示するため、書き出し日時を付与しておく
        sceneJson["crashRecoveryExportedAt"] = GetNowTimeString();
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
