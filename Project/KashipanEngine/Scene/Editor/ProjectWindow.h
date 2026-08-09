#pragma once
#ifdef USE_IMGUI

#include <string>
#include <vector>

#include "Core/ProjectManager.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief 開いているプロジェクトの確認・新規作成・切り替えを行うパネル
/// @details エンジンは起動時に読み込んだアセットを丸ごと差し替えられないため、
///          切り替えは「次回起動時に開くプロジェクト」の変更として扱い、
///          反映にはexeの再起動が必要であることをUI上で明示する。
class ProjectWindow final {
public:
    ProjectWindow(Passkey<SceneEditor>) {}

    void ShowImGui();

private:
    /// @brief Projects配下のプロジェクト一覧を再取得する（毎フレーム走査しないようキャッシュする）
    void RefreshProjectList();

    std::vector<ProjectManager::ProjectInfo> projects_;
    bool hasScannedProjects_ = false;
    std::string newProjectNameBuffer_;
    /// @brief 新規作成に失敗した場合の理由（成功時は空）
    std::string lastErrorMessage_;
    /// @brief 再起動後に開かれるプロジェクト名（現在開いているものと異なる場合のみ案内を出す）
    std::string pendingProjectName_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
