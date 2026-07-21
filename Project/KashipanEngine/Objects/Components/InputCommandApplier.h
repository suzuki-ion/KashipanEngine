#pragma once
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/ParameterBinding.h"

namespace KashipanEngine {

/// @brief InputCommandの入力を、同オブジェクトの他コンポーネントのパラメータへ適用するコンポーネント
/// @details 毎フレーム指定した入力コマンドを評価し、条件（コマンドのtrue/false、入力値と閾値の比較）を
///          満たしたときだけ、値（固定値または入力コマンドの値）をバインド先へ書き込む。
///          書き込む値にはスケールとオフセットをかけられる（適用値 = 元の値 × Scale + Offset）。
///          1つのオブジェクトへ複数アタッチでき、コマンド・条件ごとに別々の適用が行える。
///          適用先の仕様は ParameterBinding を参照（KeyFrameAnimatorと共通）。
class InputCommandApplier final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(InputCommandApplier, 0xFF,
        ADD_MEMBER_VARIABLE(threshold_);
        ADD_MEMBER_VARIABLE(fixedValue_);
        ADD_MEMBER_VARIABLE(valueScale_);
        ADD_MEMBER_VARIABLE(valueOffset_);
    )
    COMPONENT_CATEGORY("Input")
    ~InputCommandApplier() override = default;

    /// @brief 値を適用する条件
    enum class ConditionType : int {
        CommandTrue = 0,   ///< コマンドの評価結果がtrue（トリガー中）のとき
        CommandFalse,      ///< コマンドの評価結果がfalseのとき
        ValueGreaterEqual, ///< 入力値が閾値以上のとき
        ValueLessEqual,    ///< 入力値が閾値以下のとき
        ValueEqual,        ///< 入力値が閾値と等しいとき（誤差1e-6以内）
    };

    /// @brief バインド先へ書き込む値の種類
    enum class ValueSource : int {
        CommandValue = 0,  ///< 入力コマンドの評価値
        FixedValue,        ///< 固定値
    };

    std::unique_ptr<IObjectComponent> Clone() const override;

    void SetCommandName(const std::string &name) { commandName_ = name; }
    const std::string &GetCommandName() const noexcept { return commandName_; }
    void SetConditionType(ConditionType type) noexcept { conditionType_ = type; }
    ConditionType GetConditionType() const noexcept { return conditionType_; }
    void SetThreshold(float threshold) noexcept { threshold_ = threshold; }
    float GetThreshold() const noexcept { return threshold_; }
    void SetValueSource(ValueSource source) noexcept { valueSource_ = source; }
    ValueSource GetValueSource() const noexcept { return valueSource_; }
    void SetFixedValue(float value) noexcept { fixedValue_ = value; }
    float GetFixedValue() const noexcept { return fixedValue_; }
    void SetValueScale(float scale) noexcept { valueScale_ = scale; }
    float GetValueScale() const noexcept { return valueScale_; }
    void SetValueOffset(float offset) noexcept { valueOffset_ = offset; }
    float GetValueOffset() const noexcept { return valueOffset_; }
    /// @brief 前回のUpdateで条件を満たして値を適用したかどうか
    bool WasApplied() const noexcept { return wasApplied_; }
    /// @brief 前回適用した値（Scale/Offset適用後。未適用の間は前回値を保持する）
    float GetLastValue() const noexcept { return lastValue_; }

protected:
    void Update() override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

private:
    std::string commandName_;
    ConditionType conditionType_ = ConditionType::CommandTrue;
    /// @brief 入力値と比較する閾値（ValueGreaterEqual/ValueLessEqual/ValueEqualで使用）
    float threshold_ = 0.5f;
    ValueSource valueSource_ = ValueSource::CommandValue;
    /// @brief ValueSource::FixedValueのときに書き込む固定値
    float fixedValue_ = 1.0f;
    /// @brief 書き込む値にかけるスケール（オフセット加算より先に適用される）
    float valueScale_ = 1.0f;
    /// @brief 書き込む値へ加算するデフォルト値オフセット
    float valueOffset_ = 0.0f;
    /// @brief 値の適用先（複数可）
    std::vector<ParameterBinding> bindings_;

    // --- 実行時状態（保存されない） ---
    bool wasApplied_ = false;
    float lastValue_ = 0.0f;
};

REGISTER_COMPONENT_OBJECT(InputCommandApplier)

} // namespace KashipanEngine
