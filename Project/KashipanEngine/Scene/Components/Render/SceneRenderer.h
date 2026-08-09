#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "Assets/ModelManager.h"
#include "Assets/MaterialManager.h"
#include "Graphics/Renderer/EditorDebugDraw.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class ConstantBufferResource;
class EmptyObject;
class MeshRenderer;
class SpriteRenderer;
class TextRenderer;
class SkinnedMeshRenderer;
class CameraRenderer;
class LightRenderer;
class ParticleSystemBase;
class IPostProcessComponent;
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
        /// @brief 描画するインデックス範囲（サブメッシュ。indexCount==0の場合はメッシュ全体を描画する）
        std::uint32_t indexStart = 0;
        std::uint32_t indexCount = 0;
        Matrix4x4 worldMatrix = Matrix4x4::Identity();
        /// @brief SkinnedMeshRendererから作られたエントリのみ非null。
        ///        非nullの場合、頂点バッファは静的メッシュではなくこのGPUスキニング結果を使用し、
        ///        インスタンス（バッチ）結合の対象にもならない（各インスタンスが専用の出力バッファを持つため）
        RWStructuredBufferResource *skinnedVertexBuffer = nullptr;
        /// @brief オブジェクト単位の色（MeshRenderer/SkinnedMeshRendererのInstance Color）。
        ///        持たない型（SpriteRenderer）からのエントリは既定値のまま（見た目に影響しない）
        Vector4 instanceColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        /// @brief instanceColorの適用方法（MeshRenderer::ColorBlendMode/SkinnedMeshRenderer::ColorBlendModeの値。
        ///        0=Override,1=Multiply,2=Add,3=Subtract）
        int instanceColorBlendMode = 1;
    };

    /// @brief DrawEntryにソートキー（描画先種別順・パイプライン優先度）を付随させた中間データ
    struct RankedDrawEntry {
        DrawEntry entry;
        int kindOrder = 0;
        std::int32_t pipelinePriority = 0;
    };

    /// @brief キャッシュ対象（targetObjectID未指定＝エディター用描画先のみに描画するMesh/SpriteRenderer）
    ///        1件分のキャッシュ。パイプライン名解決・ソートキー算出はキャッシュ構築時のみ行い、
    ///        毎フレームはアクティブ状態の再確認とワールド行列の再計算のみ行う
    struct CachedRankedEntry {
        RankedDrawEntry ranked;
        std::variant<MeshRenderer *, SpriteRenderer *> source;
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
    void RegisterTextRenderer(TextRenderer *renderer);
    void UnregisterTextRenderer(const TextRenderer *renderer);
    void RegisterSkinnedMeshRenderer(SkinnedMeshRenderer *renderer);
    void UnregisterSkinnedMeshRenderer(const SkinnedMeshRenderer *renderer);
    void RegisterCameraRenderer(CameraRenderer *renderer);
    void UnregisterCameraRenderer(const CameraRenderer *renderer);
    void RegisterLightRenderer(LightRenderer *renderer);
    void UnregisterLightRenderer(const LightRenderer *renderer);
    /// @brief GPU Simulation有効なParticleSystem2D/3Dを登録する（ParticleSystemBase::Initialize/SwitchSimulationModeから呼ばれる）
    void RegisterGpuParticleEmitter(ParticleSystemBase *emitter);
    void UnregisterGpuParticleEmitter(const ParticleSystemBase *emitter);
    /// @brief ポストエフェクトコンポーネントを登録する（IPostProcessComponent::Initialize/Finalizeから呼ばれる）
    /// @details 派生クラスごとに個別のコンポーネント型として登録されているため、
    ///          Rendererはこの一覧からオーナーオブジェクトで絞り込むことで、型を問わず
    ///          「そのオブジェクトのポストエフェクト一覧」へ一括アクセスできる
    void RegisterPostProcessComponent(IPostProcessComponent *component);
    void UnregisterPostProcessComponent(const IPostProcessComponent *component);

    const std::vector<MeshRenderer *> &GetMeshRenderers() const noexcept { return meshRenderers_; }
    const std::vector<SpriteRenderer *> &GetSpriteRenderers() const noexcept { return spriteRenderers_; }
    const std::vector<TextRenderer *> &GetTextRenderers() const noexcept { return textRenderers_; }
    const std::vector<SkinnedMeshRenderer *> &GetSkinnedMeshRenderers() const noexcept { return skinnedMeshRenderers_; }
    const std::vector<CameraRenderer *> &GetCameraRenderers() const noexcept { return cameraRenderers_; }
    const std::vector<LightRenderer *> &GetLightRenderers() const noexcept { return lightRenderers_; }
    const std::vector<ParticleSystemBase *> &GetGpuParticleEmitters() const noexcept { return gpuParticleEmitters_; }
    const std::vector<IPostProcessComponent *> &GetPostProcessComponents() const noexcept { return postProcessComponents_; }
    /// @brief 指定オーナーオブジェクトに付与されたポストエフェクトコンポーネントのみを取得する
    std::vector<IPostProcessComponent *> GetPostProcessComponentsFor(const EmptyObject *ownerObject) const;

    /// @brief 登録済みの全SkinnedMeshRendererのアニメーション姿勢をバインドポーズへ戻す
    /// @details ゲームループ停止時（Scene::PlayStop）に呼ばれる
    void ResetAllSkinnedMeshRendererPoses();

    /// @brief ソート済み描画リストを構築して返す
    /// @details 描画先→パイプラインの描画優先度→メッシュ→マテリアルの順でソートされる
    /// @param pipelineManager パイプラインの描画優先度取得用
    const std::vector<DrawEntry> &BuildSortedDrawList(Passkey<Renderer>, PipelineManager *pipelineManager);

    /// @brief 描画リストのキャッシュを次回のBuildSortedDrawList呼び出し時に再構築させる
    /// @details パイプライン名・メッシュ・マテリアル・描画先対象の指定など、ソート結果に影響する
    ///          プロパティが変更された際にMesh/SpriteRenderer側から呼ばれる。
    ///          アクティブ状態の切り替えやワールド行列の変化はキャッシュ有無に関わらず毎フレーム
    ///          再確認されるため、ここでの呼び出しは不要
    void MarkDrawListDirty() noexcept { drawListDirty_ = true; }

    /// @brief 描画先からその描画先を所有するオブジェクトを取得（BuildSortedDrawList 後に有効）
    /// @details エディター用描画先（editorTarget_）はBuildSortedDrawList側で意図的に
    ///          targetOwners_へ登録されない（全MeshRenderer等が無条件でこの描画先へも
    ///          描画されるため、単一の「所有者」を一意に決められない）。
    ///          そのままだとPostProcess側がオーナー未設定として弾いてしまい、シーンビューの
    ///          ScreenBufferへポストエフェクト（カメラ情報を要するものに限らず）が一切
    ///          適用できなくなるため、SetEditorTargetで明示的に指定されたオーナー
    ///          （SceneEditorViewの「Scene View」オブジェクト）を優先して返す
    EmptyObject *GetTargetOwner(const IRenderTarget *target) const {
        if (editorTarget_ && target == editorTarget_ && editorTargetOwner_) {
            return editorTargetOwner_;
        }
        auto it = targetOwners_.find(target);
        return it != targetOwners_.end() ? it->second : nullptr;
    }

    /// @brief 描画先オブジェクトに付与された全描画先コンポーネントから IRenderTarget を収集する
    static void CollectRenderTargets(EmptyObject *targetObject, std::vector<IRenderTarget *> &out);

    //==================================================
    // エディター用描画先
    //==================================================

    /// @brief エディターカメラの情報（シャドウマップのカスケード計算等で使用する）
    struct EditorCameraInfo {
        Matrix4x4 viewProjection = Matrix4x4::Identity();
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        float nearClip = 0.1f;
        float farClip = 1000.0f;
        bool valid = false;
    };

    /// @brief エディター用描画先を設定する（全MeshRendererがこの描画先にも描画される）
    /// @param target エディター用描画先（nullptrで解除）
    /// @param cameraBuffer この描画先の描画時にバインドされるカメラ定数バッファ
    /// @param cameraInfo エディターカメラの情報（シャドウマップ計算用。未指定の場合は無効扱い）
    /// @param owner GetTargetOwner()が返すオーナーオブジェクト（ポストエフェクトの適用対象を
    ///        絞り込むために使われる。通常はSceneEditorViewが自動生成する「Scene View」オブジェクト）
    void SetEditorTarget(IRenderTarget *target, ConstantBufferResource *cameraBuffer,
        const EditorCameraInfo &cameraInfo = {}, EmptyObject *owner = nullptr) {
        if (editorTarget_ != target) {
            // キャッシュ済みエントリは全てeditorTarget_を指しているため、対象が変わったら作り直す
            drawListDirty_ = true;
        }
        editorTarget_ = target;
        editorCameraBuffer_ = cameraBuffer;
        editorCameraInfo_ = cameraInfo;
        editorTargetOwner_ = owner;
    }
    /// @brief 指定描画先がエディター用描画先の場合、そのカメラ定数バッファを返す
    ConstantBufferResource *GetEditorCameraBuffer(const IRenderTarget *target) const {
        return (editorTarget_ && target == editorTarget_) ? editorCameraBuffer_ : nullptr;
    }
    /// @brief 指定描画先がエディター用描画先の場合、そのカメラ情報を返す（それ以外は nullptr）
    const EditorCameraInfo *GetEditorCameraInfo(const IRenderTarget *target) const {
        return (editorTarget_ && target == editorTarget_ && editorCameraInfo_.valid) ? &editorCameraInfo_ : nullptr;
    }
    /// @brief エディター用描画先を取得する（未設定の場合は nullptr）
    IRenderTarget *GetEditorTarget() const noexcept { return editorTarget_; }

    /// @brief エディターのデバッグ表示設定（グリッド/当たり判定の可視化）を登録する
    /// @details SceneEditorViewが毎フレーム呼び、Rendererがエディター用描画先の描画時に参照する
    void SetEditorDebugDraw(EditorDebugDrawSettings settings) { editorDebugDraw_ = std::move(settings); }
    const EditorDebugDrawSettings &GetEditorDebugDraw() const noexcept { return editorDebugDraw_; }

    /// @brief エディターのシーンビューで選択中オブジェクトへ付与する押し出しアウトラインの対象を設定する
    /// @details 対象オブジェクト自身、またはその子孫が持つ全MeshRendererに、既存の押し出しアウトライン
    ///          パイプライン（Object3D.Outline）を固定色で流用描画する。対象オブジェクト自身のマテリアルが
    ///          持つoutlineWidth設定（MaterialWantsOutline）とは完全に独立しており、editorTarget_
    ///          （シーンビュー用描画先）にのみ適用される。ゲーム画面や他の描画先には一切影響しない
    /// @param selectedObjects 選択中オブジェクト（空集合を渡すとアウトラインは描画されない）
    void SetEditorSelectedObjects(std::unordered_set<EmptyObject *> selectedObjects) {
        editorSelectedObjects_ = std::move(selectedObjects);
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    std::vector<MeshRenderer *> meshRenderers_;
    std::vector<SpriteRenderer *> spriteRenderers_;
    std::vector<TextRenderer *> textRenderers_;
    std::vector<SkinnedMeshRenderer *> skinnedMeshRenderers_;
    std::vector<CameraRenderer *> cameraRenderers_;
    std::vector<LightRenderer *> lightRenderers_;
    std::vector<ParticleSystemBase *> gpuParticleEmitters_;
    std::vector<IPostProcessComponent *> postProcessComponents_;

    std::vector<DrawEntry> sortedDrawList_;
    std::unordered_map<const IRenderTarget *, EmptyObject *> targetOwners_;

    /// @brief キャッシュ（targetObjectID未指定のMesh/SpriteRenderer分）の再構築が必要かどうか
    /// @details 初回は必ず構築されるようtrueで開始する
    bool drawListDirty_ = true;
    /// @brief キャッシュ済みエントリ（常にeditorTarget_のみに対して描画する。ソート済み）
    std::vector<CachedRankedEntry> cachedEntries_;

    /// @brief drawListDirty_==trueの場合にcachedEntries_を作り直す
    void RebuildCachedEntries(PipelineManager *pipelineManager);

    IRenderTarget *editorTarget_ = nullptr;
    ConstantBufferResource *editorCameraBuffer_ = nullptr;
    EditorCameraInfo editorCameraInfo_;
    /// @brief editorTarget_のオーナーオブジェクト（GetTargetOwner参照。ポストエフェクトの適用対象の絞り込みに使う）
    EmptyObject *editorTargetOwner_ = nullptr;
    EditorDebugDrawSettings editorDebugDraw_;

    /// @brief エディターのシーンビューでアウトライン表示する対象（SetEditorSelectedObjects参照）
    std::unordered_set<EmptyObject *> editorSelectedObjects_;
    /// @brief 選択アウトライン専用の内部マテリアル（初回のBuildSortedDrawListで遅延生成する）。
    ///        名前を"__"で始めることで、マテリアル一覧・保存対象から除外される（IsInternalMaterialName参照）
    MaterialManager::MaterialHandle editorSelectionOutlineMaterial_ = MaterialManager::kInvalidHandle;
    /// @brief editorSelectionOutlineMaterial_を必要に応じて生成し、そのハンドルを返す
    MaterialManager::MaterialHandle EnsureEditorSelectionOutlineMaterial();
};

REGISTER_COMPONENT_SCENE(SceneRenderer)

} // namespace KashipanEngine
