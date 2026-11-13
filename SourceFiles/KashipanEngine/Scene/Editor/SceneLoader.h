#pragma once
#ifdef USE_IMGUI
#include <string>
#include <vector>
#include <functional>
#include <utility>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief シーンの読込メニュー（モーダルポップアップ）
class SceneLoader final {
public:
    SceneLoader(Passkey<SceneEditor>, SceneEditorContext *context, std::function<void(JSON)> loadRequest)
        : context_(context), loadRequest_(std::move(loadRequest)) {}
    ~SceneLoader() = default;

    /// @brief 読込ポップアップを開く
    void Open();

    /// @brief ポップアップの描画（毎フレーム呼ぶ）
    void ShowImGui();

private:
    /// @brief シーンファイル一覧を再取得する
    void RefreshFileList();

    SceneEditorContext *context_ = nullptr;
    std::string filePath_;
    std::vector<std::string> sceneFiles_;
    bool isOpenRequested_ = false;
    std::function<void(JSON)> loadRequest_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
