#pragma once
#include <cstdint>
#include <variant>

#include "Scene/Components/SceneComponentHeader.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/MathUtils/Easings.h"

namespace KashipanEngine {

enum class KeyframeValueType {
    Float,
    Vector3,
    Quaternion
};

using KeyframeValue = std::variant<float, Vector3, Quaternion>;
using KeyframeApplyFunction = std::variant<
    std::function<void(float)>,
    std::function<void(const Vector3 &)>,
    std::function<void(const Quaternion &)>>;

/// @brief キーフレームのノード
struct KeyframeNode {
    /// @brief キーフレームの時間（秒）
    float time = 0.0f;
    /// @brief キーフレームの値
    KeyframeValue value = 0.0f;
    /// @brief このキーから次のキーまでのイージング種別
    EaseType easeType = EaseType::Linear;
};

/// @brief キーフレームタイムライン
struct KeyframeTimeline {
    /// @brief タイムライン名
    std::string name;
    /// @brief タイムラインの総時間（秒）
    float duration = 0.0f;
    /// @brief タイムラインの値型
    KeyframeValueType valueType = KeyframeValueType::Float;
    /// @brief タイムラインのキーフレームノードリスト（バラバラであってもKeyframeAnimatorクラス側で自動ソートされる）
    std::vector<KeyframeNode> keys;
    /// @brief タイムラインの適用関数リスト
    std::vector<KeyframeApplyFunction> applyFunctions;
    /// @brief タイムラインがループするかどうか
    bool loop = false;
};

/// @brief キーフレームの再生状態
struct KeyframePlaybackState {
    /// @brief 再生中のタイムライン名
    std::string timelineName;
    /// @brief 経過時間（秒）
    float elapsedTime = 0.0f;
    /// @brief 再生中かどうか
    bool playing = false;
    /// @brief 一時停止中かどうか
    bool paused = false;
    /// @brief 再生中のスケルトンハンドル（スケルトンアニメーションの場合）
    uint32_t skeletonHandle = 0;
};

/// @brief キーフレームアニメーターシーンコンポーネント
class KeyframeAnimator final : public ISceneComponent {
public:
    KeyframeAnimator();
    ~KeyframeAnimator() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

    /// @brief AnimationManagerのハンドルとオブジェクト名からアニメーションを再生する
    /// @param handle AnimationManagerのアニメーションハンドル
    /// @param objectName 対象オブジェクト名
    /// @param loop ループ再生する場合は `true`
    /// @return 再生開始に成功した場合は `true`
    bool PlayFromAnimationHandle(uint32_t handle, const std::string &objectName, bool loop);

    /// @brief AnimationManagerとSkeletonManagerのハンドルからアニメーションを再生する
    /// @param animationHandle AnimationManagerのアニメーションハンドル
    /// @param skeletonHandle SkeletonManagerのスケルトンハンドル
    /// @param loop ループ再生する場合は `true`
    /// @return 再生開始に成功した場合は `true`
    bool PlayFromAnimationAndSkeletonHandle(uint32_t animationHandle, uint32_t skeletonHandle, bool loop, std::string clipName = "");

    /// @brief タイムラインの追加
    /// @param timelineName タイムライン名
    /// @return 追加に成功した場合は `true`
    bool AddTimeline(const std::string &timelineName);

    /// @brief タイムラインの追加
    /// @param timeline タイムライン
    /// @return 追加に成功した場合は `true`
    bool AddTimeline(const KeyframeTimeline &timeline);

    /// @brief 指定タイムラインを削除する
    /// @param timelineName タイムライン名
    /// @return 削除に成功した場合は `true`
    bool RemoveTimeline(const std::string &timelineName);

    /// @brief 指定タイムラインが存在するかどうかを確認する
    /// @param timelineName タイムライン名
    /// @return 存在する場合は `true`
    bool HasTimeline(const std::string &timelineName) const;

    /// @brief タイムラインへキーを追加する（時間順で自動挿入）
    /// @param timelineName タイムライン名
    /// @param time キー時刻（秒）
    /// @param value キー値
    /// @param easeType このキーから次キーまでのイージング種別
    /// @return 追加に成功した場合は `true`
    bool AddTimelineKey(const std::string &timelineName, float time, float value, EaseType easeType = EaseType::Linear);

    /// @brief 指定キーインデックスの値を更新する
    /// @param timelineName タイムライン名
    /// @param keyIndex キーインデックス
    /// @param value 新しい値
    /// @return 更新に成功した場合は `true`
    bool UpdateTimelineKey(const std::string &timelineName, size_t keyIndex, float value);

    /// @brief 指定キーインデックスのキーを削除する
    /// @param timelineName タイムライン名
    /// @param keyIndex キーインデックス
    /// @return 削除に成功した場合は `true`
    bool RemoveTimelineKey(const std::string &timelineName, size_t keyIndex);

    /// @brief タイムラインのループ再生設定を更新する
    /// @param timelineName タイムライン名
    /// @param loop ループ再生設定
    /// @return 更新に成功した場合は `true`
    bool SetTimelineLoop(const std::string &timelineName, bool loop);

    /// @brief 指定タイムラインが現在再生中かどうかを確認する
    /// @param timelineName タイムライン名
    /// @return 再生中の場合は `true`
    bool IsTimelinePlaying(const std::string &timelineName) const;

    /// @brief 指定タイムラインが現在一時停止中かどうかを確認する
    /// @param timelineName タイムライン名
    /// @return 一時停止中の場合は `true`
    bool IsTimelinePaused(const std::string &timelineName) const;

    /// @brief 現在の設定をJSONファイルに保存する
    /// @param filePath 保存先ファイルパス
    /// @return 保存成功時は `true`
    bool SaveSettings(const std::string &filePath);

    /// @brief JSONファイルから設定を読み込む
    /// @param filePath 読み込み元ファイルパス
    /// @return 読み込み成功時は `true`
    bool LoadSettings(const std::string &filePath);

#ifdef USE_IMGUI
    void ShowImGui() override;
#endif

private:
    std::unordered_map<std::string, KeyframeTimeline> timelines_;
    std::unordered_map<std::string, KeyframePlaybackState> playbackStates_;
};

REGISTER_COMPONENT_SCENE(KeyframeAnimator)

} // namespace KashipanEngine