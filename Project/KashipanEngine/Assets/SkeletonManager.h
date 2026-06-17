#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief スケルトン用トランスフォーム構造体
class SkeletonTransform {
public:
    SkeletonTransform() = default;
    ~SkeletonTransform() = default;

    // ワールド行列の取得
    const Matrix4x4 &GetWorldMatrix() {
        if (isDirty_) {
            UpdateWorldMatrix();
        }
        return worldMatrix_;
    }

    void SetTranslate(const Vector3 &t) { translate_ = t; MarkDirty(); }
    void SetRotate(const Quaternion &r) { rotate_ = r; MarkDirty(); }
    void SetScale(const Vector3 &s) { scale_ = s; MarkDirty(); }
    void SetParent(SkeletonTransform *parent) { parent_ = parent; MarkDirty(); }

    const Vector3 &GetTranslate() const noexcept { return translate_; }
    const Quaternion &GetRotate() const noexcept { return rotate_; }
    const Vector3 &GetScale() const noexcept { return scale_; }
    const SkeletonTransform *GetParent() const noexcept { return parent_; }

private:
    // 変更通知
    void MarkDirty() {
        isDirty_ = true;
    }

    // 行列更新処理
    void UpdateWorldMatrix() {
        // クォータニオンから回転行列を生成してワールド行列を構築
        Matrix4x4 scaleMat;
        scaleMat.MakeScale(scale_);

        Matrix4x4 rotateMat = rotate_.MakeRotateMatrix();

        Matrix4x4 translateMat;
        translateMat.MakeTranslate(translate_);

        Matrix4x4 local = scaleMat * rotateMat * translateMat;
        if (parent_) {
            // 親がある場合は親のワールド行列と掛け合わせる
            worldMatrix_ = parent_->GetWorldMatrix() * local;
        } else {
            worldMatrix_ = local;
        }

        isDirty_ = false;
    }

    Vector3 translate_ = Vector3::Zero();
    Quaternion rotate_ = Quaternion::Identity();
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
    SkeletonTransform *parent_ = nullptr;

    Matrix4x4 worldMatrix_ = Matrix4x4::Identity();
    bool isDirty_ = true;
};

/// @brief モデルのノード構造体
struct Node final {
    std::unique_ptr<SkeletonTransform> transform;
    std::string name;
    std::vector<Node> children;
};

/// @brief スケルトンのジョイント構造体
struct SkeletonJoint final {
    std::unique_ptr<SkeletonTransform> transform;
    std::string name;
    std::unique_ptr<SkeletonTransform> skeletonSpaceTransform; // スケルトンスペースの変換（ルートジョイントからの累積変換）
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