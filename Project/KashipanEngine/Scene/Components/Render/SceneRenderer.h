#pragma once
#include <unordered_map>
#include <vector>

#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class EmptyObject;
class MeshRenderer;
class CameraRenderer;
class LightRenderer;
class IRenderTarget;
class PipelineManager;
class Renderer;

/// @brief シーン内の描画用コンポーネントを収集して描画リストを構築するシーンコンポーネント
class SceneRenderer final : public ISceneComponent {
public:
    /// @brief 描画リストの1要素（描画先と描画対象のペア）
    struct DrawEntry {
        IRenderTarget *target = nullptr;
        MeshRenderer *renderer = nullptr;
    };

    SCENE_COMPONENT_CONSTRUCTOR(SceneRenderer, 1, SetUpdatePriority(1000);)
    ~SceneRenderer() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<SceneRenderer>();
    }

    //==================================================
    // 描画用コンポーネントの登録（各コンポーネントのInitialize/Finalizeから呼ばれる）
    //==================================================

    void RegisterMeshRenderer(MeshRenderer *renderer);
    void UnregisterMeshRenderer(const MeshRenderer *renderer);
    void RegisterCameraRenderer(CameraRenderer *renderer);
    void UnregisterCameraRenderer(const CameraRenderer *renderer);
    void RegisterLightRenderer(LightRenderer *renderer);
    void UnregisterLightRenderer(const LightRenderer *renderer);

    const std::vector<MeshRenderer *> &GetMeshRenderers() const noexcept { return meshRenderers_; }
    const std::vector<CameraRenderer *> &GetCameraRenderers() const noexcept { return cameraRenderers_; }
    const std::vector<LightRenderer *> &GetLightRenderers() const noexcept { return lightRenderers_; }

    /// @brief ソート済み描画リストを構築して返す
    /// @details 描画先→パイプラインの描画優先度→メッシュ→マテリアルの順でソートされる
    /// @param pipelineManager パイプラインの描画優先度取得用
    const std::vector<DrawEntry> &BuildSortedDrawList(Passkey<Renderer>, PipelineManager *pipelineManager);

    /// @brief 描画先からその描画先を所有するオブジェクトを取得（BuildSortedDrawList 後に有効）
    EmptyObject *GetTargetOwner(const IRenderTarget *target) const {
        auto it = targetOwners_.find(target);
        return it != targetOwners_.end() ? it->second : nullptr;
    }

    /// @brief 描画先オブジェクトに付与された全描画先コンポーネントから IRenderTarget を収集する
    static void CollectRenderTargets(EmptyObject *targetObject, std::vector<IRenderTarget *> &out);

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    std::vector<MeshRenderer *> meshRenderers_;
    std::vector<CameraRenderer *> cameraRenderers_;
    std::vector<LightRenderer *> lightRenderers_;

    std::vector<DrawEntry> sortedDrawList_;
    std::unordered_map<const IRenderTarget *, EmptyObject *> targetOwners_;
};

REGISTER_COMPONENT_SCENE(SceneRenderer)

} // namespace KashipanEngine
