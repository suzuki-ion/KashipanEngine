#pragma once

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Math/Vector2.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Scene/Components/ISceneComponent.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/MathUtils/Easings.h"

namespace KashipanEngine {

class SpriteAnimator final : public ISceneComponent {
public:
    struct TimelineKey {
        float time = 0.0f;
        float value = 0.0f;
        EaseType easeType = EaseType::Linear;
    };

    struct Timeline {
        std::vector<TimelineKey> keys;
        bool loop = false;
    };

    struct PresetObject {
        std::string objectName;
        std::string parentObjectName;
        Vector2 pivotPoint{0.5f, 0.5f};
        Vector2 anchorPoint{0.5f, 0.5f};
    };

    using ApplyFunction = std::function<void(Sprite *, float)>;

    /// @brief `SpriteAnimator` を生成する
    SpriteAnimator();

    /// @brief `SpriteAnimator` を破棄する
    ~SpriteAnimator() override = default;

    /// @brief コンポーネント初期化
    void Initialize() override;

    /// @brief コンポーネント終了処理
    void Finalize() override;

    /// @brief 再生中アニメーションを1フレーム更新する
    void Update() override;

    /// @brief プリセットへ対象スプライト情報を追加・更新する
    /// @param presetName バインディングプリセット名
    /// @param objectName 対象オブジェクト名
    /// @param parentObjectName 親オブジェクト名（未指定時は親なし）
    /// @param pivotPoint ピボット位置
    /// @param anchorPoint アンカー位置
    /// @return 追加または更新に成功した場合は `true`
    bool AddPresetObject(const std::string &presetName, const std::string &objectName, const std::string &parentObjectName = "", const Vector2 &pivotPoint = Vector2{0.5f, 0.5f}, const Vector2 &anchorPoint = Vector2{0.5f, 0.5f});

    /// @brief プリセットから対象スプライト情報を削除する
    /// @param presetName バインディングプリセット名
    /// @param objectName 対象オブジェクト名
    /// @return 削除できた場合は `true`
    bool RemovePresetObject(const std::string &presetName, const std::string &objectName);

    /// @brief 指定プリセットとそのバインディングを消去する
    /// @param presetName バインディングプリセット名
    void ClearPreset(const std::string &presetName);

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
    /// @param time キー時刻
    /// @param value キー値
    /// @param easeType このキーから次キーまでのイージング種別
    /// @return 更新成功時は `true`
    bool UpdateTimelineKey(const std::string &timelineName, size_t keyIndex, float time, float value, EaseType easeType);

    /// @brief 指定キーインデックスのキーを削除する
    /// @param timelineName タイムライン名
    /// @param keyIndex キーインデックス
    /// @return 削除成功時は `true`
    bool RemoveTimelineKey(const std::string &timelineName, size_t keyIndex);

    /// @brief タイムラインのループ再生設定を更新する
    /// @param timelineName タイムライン名
    /// @param loop ループ再生する場合は `true`
    /// @return 対象タイムラインが存在し更新できた場合は `true`
    bool SetTimelineLoop(const std::string &timelineName, bool loop);

    /// @brief 指定タイムラインを削除する
    /// @param timelineName タイムライン名
    void ClearTimeline(const std::string &timelineName);

    /// @brief バインディングプリセット内オブジェクトへタイムライン適用バインドを追加する
    /// @param presetName バインディングプリセット名
    /// @param objectName 対象オブジェクト名
    /// @param timelineName 参照するタイムライン名
    /// @param apply 値適用関数
    /// @return 追加に成功した場合は `true`
    bool AddBinding(const std::string &presetName, const std::string &objectName, const std::string &timelineName, ApplyFunction apply);

    /// @brief プロパティパス指定でタイムライン適用バインドを追加する
    /// @param presetName バインディングプリセット名
    /// @param objectName 対象オブジェクト名
    /// @param timelineName 参照するタイムライン名
    /// @param propertyPath 適用先プロパティパス
    /// @return 追加に成功した場合は `true`
    bool AddBindingByPath(const std::string &presetName, const std::string &objectName, const std::string &timelineName, const std::string &propertyPath);

    /// @brief 指定バインディングプリセットのバインディングを全て削除する
    /// @param presetName バインディングプリセット名
    void ClearBindings(const std::string &presetName);

    /// @brief 指定オブジェクトプリセットとバインディングプリセットのアニメーション再生を開始する
    /// @details 同じ組み合わせが再生中の場合は先頭から再生し直す
    /// @param objectPresetName オブジェクトのプリセット名
    /// @param bindingPresetName バインディングのプリセット名
    /// @return 再生開始に成功した場合は `true`
    bool Play(const std::string &objectPresetName, const std::string &bindingPresetName);

    /// @brief 指定プリセット名をオブジェクト・バインディング両方に用いて再生する
    /// @param presetName 再生するプリセット名
    /// @return 再生開始に成功した場合は `true`
    bool Play(const std::string &presetName);

    /// @brief すべてのアニメーション再生を停止する
    void Stop();

    /// @brief 指定オブジェクトプリセットの再生を停止する
    /// @param objectPresetName 停止するオブジェクトプリセット名
    /// @return 停止成功時は `true`
    bool Stop(const std::string &objectPresetName);

    /// @brief 指定オブジェクトプリセットとバインディングプリセットの再生を停止する
    /// @param objectPresetName 停止するオブジェクトプリセット名
    /// @param bindingPresetName 停止するバインディングプリセット名
    /// @return 停止成功時は `true`
    bool Stop(const std::string &objectPresetName, const std::string &bindingPresetName);

    /// @brief アニメーション再生を一時停止する（全再生対象）
    void Pause();

    /// @brief 一時停止中のアニメーション再生を再開する（全再生対象）
    void Resume();

    /// @brief 現在再生中かどうかを取得する
    /// @return 1つでも再生中なら `true`
    bool IsPlaying() const;

    /// @brief 現在一時停止中かどうかを取得する
    /// @return 1つでも一時停止中なら `true`
    bool IsPaused() const;

    /// @brief 現在の設定をJSONファイルに保存する
    /// @param filePath 保存先ファイルパス
    /// @return 保存成功時は `true`
    bool SaveToJsonFile(const std::string &filePath) const;

    /// @brief JSONファイルから設定を読み込む
    /// @param filePath 読み込み元ファイルパス
    /// @return 読み込み成功時は `true`
    bool LoadFromJsonFile(const std::string &filePath);

#if defined(USE_IMGUI)
    /// @brief ImGui でアニメーション編集UIを表示する
    void ShowImGui() override;
#endif

private:
    struct Binding {
        std::string objectName;
        std::string timelineName;
        ApplyFunction apply;
        std::string propertyPath;
    };

    struct PlaybackState {
        std::string objectPresetName;
        std::string bindingPresetName;
        float elapsedTime = 0.0f;
        bool paused = false;
        std::unordered_map<std::string, Sprite *> activeSprites;
    };

    static float EvaluateTimeline(const Timeline &timeline, float time);
    static ApplyFunction MakeApplyFunction(const std::string &propertyPath);
    static bool IsColorPath(const std::string &propertyPath);

    bool HasAnyLoopingTimeline(const std::vector<Binding> &bindings) const;
    bool ResolveActiveSprites(const std::string &presetName, std::unordered_map<std::string, Sprite *> &outSprites) const;
    void ApplyPresetHierarchy(const std::vector<PresetObject> &preset, const std::unordered_map<std::string, Sprite *> &activeSprites);
    bool RebuildBindingFunction(Binding &binding);

#if defined(USE_IMGUI)
    void ShowImGuiWindowPresets();
    void ShowImGuiWindowHierarchy();
    void ShowImGuiWindowTimelines();
    void ShowImGuiWindowTimelineEditor();
    void ShowImGuiWindowBindings();
    void ShowImGuiWindowPlayers();
    void ShowImGuiWindowStorage();

    void RefreshTimelineEditorRange();
    size_t FindNearestTimelineKey(const Timeline &timeline, float targetTime, float maxDistance) const;
    static const std::vector<const char *> &GetEaseTypeNames();
#endif

private:
    std::unordered_map<std::string, std::vector<PresetObject>> presets_;
    std::unordered_map<std::string, Timeline> timelines_;
    std::unordered_map<std::string, std::vector<Binding>> presetBindings_;
    std::vector<PlaybackState> playbacks_;

#if defined(USE_IMGUI)
    std::array<char, 128> presetNameBuffer_{};
    std::array<char, 128> objectNameBuffer_{};
    std::array<char, 128> parentObjectNameBuffer_{};
    std::array<char, 128> timelineNameBuffer_{};
    std::array<char, 128> bindingPresetBuffer_{};
    std::array<char, 128> bindingObjectBuffer_{};
    std::array<char, 128> bindingTimelineBuffer_{};
    std::array<char, 260> jsonPathBuffer_{};
    std::array<char, 128> playPresetBuffer_{};
    std::array<char, 128> playBindingPresetBuffer_{};

    std::string selectedPresetName_;
    std::string selectedTimelineName_;
    int selectedBindingIndex_ = -1;

    float imguiKeyTime_ = 0.0f;
    float imguiKeyValue_ = 0.0f;
    int imguiEaseTypeIndex_ = 0;
    bool imguiTimelineLoop_ = false;
    int imguiPropertyPathIndex_ = 0;
    int imguiBindingObjectIndex_ = -1;
    int imguiBindingTimelineIndex_ = -1;
    int imguiPlayObjectPresetIndex_ = -1;
    int imguiPlayBindingPresetIndex_ = -1;

    std::string imguiBindingObjectName_;
    std::string imguiBindingTimelineName_;
    std::string imguiPlayObjectPresetName_;
    std::string imguiPlayBindingPresetName_;
    bool imguiUseSameBindingPreset_ = true;

    Vector2 imguiPresetPivot_{0.5f, 0.5f};
    Vector2 imguiPresetAnchor_{0.5f, 0.5f};

    float timelineViewStartTime_ = 0.0f;
    float timelineViewDuration_ = 5.0f;
    int timelineDragKeyIndex_ = -1;
    int selectedTimelineKeyIndex_ = -1;
#endif
};

} // namespace KashipanEngine
