#pragma once
#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

/// @brief 入力デバイス判定（キーボード＋マウス / コントローラー）のしきい値をシーンごとに上書きするシーンコンポーネント
/// @details 上書きを有効にした項目だけ EngineSettings.json（input セクション）の値を置き換える。
///          シーン破棄時（シーン切り替え・Play停止・エディターでの再読み込み）には `Scene` が
///          しきい値を EngineSettings の値へ戻すため、スクリプトから変更した値も含めて次のシーンへは持ち越されない
class InputDeviceSettings final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(InputDeviceSettings, 1, LoadDefaultValues();)
    COMPONENT_CATEGORY("Input")
    ~InputDeviceSettings() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        auto clone = std::make_unique<InputDeviceSettings>();
        clone->mouseMoveThreshold_ = mouseMoveThreshold_;
        clone->controllerStickThreshold_ = controllerStickThreshold_;
        clone->controllerTriggerThreshold_ = controllerTriggerThreshold_;
        return clone;
    }

    /// @brief 現在の設定を入力デバイス判定へ反映する
    /// @details EngineSettings の値へ戻してから、上書きが有効な項目だけを設定する
    void Apply() const;

protected:
    void Initialize() override { Apply(); }
    void Finalize() override;

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

private:
    struct OverrideValue {
        bool enabled = false;
        float value = 0.0f;
    };

    /// @brief 上書き値の初期値を EngineSettings の値にする（上書きを有効にした直後に既定値から調整できるように）
    void LoadDefaultValues();

    OverrideValue mouseMoveThreshold_;
    OverrideValue controllerStickThreshold_;
    OverrideValue controllerTriggerThreshold_;
};

REGISTER_COMPONENT_SCENE(InputDeviceSettings)

} // namespace KashipanEngine
