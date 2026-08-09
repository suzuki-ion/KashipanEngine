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
///          1つのコンポーネントに複数の入力コマンド設定（コマンド名・条件・バインド先）を登録でき、
///          KeyFrameAnimatorと同様に名前で識別する（1コンポーネントにつき1つに限定しない）。
///          適用先の仕様は ParameterBinding を参照（KeyFrameAnimatorと共通）。
class InputCommandApplier final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(InputCommandApplier, 0xFF, )
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

    /// @brief 入力コマンド設定1件分（識別名・評価するコマンド・条件・バインド先）
    struct CommandEntry {
        std::string name;           ///< 識別名（WasApplied/GetLastValue等で指定する名前）
        std::string commandName;    ///< 評価する入力コマンド名（InputCommandに登録済みのコマンド）
        ConditionType conditionType = ConditionType::CommandTrue;
        /// @brief 入力値と比較する閾値（ValueGreaterEqual/ValueLessEqual/ValueEqualで使用）
        float threshold = 0.5f;
        ValueSource valueSource = ValueSource::CommandValue;
        /// @brief ValueSource::FixedValueのときに書き込む固定値
        float fixedValue = 1.0f;
        /// @brief 書き込む値にかけるスケール（オフセット加算より先に適用される）
        float valueScale = 1.0f;
        /// @brief 書き込む値へ加算するデフォルト値オフセット
        float valueOffset = 0.0f;
        /// @brief 値の適用先（複数可）
        std::vector<ParameterBinding> bindings;

        // --- 実行時状態（保存されない） ---
        bool wasApplied = false;
        float lastValue = 0.0f;
    };

    std::unique_ptr<IObjectComponent> Clone() const override;

    /// @brief 指定した名前のコマンド設定が、前回のUpdateで条件を満たして値を適用したかどうか
    bool WasApplied(const std::string &name) const;
    /// @brief 指定した名前のコマンド設定が、前回適用した値を取得する（Scale/Offset適用後。未適用の間は前回値を保持する）
    /// @return 名前が一致するコマンド設定が見つかった場合は true
    bool TryGetLastValue(const std::string &name, float &outValue) const;
    size_t GetCommandCount() const noexcept { return commands_.size(); }

protected:
    void Update() override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

private:
    CommandEntry *FindCommand(const std::string &name);
    const CommandEntry *FindCommand(const std::string &name) const;
    /// @brief 評価値をエントリの全バインド先へ書き込む
    void ApplyValue(const CommandEntry &entry, float value);

#if defined(USE_IMGUI)
    void ShowCommandImGui(CommandEntry &entry, const std::vector<ParameterBindingCandidate> &candidates);
#endif

    std::vector<CommandEntry> commands_;
};

REGISTER_COMPONENT_OBJECT(InputCommandApplier)

} // namespace KashipanEngine
