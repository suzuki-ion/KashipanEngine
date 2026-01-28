#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class ScoreManager;

/// @brief プレイヤーのスコアを表示するUIコンポーネント
class ScoreUI final : public ISceneComponent {
public:
    /// @brief スコアUIコンポーネントを作成
    ScoreUI()
        : ISceneComponent("ScoreUI", 1) {}

    ~ScoreUI() override = default;

    /// @brief ScoreManagerをバインド
    /// @param scoreManager バインドするScoreManagerオブジェクト
    void SetScoreManager(ScoreManager* scoreManager) {
        scoreManager_ = scoreManager;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    ScoreManager* scoreManager_ = nullptr;
};

} // namespace KashipanEngine
