#pragma once
#ifdef USE_IMGUI
#include <string>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief シーンの保存メニュー（モーダルポップアップ）
class SceneSaver final {
public:
    enum class Result {
        None,
        Saved,
        Cancelled,
    };

    SceneSaver(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneSaver() = default;

    /// @brief 保存ポップアップを開く
    void Open();

    /// @brief ポップアップの描画（毎フレーム呼ぶ）
    Result ShowImGui();

    /// @brief 保存成功時に監視側へ通知するための保存先パス
    const std::string &GetFilePath() const { return filePath_; }

private:
    SceneEditorContext *context_ = nullptr;
    std::string filePath_;
    bool isOpenRequested_ = false;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
