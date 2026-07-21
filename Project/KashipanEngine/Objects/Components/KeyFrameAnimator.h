#pragma once
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/ParameterBinding.h"
#include "Utilities/KeyframeAnimation.h"

namespace KashipanEngine {

/// @brief キーフレームアニメーション(json)のfloat値を、同オブジェクトの他コンポーネントのパラメータへ適用するコンポーネント
/// @details アニメーションは KeyframeAnimation::LoadFromJson 形式のjsonファイルから読み込む
///          （エディターツールの Keyframe Animation Editor で作成できる）。
///          複数のアニメーションを登録でき、それぞれに再生速度・時間オフセット・値オフセットを設定できる。
///          再生中のアニメーションの評価値は、各アニメーションに設定した適用先（複数可）へ毎フレーム書き込まれる。
///          適用先は以下の2種類:
///          - 他コンポーネントの ADD_MEMBER_VARIABLE で登録されたメンバ変数
///            （float/doubleはそのまま、Vector2/3/4は成分を指定。書き込み後はonModifiedコールバックが呼ばれる）
///          - ScriptComponent の [SerializeField] 付き float 変数
class KeyFrameAnimator final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(KeyFrameAnimator, 0xFF, )
    COMPONENT_CATEGORY("Animation")
    ~KeyFrameAnimator() override = default;

    /// @brief アニメーション値の適用先1件分（ParameterBinding.h の共通型）
    using TargetBinding = ParameterBinding;

    /// @brief アニメーション1本分の設定と再生状態
    struct AnimationEntry {
        std::string name;             ///< 識別名（Play/Stopで指定する名前）
        std::string jsonPath;         ///< キーフレームjsonのパス（実行ディレクトリ基準の相対パスまたは絶対パス）
        float playbackSpeed = 1.0f;   ///< 再生速度倍率（負の値で逆再生）
        float timeOffset = 0.0f;      ///< 評価時刻へ加算する再生時オフセット（秒）
        float valueScale = 1.0f;      ///< 評価値にかけるスケール（valueOffset加算より先に適用される）
        float valueOffset = 0.0f;     ///< 評価値へ加算するデフォルト値オフセット
        bool loop = true;             ///< ループ再生するか（無効の場合は末端に到達すると自動停止する）
        bool playOnStart = false;     ///< ゲームループ開始時に自動で再生を開始するか
        std::vector<TargetBinding> bindings; ///< 評価値の適用先（複数可）

        // --- 実行時状態（保存されない） ---
        KeyframeAnimation animation;
        bool loaded = false;
        bool loadFailed = false;
        bool playing = false;
        float currentTime = 0.0f;
        float lastValue = 0.0f;
    };

    std::unique_ptr<IObjectComponent> Clone() const override;

    //==================================================
    // 再生制御
    //==================================================

    /// @brief 指定した名前のアニメーションを先頭から再生する
    /// @return 名前が一致するアニメーションが見つかった場合は true
    bool Play(const std::string &name);
    /// @brief 指定した名前のアニメーションを停止する（再生位置は保持される）
    bool Stop(const std::string &name);
    /// @brief 全アニメーションを先頭から再生する
    void PlayAll();
    /// @brief 全アニメーションを停止する
    void StopAll();
    bool IsPlaying(const std::string &name) const;
    bool SetPlaybackSpeed(const std::string &name, float speed);
    float GetPlaybackSpeed(const std::string &name) const;
    /// @brief 最後に評価された値（valueOffset適用後）を取得する
    bool TryGetValue(const std::string &name, float &outValue) const;
    size_t GetAnimationCount() const noexcept { return animations_.size(); }

protected:
    void Initialize() override;
    void Update() override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

private:
    AnimationEntry *FindAnimation(const std::string &name);
    const AnimationEntry *FindAnimation(const std::string &name) const;
    /// @brief jsonPathからキーフレームを読み込む（未読み込みかつ失敗履歴なしの場合のみ）
    void EnsureLoaded(AnimationEntry &entry);
    /// @brief 評価値をエントリの全バインド先へ書き込む
    void ApplyValue(const AnimationEntry &entry, float value);

#if defined(USE_IMGUI)
    void ShowAnimationImGui(AnimationEntry &entry, const std::vector<ParameterBindingCandidate> &candidates);
#endif

    std::vector<AnimationEntry> animations_;
    /// @brief 次のUpdateでplayOnStartの適用を行うか（Initialize後、最初のUpdateで一度だけ）
    bool applyPlayOnStart_ = false;
};

REGISTER_COMPONENT_OBJECT(KeyFrameAnimator)

} // namespace KashipanEngine
