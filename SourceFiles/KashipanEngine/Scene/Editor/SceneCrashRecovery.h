#pragma once
#ifdef USE_IMGUI
#include <string>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief 前回セッションのクラッシュ復元の確認モーダル
class SceneCrashRecovery final {
public:
    SceneCrashRecovery(Passkey<SceneEditor>, SceneEditorContext *context);
    ~SceneCrashRecovery() = default;

    /// @brief ポップアップの描画（毎フレーム呼ぶ）
    /// @return このフレームでシーンが復元された場合は true（呼び出し側で選択状態や履歴のクリアを行う）
    bool ShowImGui();

private:
    SceneEditorContext *context_ = nullptr;

    /// @brief 復元待ちのバックアップが見つかっているか
    bool isPendingFound_ = false;
    /// @brief モーダルの表示要求
    bool isPopupRequested_ = false;
    /// @brief 復元待ちバックアップの物理パス
    std::string pendingFilePath_;
    /// @brief 表示用のバックアップ書き出し日時（crashRecoveryExportedAt）
    std::string timestampText_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
