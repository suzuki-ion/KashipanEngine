#pragma once
#include <string>
#include <vector>

#include "Utilities/FileIO/JSON.h"
#include "Utilities/MathUtils/Easings.h"

namespace KashipanEngine {

/// @brief floatの値が時間で遷移するだけの汎用キーフレームアニメーション
/// @details キーは「時刻(秒)・値・次のキーへ遷移する際のイージング」の組。
///          どのパラメータにも紐づかない純粋な値の遷移で、評価結果をどこへ適用するかは
///          利用側（KeyFrameAnimatorコンポーネント等）が決める。
///          エディターツールの Keyframe Animation Editor（EditorTools/KeyframeAnimationEditor.as）で
///          このクラスのLoadFromJsonが読み込める形式のjsonファイルを作成できる。
class KeyframeAnimation {
public:
    struct Keyframe {
        /// @brief キーの時刻（秒）
        float time = 0.0f;
        /// @brief キーの値
        float value = 0.0f;
        /// @brief このキーから次のキーへ遷移する際のイージング（最後のキーでは未使用）
        EaseType easeType = EaseType::Linear;
    };

    void Clear() { keyframes_.clear(); }
    /// @brief キーを追加する（時刻昇順の位置へ挿入される）
    void AddKeyframe(float time, float value, EaseType easeType = EaseType::Linear);
    const std::vector<Keyframe> &GetKeyframes() const noexcept { return keyframes_; }
    size_t GetKeyframeCount() const noexcept { return keyframes_.size(); }
    /// @brief 最後のキーの時刻（キーが無い場合は0）
    float GetDuration() const noexcept { return keyframes_.empty() ? 0.0f : keyframes_.back().time; }

    /// @brief 指定時刻の値を評価する
    /// @details 最初のキー以前は最初のキーの値、最後のキー以降は最後のキーの値を返す。
    ///          キーが1つも無い場合は0を返す
    float Evaluate(float time) const;

    /// @brief キーフレーム情報をjsonへ保存する
    JSON SaveToJson() const;
    /// @brief jsonからキーフレーム情報を読み込む（既存のキーはクリアされ、時刻昇順へソートされる）
    /// @return 形式が正しい場合はtrue
    bool LoadFromJson(const JSON &json);

private:
    /// @brief 時刻昇順のキー列
    std::vector<Keyframe> keyframes_;
};

} // namespace KashipanEngine
