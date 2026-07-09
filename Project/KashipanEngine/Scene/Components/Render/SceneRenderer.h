#pragma once
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Assets/ModelManager.h"
#include "Assets/MaterialManager.h"
#include "Graphics/Renderer/EditorDebugDraw.h"
#include "Math/Matrix4x4.h"
#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class ConstantBufferResource;
class EmptyObject;
class MeshRenderer;
class SpriteRenderer;
class SkinnedMeshRenderer;
class CameraRenderer;
class LightRenderer;
class IRenderTarget;
class PipelineManager;
class Renderer;
class RWStructuredBufferResource;

/// @brief シーン内の描画用コンポーネントを収集して描画リストを構築するシーンコンポーネント
class SceneRenderer final : public ISceneComponent {
public:
    /// @brief 描画リストの1要素
    /// @details MeshRenderer/SpriteRenderer どちらから作られたエントリかを問わず、
    ///          描画に必要な値をここで解決済みの状態で保持する（Rendererはこの値のみを参照する）。
    struct DrawEntry {
        IRenderTarget *target = nullptr;
        std::string pipelineName;
        ModelManager::ModelHandle meshHandle = ModelManager::kInvalidHandle;
        MaterialManager::MaterialHandle materialHandle = MaterialManager::kInvalidHandle;
        Matrix4x4 worldMatrix = Matrix4x4::Identity();
        /// @brief SkinnedMeshRendererから作られたエントリのみ非null。
        ///        非nullの場合、頂点バッファは静的メッシュではなくこのGPUスキニング結果を使用し、
        ///        インスタンス（バッチ）結合の対象にもならない（各インスタンスが専用の出力バッファを持つため）
        RWStructuredBufferResource *skinnedVertexBuffer = nullptr;
    };

    SCENE_COMPONENT_CONSTRUCTOR(SceneRenderer, 1, SetUpdatePriority(1000);)
    COMPONENT_CATEGORY("Render")
    ~SceneRenderer() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<SceneRenderer>();
    }

    //==================================================
    // 描画用コンポーネントの登録（各コンポーネントのInitialize/Finalizeから呼ばれる）
    //==================================================

    void RegisterMeshRenderer(MeshRenderer *renderer);
    void UnregisterMeshRenderer(const MeshRenderer *renderer);
    void RegisterSpriteRenderer(SpriteRenderer *renderer);
    void UnregisterSpriteRenderer(const SpriteRenderer *renderer);
    void RegisterSkinnedMeshRenderer(SkinnedMeshRenderer *renderer);
    void UnregisterSkinnedMeshRenderer(const SkinnedMeshRenderer *renderer);
    void RegisterCameraRenderer(CameraRenderer *renderer);
    void UnregisterCameraRenderer(const CameraRenderer *renderer);
    void RegisterLightRenderer(LightRenderer *renderer);
    void UnregisterLightRenderer(const LightRenderer *renderer);

    const std::vector<MeshRenderer *> &GetMeshRenderers() const noexcept { return meshRenderers_; }
    const std::vector<SpriteRenderer *> &GetSpriteRenderers() const noexcept { return spriteRenderers_; }
    const std::vector<SkinnedMeshRenderer *> &GetSkinnedMeshRenderers() const noexcept { return skinnedMeshRenderers_; }
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

    //==================================================
    // エディター用描画先
    //==================================================

    /// @brief エディター用描画先を設定する（全MeshRendererがこの描画先にも描画される）
    /// @param target エディター用描画先（nullptrで解除）
    /// @param cameraBuffer この描画先の描画時にバインドされるカメラ定数バッファ
    void SetEditorTarget(IRenderTarget *target, ConstantBufferResource *cameraBuffer) {
        editorTarget_ = target;
        editorCameraBuffer_ = cameraBuffer;
    }
    /// @brief 指定描画先がエディター用描画先の場合、そのカメラ定数バッファを返す
    ConstantBufferResource *GetEditorCameraBuffer(const IRenderTarget *target) const {
        return (editorTarget_ && target == editorTarget_) ? editorCameraBuffer_ : nullptr;
    }
    /// @brief エディター用描画先を取得する（未設定の場合は nullptr）
    IRenderTarget *GetEditorTarget() const noexcept { return editorTarget_; }

    /// @brief エディターのデバッグ表示設定（グリッド/当たり判定の可視化）を登録する
    /// @details SceneEditorViewが毎フレーム呼び、Rendererがエディター用描画先の描画時に参照する
    void SetEditorDebugDraw(EditorDebugDrawSettings settings) { editorDebugDraw_ = std::move(settings); }
    const EditorDebugDrawSettings &GetEditorDebugDraw() const noexcept { return editorDebugDraw_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    std::vector<MeshRenderer *> meshRenderers_;
    std::vector<SpriteRenderer *> spriteRenderers_;
    std::vector<SkinnedMeshRenderer *> skinnedMeshRenderers_;
    std::vector<CameraRenderer *> cameraRenderers_;
    std::vector<LightRenderer *> lightRenderers_;

    std::vector<DrawEntry> sortedDrawList_;
    std::unordered_map<const IRenderTarget *, EmptyObject *> targetOwners_;

    IRenderTarget *editorTarget_ = nullptr;
    ConstantBufferResource *editorCameraBuffer_ = nullptr;
    EditorDebugDrawSettings editorDebugDraw_;
};

REGISTER_COMPONENT_SCENE(SceneRenderer)

} // namespace KashipanEngine
