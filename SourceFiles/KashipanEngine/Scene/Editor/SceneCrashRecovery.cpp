#include "SceneCrashRecovery.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <filesystem>

#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

SceneCrashRecovery::SceneCrashRecovery(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {
    pendingFilePath_ = ProjectPaths::InProjectRoot("CrashRecovery/PendingCrashScene.json");

    std::error_code ec;
    if (!std::filesystem::exists(Utf8StringToPath(pendingFilePath_), ec) || ec) {
        return;
    }

    // 表示用の書き出し日時はバックアップJSON自体に埋め込まれているものを使う
    const JSON backupJson = LoadJSON(pendingFilePath_);
    timestampText_ = backupJson.value("crashRecoveryExportedAt", "");

    isPendingFound_ = true;
    isPopupRequested_ = true;
}

bool SceneCrashRecovery::ShowImGui() {
    if (!isPendingFound_) return false;

    bool restored = false;
    if (isPopupRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.crashrecovery.title"));
        isPopupRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.crashrecovery.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", TranslationC("editor.crashrecovery.message"));
        if (!timestampText_.empty()) {
            ImGui::Text("%s%s", TranslationC("editor.crashrecovery.timestamp"), timestampText_.c_str());
        }

        if (ImGui::Button(TranslationLabel("editor.crashrecovery.restore"), ImVec2(120, 0))) {
            const JSON backupJson = LoadJSON(pendingFilePath_);
            if (!backupJson.empty() && context_->LoadSceneFromJSON(backupJson)) {
                restored = true;
            }
            std::filesystem::remove(Utf8StringToPath(pendingFilePath_));
            isPendingFound_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.crashrecovery.dismiss"), ImVec2(120, 0))) {
            std::filesystem::remove(Utf8StringToPath(pendingFilePath_));
            isPendingFound_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return restored;
}

} // namespace KashipanEngine

#endif // USE_IMGUI
