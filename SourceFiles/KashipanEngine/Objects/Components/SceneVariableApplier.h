#pragma once
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/ParameterBinding.h"

namespace KashipanEngine {

/// @brief シーン変数の値を、同オブジェクトの他コンポーネントのパラメータへ適用するコンポーネント
/// @details 毎フレーム指定した名前のシーン変数（通常またはグローバル）を読み取り、値をバインド先
///          （複数可）へそのまま書き込む。InputCommandApplier/KeyFrameAnimatorがfloat値のみを扱うのに対し、
///          このコンポーネントはシーン変数が持つ型（Bool/Int/Float/Double/String/Vector2/3/4/Quaternion/Matrix4x4）
///          をそのままバインド先へ適用できる。適用先の仕様は ParameterBinding / ApplyParameterBindingValue を参照。
class SceneVariableApplier final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(SceneVariableApplier, 0xFF, )
    COMPONENT_CATEGORY("Utility")
    ~SceneVariableApplier() override = default;

    /// @brief シーン変数のスコープ
    enum class VariableScope : int {
        Scene = 0, ///< 現在のシーン内でのみ有効なシーン変数
        Global,    ///< シーンをまたいで保持されるグローバルシーン変数
    };

    std::unique_ptr<IObjectComponent> Clone() const override;

    void SetVariableName(const std::string &name) { variableName_ = name; }
    const std::string &GetVariableName() const noexcept { return variableName_; }
    void SetVariableScope(VariableScope scope) noexcept { variableScope_ = scope; }
    VariableScope GetVariableScope() const noexcept { return variableScope_; }
    /// @brief 前回のUpdateで値の適用に（1件以上）成功したかどうか
    bool WasApplied() const noexcept { return wasApplied_; }

protected:
    void Update() override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

private:
    std::string variableName_;
    VariableScope variableScope_ = VariableScope::Scene;
    /// @brief 値の適用先（複数可）
    std::vector<ParameterBinding> bindings_;

    // --- 実行時状態（保存されない） ---
    bool wasApplied_ = false;
};

REGISTER_COMPONENT_OBJECT(SceneVariableApplier)

} // namespace KashipanEngine
