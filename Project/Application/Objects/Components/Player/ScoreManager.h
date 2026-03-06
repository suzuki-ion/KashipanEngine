#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

/// @brief プレイヤーのスコアを管理するコンポーネント
class ScoreManager final : public ISceneComponent {
public:
    ScoreManager()
        : ISceneComponent("ScoreManager", 1) {}

    ~ScoreManager() override = default;

    void Initialize() override {
        score_ = 0;
    }

    /// @brief スコアを加算
    /// @param points 加算するポイント
    void AddScore(int points) {
        score_ += points;
    }

    /// @brief 現在のスコアを取得
    /// @return 現在のスコア
    int GetScore() const {
        return score_;
    }

    /// @brief スコアをリセット
    void ResetScore() {
        score_ = 0;
    }

    /// @brief スコアを設定
    /// @param score 設定するスコア
    void SetScore(int score) {
        score_ = score;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted("ScoreManager");
        ImGui::Text("Current Score: %d", score_);
        if (ImGui::Button("Reset Score")) {
            ResetScore();
        }
    }
#endif

private:
    int score_ = 0;
};

} // namespace KashipanEngine
