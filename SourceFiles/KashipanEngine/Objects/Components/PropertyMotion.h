#pragma once
#include <string>
#include "Objects/ObjectComponentHeader.h"
#include "Utilities/ShortTextPropertyRequest.h"

namespace KashipanEngine {

/// @brief 同じオブジェクトの別コンポーネントの公開数値プロパティを、2つの絶対値の間で往復させる
/// @details 対象は「コンポーネント型・同型内の番号・登録メンバー名・成分」で指定する
///          （例: Transform / 0 / translate_ / X）。書き込みは登録済みセッター経由で行い、
///          読み取り専用・型不一致の項目には書き込まない。
///          - 再生中だけ動作する。編集中の値は変更せず、停止時はシーンの再生前状態へ戻る。
///          - 値は時刻0で開始値、周期の半分で終了値、1周期で開始値へ戻る（ローカル絶対値）。
///          - 単位を「度」にした場合は、開始値・終了値を度として扱いラジアンへ変換して書き込む。
///          短文の挙動から作られたものは shortTextManaged が true になり、同じ項目への再適用時に更新される。
class PropertyMotion final : public IObjectComponent {
public:
    struct Settings {
        std::string component = "Transform";
        int componentIndex = 0;
        std::string member = "translate_";
        int axis = 0; // -1: 単一の数値
        float from = -1.0f;
        float to = 1.0f;
        float period = ShortText::PropertyDefaultPeriod; // 1往復の秒数
        ShortText::PropertyUnit unit = ShortText::PropertyUnit::Raw;
        ShortText::PropertyWave wave = ShortText::PropertyWave::Linear;
        bool shortTextManaged = false;
        std::string sourceText;
    };

    OBJECT_COMPONENT_CONSTRUCTOR(PropertyMotion, 0xFF,
        AddMemberProperty<float>("from_", &settings_.from, [this](const float &v) { auto next = settings_; next.from = v; return SetSettings(next); });
        AddMemberProperty<float>("to_", &settings_.to, [this](const float &v) { auto next = settings_; next.to = v; return SetSettings(next); });
        AddMemberProperty<float>("period_", &settings_.period, [this](const float &v) { auto next = settings_; next.period = v; return SetSettings(next); });
    )
    COMPONENT_CATEGORY("Animation")
    ~PropertyMotion() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override;

    static bool IsValid(const Settings &settings);
    const Settings &GetSettings() const noexcept { return settings_; }
    /// @brief 検証に通った設定だけを反映する（失敗時は変更しない）。再生位置は先頭へ戻る
    bool SetSettings(const Settings &settings);
    /// @brief 対象（型・番号・メンバー・成分）が同じか
    bool HasSameTarget(const Settings &other) const noexcept {
        return settings_.component == other.component && settings_.componentIndex == other.componentIndex &&
            settings_.member == other.member && settings_.axis == other.axis;
    }
    /// @brief 経過時間を進めて対象へ書き込む（Updateが再生中だけ呼ぶ。テストからも直接呼べる）
    bool Advance(double dt);
    /// @brief 再生位置を先頭へ戻す
    void ResetTime() noexcept { elapsed_ = 0; }
    double GetElapsed() const noexcept { return elapsed_; }
    /// @brief 現在時刻の値（度指定の場合は度）
    float GetCurrentValue() const;
    /// @brief 書き込みに使うプロパティパス（例: "Transform.translate_.x"）
    std::string GetTargetPath() const;
    const std::string &GetLastError() const noexcept { return lastError_; }

protected:
    void Initialize() override;
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    Settings settings_;
    // --- 実行時状態（保存されない） ---
    double elapsed_ = 0;
    bool playing_ = false; // 再生開始の検出用（開始時に先頭から書き込む）
    std::string lastError_;
};

REGISTER_COMPONENT_OBJECT(PropertyMotion)

/// @brief Add/ComponentEditCommandで使うコンポーネント状態（priority/isActive/tag/customData）
JSON MakePropertyMotionState(const PropertyMotion::Settings &settings);
/// @brief customData形式のJSONを設定へ変換する（不正な場合はfalse）
bool DecodePropertyMotionSettings(const JSON &json, PropertyMotion::Settings &settings);
JSON EncodePropertyMotionSettings(const PropertyMotion::Settings &settings);
/// @brief 登録メンバーの存在・書き込み可否・数値成分数（float/double=1, Vector2/3/4）を調べる
ShortText::PropertyMemberInfo InspectPropertyMember(const IObjectComponent *component, const std::string &member);
/// @brief オブジェクト上で指定型のindex番目のコンポーネントを探す（無ければnullptr）
IObjectComponent *FindComponentOfType(const EmptyObject *object, const std::string &type, int index = 0);

} // namespace KashipanEngine
