#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/ModelManager.h"
#include "Assets/SkeletonManager.h"
#include "Assets/AnimationManager.h"
#include "Utilities/Passkeys.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#endif

namespace KashipanEngine {

class SkinnedMeshRenderer;

/// @brief スケルトンアニメーションの再生を管理するコンポーネント
/// @details 同じオブジェクトのMeshFilterが指すメッシュのスケルトンへアニメーションクリップを
///          適用する（Unityの Animator と同様の役割）。アニメーションクリップの取得元は既定では
///          メッシュ自身のファイルだが、animationSourceAssetPath_ を指定すると別ファイルの
///          アニメーションを適用できる（同じスケルトン構造・ボーン名を持つファイル間でのみ有効）。
///          SkinnedMeshRendererはこのコンポーネントが保持する姿勢（ボーンのワールド行列）を
///          読み取ってGPUスキニングを行うのみで、アニメーションの再生状態そのものは持たない。
class Animator final : public IObjectComponent {
public:
    // clipName_の直接書き込み時は、セッターと同様に再生位置を先頭へ戻す
    OBJECT_COMPONENT_CONSTRUCTOR(Animator, 1,
        SetUpdatePriority(500);
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(clipName_, [this] { elapsedTime_ = 0.0f; });
        ADD_MEMBER_VARIABLE(animationSourceAssetPath_);
        ADD_MEMBER_VARIABLE(playOnStart_);
        ADD_MEMBER_VARIABLE(loop_);
        ADD_MEMBER_VARIABLE(playbackSpeed_);
    )
    COMPONENT_CATEGORY("Animation")
    ~Animator() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Animator>();
        ptr->rootBoneObjectID_ = rootBoneObjectID_;
        ptr->clipName_ = clipName_;
        ptr->animationSourceAssetPath_ = animationSourceAssetPath_;
        ptr->playOnStart_ = playOnStart_;
        ptr->loop_ = loop_;
        ptr->playbackSpeed_ = playbackSpeed_;
        return ptr;
    }

    //==================================================
    // 再生対象・再生設定
    //==================================================

    /// @brief 再生するアニメーションクリップ名を設定（アニメーション取得元アセットの中から選択する。
    ///        空文字の場合はバインドポーズのまま静止する）
    void SetClipName(const std::string &clipName) { clipName_ = clipName; elapsedTime_ = 0.0f; }
    const std::string &GetClipName() const noexcept { return clipName_; }

    /// @brief アニメーション取得元のアセットパスを設定する
    /// @details 空文字の場合はMeshFilterが指すメッシュ自身のファイルからアニメーションを取得する。
    ///          別ファイルを指定すると、そのファイルのアニメーションを（ボーン名が一致する範囲で）
    ///          このスケルトンへ適用できる（例: Mixamo等、同一リグでアニメーションだけ別ファイルの場合）
    void SetAnimationSourceAssetPath(const std::string &path) { animationSourceAssetPath_ = path; }
    const std::string &GetAnimationSourceAssetPath() const noexcept { return animationSourceAssetPath_; }

    void SetPlayOnStart(bool playOnStart) { playOnStart_ = playOnStart; }
    bool GetPlayOnStart() const noexcept { return playOnStart_; }
    void SetLoop(bool loop) { loop_ = loop; }
    bool GetLoop() const noexcept { return loop_; }
    void SetPlaybackSpeed(float speed) { playbackSpeed_ = speed; }
    float GetPlaybackSpeed() const noexcept { return playbackSpeed_; }

    void Play() { isPlaying_ = true; }
    void Stop() { isPlaying_ = false; }
    bool IsPlaying() const noexcept { return isPlaying_; }

    /// @brief Root Boneオブジェクトを設定する（Unityと同様。設定するとメッシュの描画位置が
    ///        このオブジェクトのTransformに沿って動き、アーマチュア同期の探索起点にもなる）
    /// @details 未設定（無効なUUID）の場合は所有オブジェクト自身の最上位の祖先を起点に使う
    void SetRootBoneObject(const EmptyObject *rootBoneObject) {
        rootBoneObjectID_ = rootBoneObject ? rootBoneObject->GetObjectID() : UUID128();
    }
    void SetRootBoneObject(const UUID128 &rootBoneObjectID) { rootBoneObjectID_ = rootBoneObjectID; }
    const UUID128 &GetRootBoneObjectID() const noexcept { return rootBoneObjectID_; }
    EmptyObject *GetRootBoneObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !rootBoneObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(rootBoneObjectID_);
    }

    /// @brief アニメーション取得元アセットに含まれるアニメーションクリップ名の一覧を取得
    std::vector<std::string> GetAvailableClipNames() const;

    /// @brief このコンポーネント専用のスケルトンインスタンスの姿勢をバインドポーズへ戻し、
    ///        アニメーション経過時間もリセットする（ゲームループ停止時にSceneRenderer経由で呼ばれる）
    void ResetToBindPose();

    //==================================================
    // SkinnedMeshRenderer専用: 姿勢取得用（Passkey<SkinnedMeshRenderer>限定）
    //==================================================

    /// @brief このコンポーネント専用のスケルトンインスタンス（現在の姿勢）を取得する
    /// @details 呼び出し前にメッシュ解決を最新化するため、SkinnedMeshRenderer::RefreshSkinningResources
    ///          から毎フレーム呼ばれる想定
    const Skeleton &GetSkeletonInstance(Passkey<SkinnedMeshRenderer>) {
        RebuildSkeletonInstanceIfNeeded();
        return skeletonInstance_;
    }

protected:
    void Initialize() override;
    void Update() override;

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

private:
    ModelManager::ModelHandle GetMeshHandle() const;
    /// @brief 現在のメッシュハンドル・アニメーション取得元に応じてスケルトン・アニメーションの
    ///        解決を（再）行う。メッシュ・取得元のどちらも変わっていない場合は何もしない
    void RebuildSkeletonInstanceIfNeeded();
    /// @brief 選択中アニメーションを進行させ、スケルトンのジョイントへ反映する
    void AdvanceAnimation(float deltaTime);
    /// @brief シーン上のアーマチュア用オブジェクトへ現在のジョイント姿勢を同期する
    void SyncArmatureObjects();

    UUID128 rootBoneObjectID_{};
    std::string clipName_;
    std::string animationSourceAssetPath_;
    bool playOnStart_ = true;
    bool loop_ = true;
    float playbackSpeed_ = 1.0f;

    // 再生状態（非シリアライズ）
    bool isPlaying_ = false;
    float elapsedTime_ = 0.0f;

    // メッシュ・スケルトン・アニメーションの解決結果（非シリアライズ、キャッシュ用）
    ModelManager::ModelHandle lastMeshHandle_ = ModelManager::kInvalidHandle;
    std::string lastAnimationSourceAssetPath_;
    std::string baseAssetPath_;
    SkeletonManager::SkeletonHandle skeletonHandle_ = SkeletonManager::kInvalidHandle;
    AnimationManager::AnimationHandle animationHandle_ = AnimationManager::kInvalidHandle;
    /// @brief このコンポーネント専用のスケルトンインスタンス（SkeletonManagerが保持する
    ///        アセット本体とは独立した複製）。同じスケルトンアセットを参照する複数のAnimatorが
    ///        互いのアニメーション再生状態に干渉しないよう、メッシュ解決時にCloneSkeletonで複製して保持する
    Skeleton skeletonInstance_;
};

REGISTER_COMPONENT_OBJECT(Animator)

} // namespace KashipanEngine
