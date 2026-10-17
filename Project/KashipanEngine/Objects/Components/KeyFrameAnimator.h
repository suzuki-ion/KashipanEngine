#pragma once
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/ParameterBinding.h"
#include "Utilities/KeyframeAnimation.h"

namespace KashipanEngine {

/// @brief キーフレームアニメーション(json)のfloat値を、同オブジェクトの他コンポーネントのパラメータへ適用するコンポーネント
/// @details アニメーションは KeyframeAnimation::LoadFromJson 形式のjsonファイルから読み込むか
///          （エディターツールの Keyframe Animation Editor で作成できる）、
///          キーフレームをエントリ内に埋め込んで（embedded）シーンへ直接保存する。
///          埋め込みエントリは AddAnimationData/SetAnimationData で登録し、短文の一回変化（Tween）もこの形式で生成される。
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
        /// @brief キーフレームをこのエントリ内に保持するか（true の場合 jsonPath は使わず、animation がそのまま保存される）
        bool embedded = false;
        /// @brief 短文から生成されたエントリか（同じ適用先への再適用時はこのエントリだけが更新・置換される）
        bool shortTextManaged = false;
        std::string sourceText;       ///< 生成元の文章（shortTextManaged の場合）

        // --- 実行時状態（保存されない。embedded の場合 animation は保存対象） ---
        KeyframeAnimation animation;
        bool loaded = false;
        bool loadFailed = false;
        bool playing = false;
        /// @brief 再生開始直後か（埋め込みエントリは開始フレームで時刻0の値を書き込んでから進める）
        bool startPending = false;
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
    float GetAnimationTime(const std::string &name) const;
    bool SetCurrentTime(const std::string &name, float time);
    float GetDuration(const std::string &name) const;
    /// @brief 最後に評価された値（valueOffset適用後）を取得する
    bool TryGetValue(const std::string &name, float &outValue) const;
    size_t GetAnimationCount() const noexcept { return animations_.size(); }

    //==================================================
    // 設定（スクリプト/エディターツールからの構築用）
    //==================================================

    /// @brief アニメーションを追加する（再生速度等はデフォルト値、適用先は空で追加される）
    /// @return 名前が空、または同名のアニメーションが既にある場合は false
    bool AddAnimation(const std::string &name, const std::string &jsonPath);
    /// @brief キーフレームデータ（KeyframeAnimation::SaveToJson 形式）を埋め込んだアニメーションを追加する
    /// @return 名前が空・重複している場合、またはデータが不正な場合は false（何も追加しない）
    bool AddAnimationData(const std::string &name, const JSON &keyframesJson);
    /// @brief 既存アニメーションのキーフレームを埋め込みデータで置き換える（ファイル参照から埋め込みへ切り替わる）
    /// @return アニメーションが無い・データが不正な場合は false（以前の状態を保持する）
    bool SetAnimationData(const std::string &name, const JSON &keyframesJson);
    /// @brief キーフレームが埋め込みデータか（false の場合は jsonPath のファイルを参照する）
    bool IsEmbedded(const std::string &name) const;
    /// @brief 読み込み済みのキーフレーム（埋め込み、またはファイルから読み込み済みのもの）。無い場合は nullptr
    const KeyframeAnimation *GetAnimationData(const std::string &name) const;
    /// @brief 短文由来の印と生成元の文章を設定する（文章は512バイトまで）
    bool SetShortTextSource(const std::string &name, bool managed, const std::string &sourceText);
    bool IsShortTextManaged(const std::string &name) const;
    std::vector<std::string> GetAnimationNames() const;
    /// @brief 適用先一覧（アニメーションが無い場合は nullptr）
    const std::vector<TargetBinding> *GetBindings(const std::string &name) const;
    /// @brief エントリ1件をシーン保存と同じ形式で取得する（アニメーションが無い場合は null）
    JSON SaveAnimationEntryToJson(const std::string &name) const;
    /// @brief 指定した名前のアニメーションを削除する
    bool RemoveAnimation(const std::string &name);
    /// @brief 全アニメーションを削除する
    void ClearAnimations() { animations_.clear(); }
    bool HasAnimation(const std::string &name) const { return FindAnimation(name) != nullptr; }

    /// @brief キーフレームjsonのパスを変更する（次回の再生時に読み込み直される。埋め込みエントリはファイル参照へ戻る）
    bool SetJsonPath(const std::string &name, const std::string &jsonPath);
    std::string GetJsonPath(const std::string &name) const;
    bool SetLoop(const std::string &name, bool loop);
    bool GetLoop(const std::string &name) const;
    bool SetPlayOnStart(const std::string &name, bool playOnStart);
    bool GetPlayOnStart(const std::string &name) const;
    bool SetTimeOffset(const std::string &name, float timeOffset);
    float GetTimeOffset(const std::string &name) const;
    bool SetValueScale(const std::string &name, float valueScale);
    float GetValueScale(const std::string &name) const;
    bool SetValueOffset(const std::string &name, float valueOffset);
    float GetValueOffset(const std::string &name) const;

    /// @brief 評価値の適用先を追加する
    /// @param name アニメーション名
    /// @param componentType 適用先コンポーネントの型名（例: "Transform"）。isScriptVariable の場合は無視される
    /// @param parameterName メンバ変数名（例: "translate_"）またはスクリプト変数名
    /// @param channel Vector2/3/4の成分 (0=x, 1=y, 2=z, 3=w)。float/doubleでは無視
    /// @param componentIndex 同型コンポーネントが複数ある場合のインデックス（追加順）
    /// @param isScriptVariable ScriptComponentの[SerializeField]変数への適用かどうか
    bool AddBinding(const std::string &name, const std::string &componentType, const std::string &parameterName,
        int channel = 0, int componentIndex = 0, bool isScriptVariable = false);
    /// @brief 指定したアニメーションの適用先を全て削除する
    bool ClearBindings(const std::string &name);
    /// @brief 指定したアニメーションの適用先の数（アニメーションが無い場合は0）
    size_t GetBindingCount(const std::string &name) const;

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
    /// @brief 再生開始状態にする（Play/PlayAll/playOnStart 共通）
    static void StartEntry(AnimationEntry &entry);
    static JSON SaveEntryToJson(const AnimationEntry &entry);
    /// @brief エントリ1件を厳密に検証して読み込む（不正な場合は false）
    static bool LoadEntryFromJson(const JSON &json, AnimationEntry &entry);

#if defined(USE_IMGUI)
    void ShowAnimationImGui(AnimationEntry &entry, const std::vector<ParameterBindingCandidate> &candidates);
    /// @brief 埋め込みキーフレームの一覧・編集（時刻・値・次のキーへのイージング、追加・削除）
    void ShowEmbeddedKeyframesImGui(AnimationEntry &entry);
#endif

    std::vector<AnimationEntry> animations_;
    /// @brief 次のUpdateでplayOnStartの適用を行うか（Initialize後、最初のUpdateで一度だけ）
    bool applyPlayOnStart_ = false;
};

REGISTER_COMPONENT_OBJECT(KeyFrameAnimator)

} // namespace KashipanEngine
