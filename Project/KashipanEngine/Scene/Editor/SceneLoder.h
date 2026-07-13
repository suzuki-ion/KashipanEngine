#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief シーンの読込メニュー（モーダルポップアップ）
class SceneLoader final {
public:
    SceneLoader(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneLoader() = default;

    /// @brief 読込ポップアップを開く
    void Open();

    /// @brief ポップアップの描画（毎フレーム呼ぶ）
    /// @return シーンが読み込まれた場合は true（呼び出し側で選択状態や履歴のクリアを行う）
    bool ShowImGui();

private:
    /// @brief シーンファイル一覧を再取得する
    void RefreshFileList();

    SceneEditorContext *context_ = nullptr;
    std::string filePath_;
    std::vector<std::string> sceneFiles_;
    bool isOpenRequested_ = false;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
