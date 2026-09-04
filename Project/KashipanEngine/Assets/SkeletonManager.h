#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "Debug/Logger.h"
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

    /// @brief 現在のTRSをバインドポーズとして記憶する（スケルトン構築直後に呼ぶ想定）
    void CaptureBindPose() {
        bindTranslate_ = translate_;
        bindRotate_ = rotate_;
        bindScale_ = scale_;
    }
    /// @brief 記憶しておいたバインドポーズへ復元する（ゲームループ停止時などに使用）
    void ResetToBindPose() {
        translate_ = bindTranslate_;
        rotate_ = bindRotate_;
        scale_ = bindScale_;
        MarkDirty();
    }

    /// @brief このTransformの複製を作成する（現在のTRS・バインドポーズをコピーする）
    /// @details parent_は複製しないため、階層を複製する場合は呼び出し側で複製後の
    ///          親インスタンスへSetParentし直すこと（SkeletonManager::CloneSkeleton参照）
    std::unique_ptr<SkeletonTransform> Clone() const {
        auto copy = std::make_unique<SkeletonTransform>();
        copy->translate_ = translate_;
        copy->rotate_ = rotate_;
        copy->scale_ = scale_;
        copy->bindTranslate_ = bindTranslate_;
        copy->bindRotate_ = bindRotate_;
        copy->bindScale_ = bindScale_;
        copy->isDirty_ = true;
        return copy;
    }

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
            // 親がある場合は親のワールド行列と掛け合わせる（行ベクトル規約: 自身のlocalを先に適用してから
            // 親のワールド行列を適用する。Objects/Components/Transform::GetWorldMatrixと同じ規約）
            worldMatrix_ = local * parent_->GetWorldMatrix();
        } else {
            worldMatrix_ = local;
        }

        isDirty_ = false;
    }

    Vector3 translate_ = Vector3::Zero();
    Quaternion rotate_ = Quaternion::Identity();
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
    SkeletonTransform *parent_ = nullptr;

    // CaptureBindPose()で記憶したバインドポーズ（ResetToBindPose()用）
    Vector3 bindTranslate_ = Vector3::Zero();
    Quaternion bindRotate_ = Quaternion::Identity();
    Vector3 bindScale_ = { 1.0f, 1.0f, 1.0f };

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

    /// @brief 読み込み済みの全スケルトンのジョイント姿勢をバインドポーズへ復元する
    /// @details アニメーションはSkeletonManagerが持つジョイントのTransformを直接書き換えて
    ///          進行するため、シーンオブジェクトの状態とは独立してポーズが残り続ける。
    ///          ゲームループ停止時（シーンエディターのStop）など、アニメーション再生前の
    ///          状態へ戻したい場合に呼ぶ。
    static void ResetAllSkeletonsToBindPose();

    /// @brief 指定スケルトンの複製（ジョイント階層・バインドポーズを含む）を作成する
    /// @details 複数のSkinnedMeshRendererが同じスケルトンアセットを参照していても、
    ///          このManagerが持つ本体（アセット単位で共有）を直接書き換えるとアニメーション
    ///          再生状態が互いに干渉してしまう。SkinnedMeshRendererはこの関数で複製した
    ///          自分専用のSkeletonを保持し、そちらのジョイントTransformのみを書き換えて使う。
    /// @return 複製されたSkeleton（ハンドルが無効な場合は空のSkeleton）
    static Skeleton CloneSkeleton(SkeletonHandle handle);

private:
    void LoadAllFromAssetsFolder();

    std::string assetsRootPath_;
    static inline const SkeletonData sEmptyData;
};

} // namespace KashipanEngine