#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Debug/Logger.h"
#include "Utilities/Passkeys.h"
#include "Scene/Components/KeyframeAnimator.h"

namespace KashipanEngine {

class GameEngine;

/// @brief アニメーションクリップデータ
struct AnimationClip final {
    std::string name;
    float duration = 0.0f;
    float ticksPerSecond = 0.0f;
    std::vector<KeyframeTimeline> timelines;
    std::unordered_map<std::string, uint32_t> timelineNameToIndex;
    std::unordered_map<std::string, std::vector<uint32_t>> nodeNameToTimelineIndices;
};

/// @brief アニメーションデータ
struct AnimationData final {
private:
    friend class AnimationManager;

    AnimationData() = default;

    std::vector<AnimationClip> clips_;
    std::unordered_map<std::string, uint32_t> clipNameToIndex_;
    std::string assetRelativePath_;

public:
    uint32_t GetClipCount() const noexcept { return static_cast<uint32_t>(clips_.size()); }
    const AnimationClip *GetClip(uint32_t idx) const noexcept { return idx < clips_.size() ? &clips_[idx] : nullptr; }
    const AnimationClip *FindClipByName(const std::string &clipName) const noexcept {
        LogScope scope;
        auto it = clipNameToIndex_.find(clipName);
        if (it == clipNameToIndex_.end()) return nullptr;
        return GetClip(it->second);
    }
    const std::vector<AnimationClip> &GetClips() const noexcept { return clips_; }
    const std::string &GetAssetRelativePath() const noexcept { return assetRelativePath_; }
};

/// @brief アニメーション管理クラス
class AnimationManager final {
public:
    using AnimationHandle = uint32_t;
    static constexpr AnimationHandle kInvalidHandle = 0;

    struct AnimationListEntry final {
        AnimationHandle handle = kInvalidHandle;
        std::string fileName;
        std::string assetPath;
        uint32_t clipCount = 0;
    };

    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    /// @param assetsRootPath Assets フォルダのルートパス
    AnimationManager(Passkey<GameEngine>, const std::string &assetsRootPath = "Assets");
    ~AnimationManager();

    AnimationManager(const AnimationManager &) = delete;
    AnimationManager &operator=(const AnimationManager &) = delete;
    AnimationManager(AnimationManager &&) = delete;
    AnimationManager &operator=(AnimationManager &&) = delete;

    /// @brief 指定ファイルパスのアニメーションを読み込む（Assets ルートからの相対 or フルパス）
    /// @return 読み込んだアニメーションのハンドル（失敗時は `kInvalidHandle`）
    AnimationHandle LoadAnimation(const std::string &filePath);

    /// @brief ファイル名単体からアニメーションハンドルを取得する
    /// @details 同名ファイルが複数読み込まれている場合は最初に登録されたハンドルを返す。
    ///          全てのハンドルが必要な場合は `GetAnimationHandlesFromFileName` を使うこと。
    static AnimationHandle GetAnimationHandleFromFileName(const std::string &fileName);
    /// @brief ファイル名単体から一致する全てのアニメーションハンドルを取得する
    /// @details 異なるフォルダに同名ファイルが存在する場合、複数のハンドルが返ることがある。
    static const std::vector<AnimationHandle> &GetAnimationHandlesFromFileName(const std::string &fileName);
    /// @brief Assetsルートからの相対パスからアニメーションハンドルを取得
    static AnimationHandle GetAnimationHandleFromAssetPath(const std::string &assetPath);

    /// @brief ハンドルからアニメーションデータを取得
    static const AnimationData &GetAnimationData(AnimationHandle handle);
    /// @brief ファイル名単体からアニメーションデータを取得
    static const AnimationData &GetAnimationDataFromFileName(const std::string &fileName);
    /// @brief Assetsルートからの相対パスからアニメーションデータを取得
    static const AnimationData &GetAnimationDataFromAssetPath(const std::string &assetPath);

    /// @brief 読み込まれたアニメーション一覧を取得する
    static std::vector<AnimationListEntry> GetLoadedAnimationListEntries();

    const std::string &GetAssetsRootPath() const noexcept { return assetsRootPath_; }

private:
    void LoadAllFromAssetsFolder();

    std::string assetsRootPath_;
    static inline const AnimationData sEmptyData;
    static inline const std::vector<AnimationHandle> sEmptyHandleList;
};

} // namespace KashipanEngine
