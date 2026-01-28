#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <memory>

namespace KashipanEngine {

/// @brief 爆発時に倒した敵の数を表示するコンポーネント
class ScoreDisplay final : public ISceneComponent {
public:
    ScoreDisplay();
    ~ScoreDisplay() override = default;

    void Initialize() override;
    void Update() override;

    /// @brief ScreenBufferを設定
    void SetScreenBuffer(ScreenBuffer* screenBuffer) { screenBuffer_ = screenBuffer; }

    /// @brief ShadowMapBufferを設定
    void SetShadowMapBuffer(ShadowMapBuffer* shadowMapBuffer) { shadowMapBuffer_ = shadowMapBuffer; }

    /// @brief 指定位置に数字を表示
    /// @param position 表示する位置
    /// @param count 表示する数字（倒した敵の数）
    void SpawnNumber(const Vector3& position, int count);

    /// @brief 数字の表示時間を設定（秒）
    void SetDisplayLifetime(float lifetime) { displayLifetime_ = lifetime; }

    /// @brief 数字のスケールを設定
    void SetNumberScale(float scale) { numberScale_ = scale; }

	/// @brief Y方向のオフセットを設定
	void SetYOffset(float yOffset) { yOffset_ = yOffset; }
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    /// @brief 数字表示情報
    struct NumberDisplayInfo {
        Model* object = nullptr;
        float elapsedTime = 0.0f;
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        int number = 0;
        float initialScale = 1.0f;
    };

    ScreenBuffer* screenBuffer_ = nullptr;
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;

    std::vector<NumberDisplayInfo> activeNumbers_;

    float displayLifetime_ = 1.0f;  // 数字の表示時間（秒）
    float numberScale_ = 1.0f;      // 数字のスケール
    float yOffset_ = 1.0f;          // Y方向のオフセット
};

} // namespace KashipanEngine
