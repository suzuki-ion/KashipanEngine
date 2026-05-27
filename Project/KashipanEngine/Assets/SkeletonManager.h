#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Objects/Components/3D/Transform3D.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief モデルのノード構造体
struct Node final {
    std::unique_ptr<Transform3D> transform;
    std::string name;
    std::vector<Node> children;
};

/// @brief スケルトンのジョイント構造体
struct SkeletonJoint final {
    std::unique_ptr<Transform3D> transform;
    std::string name;
    std::unique_ptr<Transform3D> skeletonSpaceTransform; // スケルトンスペースの変換（ルートジョイントからの累積変換）
    std::vector<int32_t> childrenIndices; // 子ジョイントのインデックス（ModelData のジョイント配列内でのインデックス）
    std::optional<int32_t> parentIndex; // 親ジョイントのインデックス（ModelData のジョイント配列内でのインデックス、ルートジョイントの場合は std::nullopt）
};

/// @brief スケルトン構造体
struct Skeleton final {
    int32_t rootJointIndex; // ルートジョイントのインデックス（ModelData のジョイント配列内でのインデックス）
    std::unordered_map<std::string, int32_t> jointNameToIndexMap; // ジョイント名からインデックスへのマップ
    std::vector<SkeletonJoint> joints; // ジョイントの配列
};

class GameEngine;
class SkeletonManager;

/// @brief スケルトンデータ
struct SkeletonData final {
private:
    friend class SkeletonManager;

    SkeletonData() = default;

    Node rootNode_{};
    Skeleton skeleton_{};
    std::string assetRelativePath_;

public:
    const Node &GetRootNode() const noexcept { return rootNode_; }
    const Skeleton &GetSkeleton() const noexcept { return skeleton_; }
    const std::string &GetAssetRelativePath() const noexcept { return assetRelativePath_; }
};

/// @brief スケルトン管理クラス
class SkeletonManager final {
public:
    using SkeletonHandle = uint32_t;
    static constexpr SkeletonHandle kInvalidHandle = 0;

    struct SkeletonListEntry final {
        SkeletonHandle handle = kInvalidHandle;
        std::string fileName;
        std::string assetPath;
        uint32_t jointCount = 0;
    };

    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    /// @param assetsRootPath Assets フォルダのルートパス
    SkeletonManager(Passkey<GameEngine>, const std::string &assetsRootPath = "Assets");
    ~SkeletonManager();

    SkeletonManager(const SkeletonManager &) = delete;
    SkeletonManager &operator=(const SkeletonManager &) = delete;
    SkeletonManager(SkeletonManager &&) = delete;
    SkeletonManager &operator=(SkeletonManager &&) = delete;

    /// @brief 指定ファイルパスのスケルトンを読み込む（Assets ルートからの相対 or フルパス）
    /// @return 読み込んだスケルトンのハンドル（失敗時は `kInvalidHandle`）
    SkeletonHandle LoadSkeleton(const std::string &filePath);

    /// @brief ファイル名単体からスケルトンハンドルを取得
    static SkeletonHandle GetSkeletonHandleFromFileName(const std::string &fileName);
    /// @brief Assetsルートからの相対パスからスケルトンハンドルを取得
    static SkeletonHandle GetSkeletonHandleFromAssetPath(const std::string &assetPath);

    /// @brief ハンドルからスケルトンデータを取得
    static const SkeletonData &GetSkeletonData(SkeletonHandle handle);
    /// @brief ファイル名単体からスケルトンデータを取得
    static const SkeletonData &GetSkeletonDataFromFileName(const std::string &fileName);
    /// @brief Assetsルートからの相対パスからスケルトンデータを取得
    static const SkeletonData &GetSkeletonDataFromAssetPath(const std::string &assetPath);

    /// @brief 読み込まれたスケルトン一覧を取得する
    static std::vector<SkeletonListEntry> GetLoadedSkeletonListEntries();

    const std::string &GetAssetsRootPath() const noexcept { return assetsRootPath_; }

    /// @brief スケルトンのジョイントの変換行列を更新する
    /// @param handle スケルトンハンドル
    /// @return 更新に成功した場合はtrue、失敗した場合はfalseを返す
    static const bool UpdateSkeletonJointTransforms(SkeletonHandle handle);

private:
    void LoadAllFromAssetsFolder();

    std::string assetsRootPath_;
    static inline const SkeletonData sEmptyData;
};

} // namespace KashipanEngine