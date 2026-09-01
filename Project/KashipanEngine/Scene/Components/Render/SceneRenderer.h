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
#include "Math/Vector2.h"
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
class ScreenBufferViewport;
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
        /// @brief gTextureへバインドするテクスチャの上書き。TextRendererは任意マテリアルの
        ///        テクスチャを無視し、ここへフォントアトラスを指定する。
        TextureManager::TextureHandle textureOverrideHandle = TextureManager::kInvalidHandle;
        /// @brief gSamplerへバインドするサンプラーの上書き。TextRendererのアトラスにはLinearClampを使う。
        SamplerManager::SamplerHandle samplerOverrideHandle = SamplerManager::kInvalidHandle;
        /// @brief 描画するインデックス範囲（サブメッシュ。indexCount==0の場合はメッシュ全体を描画する）
        std::uint32_t indexStart = 0;
        std::uint32_t indexCount = 0;
        Matrix4x4 worldMatrix = Matrix4x4::Identity();
        /// @brief SkinnedMeshRendererから作られたエントリのみ非null。
        ///        非nullの場合、頂点バッファは静的メッシュではなくこのGPUスキニング結果を使用し、
        ///        インスタンス（バッチ）結合の対象にもならない（各インスタンスが専用の出力バッファを持つため）
        RWStructuredBufferResource *skinnedVertexBuffer = nullptr;
        /// @brief オブジェクト単位の色（MeshRenderer/SkinnedMeshRenderer/SpriteRenderer/TextRendererのInstance Color）
        Vector4 instanceColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        /// @brief instanceColorの適用方法（各RendererのColorBlendModeの値。
        ///        0=Override,1=Multiply,2=Add,3=Subtract）
        int instanceColorBlendMode = 1;
        /// @brief オブジェクト単位のUVオフセット（MeshRenderer/SpriteRenderer/SkinnedMeshRendererの
        ///        InstanceUvTranslate。マテリアルのUV変換とinstanceUvCombineModeで合成される。既定(0,0)）
        Vector2 instanceUvTranslate{ 0.0f, 0.0f };
        /// @brief オブジェクト単位のUV回転（ラジアン。既定0）
        float instanceUvRotation = 0.0f;
        /// @brief オブジェクト単位のUVスケール（既定(1,1)）
        Vector2 instanceUvScale{ 1.0f, 1.0f };
        /// @brief オブジェクト単位のUV回転の中心座標（UV基準。既定は中心(0.5, 0.5)）
        Vector2 instanceUvPivot{ 0.5f, 0.5f };
        /// @brief instanceUvTranslate/Rotation/ScaleとマテリアルのUV変換の合成方法（各RendererのUVCombineModeの値。
        ///        0=MaterialThenInstance,1=InstanceThenMaterial,2=InstanceOnly）
        int instanceUvCombineMode = 0;
        /// @brief オブジェクト固有のシード値（EmptyObject::GetObjectID()から導出）。ドローコールを
        ///        またいでもシーン内で一意になるため、SV_InstanceIDのようなドローコールローカルな値や
        ///        ワールド座標とは異なり、原点が一致する別オブジェクト同士でも確実に異なる値になる。
        ///        半透明のディザリングパターンをオブジェクトごとに分離する用途で使う（ObjectPS.hlsl参照）
        float objectIdSeed = 0.0f;
        /// @brief このエントリを他のエントリとインスタンシング（1回のDrawIndexedInstancedへのバッチ結合）
        ///        の対象にしてよいか。false の場合、パイプライン/メッシュ/マテリアルが完全に一致する
        ///        隣接エントリがあっても常に単独のドローコールとして描画される
        ///        （MeshRenderer/SpriteRenderer/TextRendererのAllowInstancing参照）
        bool allowInstancing = true;
        /// @brief TextRendererのみ使用。アトラス内のUV矩形（x0,y0,x1,y1）。既定は全面（0,0,1,1）。
        ///        Text系パイプラインのMaterial構造体にのみ存在する"uvRect"フィールドへ
        ///        WriteMaterialFieldでインスタンスごとに書き込まれる（他の型・パイプラインでは無視される）
        Vector4 uvRect{ 0.0f, 0.0f, 1.0f, 1.0f };
        /// @brief TextRendererのみ使用。<b>タグによる太らせ量（SDF閾値のシフト量）。既定0
        float boldWeight = 0.0f;
        /// @brief TextRendererのみ使用。SDF輪郭から外側へ広げるアウトライン幅（SDF正規化値）
        float textOutlineWidth = 0.0f;
        /// @brief TextRendererのみ使用。アウトライン色
        Vector4 textOutlineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    /// @brief DrawEntryにソートキー（描画先種別順・パイプライン優先度・RenderPriority）を付随させた中間データ
    struct RankedDrawEntry {
        DrawEntry entry;
        int kindOrder = 0;
        std::int32_t pipelinePriority = 0;
        /// @brief 各Rendererコンポーネントが持つRenderPriority（既定0）。値が小さいほど先に描画され、
        ///        大きいほど後（手前）に描画される。既定値0同士は常にタイになるため、この値を
        ///        変更しない限り既存のパイプライン名/メッシュ/マテリアル単位の並び（＝インスタンシングの
        ///        バッチ化）は一切影響を受けない
        std::int32_t renderPriority = 0;
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

    /// @brief オブジェクト固有のディザ用シード値（EmptyObject::GetObjectID()から導出）を求める
    /// @details 通常描画のDrawEntry構築時だけでなく、RendererShadow.cppのシャドウマップ描画でも
    ///          同じ値を使い、本体と影のディザ位相を揃えるために公開している
    static float ObjectIdSeedFor(const EmptyObject *owner);

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
    /// @brief ScreenBufferViewportを登録する（ScreenBufferViewport::Initialize/Finalizeから呼ばれる）
    /// @details UIButton/MeshButtonが、描画先としてWindowを持たないScreenBufferObjectを対象にした際に、
    ///          「そのScreenBufferを最終的にどのWindowへ中継表示しているか」を逆引きするために使う
    ///          （全シーンオブジェクトを走査せず、登録済みの一覧だけを見れば済むようにするため）
    void RegisterScreenBufferViewport(ScreenBufferViewport *viewport);
    void UnregisterScreenBufferViewport(const ScreenBufferViewport *viewport);

    const std::vector<MeshRenderer *> &GetMeshRenderers() const noexcept { return meshRenderers_; }
    const std::vector<SpriteRenderer *> &GetSpriteRenderers() const noexcept { return spriteRenderers_; }
    const std::vector<TextRenderer *> &GetTextRenderers() const noexcept { return textRenderers_; }
    const std::vector<SkinnedMeshRenderer *> &GetSkinnedMeshRenderers() const noexcept { return skinnedMeshRenderers_; }
    const std::vector<CameraRenderer *> &GetCameraRenderers() const noexcept { return cameraRenderers_; }
    const std::vector<LightRenderer *> &GetLightRenderers() const noexcept { return lightRenderers_; }
    const std::vector<ParticleSystemBase *> &GetGpuParticleEmitters() const noexcept { return gpuParticleEmitters_; }
    const std::vector<IPostProcessComponent *> &GetPostProcessComponents() const noexcept { return postProcessComponents_; }
    const std::vector<ScreenBufferViewport *> &GetScreenBufferViewports() const noexcept { return screenBufferViewports_; }
    /// @brief 指定オーナーオブジェクトに付与されたポストエフェクトコンポーネントのみを取得する
    std::vector<IPostProcessComponent *> GetPostProcessComponentsFor(const EmptyObject *ownerObject) const;

    /// @brief 登録済みの全SkinnedMeshRendererのアニメーション姿勢をバインドポーズへ戻す
    /// @details ゲームループ停止時（Scene::PlayStop）に呼ばれる
    void ResetAllSkinnedMeshRendererPoses();

    /// @brief ソート済み描画リストを構築して返す
    /// @details 描画先→パイプラインの描画優先度→RenderPriority（各Rendererコンポーネントが持つ値。
    ///          小さいほど先＝奥、大きいほど後＝手前）→メッシュ→マテリアルの順でソートされる
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

    /// @brief シーンビューに表示・選択・ギズモ編集の対象とするオブジェクトの種類
    /// @details SceneEditorViewのツールバーから切り替えられる。あくまでエディターのシーンビュー
    ///          （editorTarget_）の表示・ピッキング対象を絞り込むだけで、実際のゲーム画面には
    ///          一切影響しない
    enum class EditorDisplayMode {
        Combined,  ///< 2D/3D両方を表示・選択対象にする（既定）
        ThreeDOnly, ///< 3Dオブジェクト（MeshRenderer/SkinnedMeshRenderer）のみを表示・選択対象にする
        TwoDOnly,  ///< 2Dオブジェクト（SpriteRenderer）のみを表示・選択対象にする
    };

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

    /// @brief 「2D」表示モード専用のパン・ズームカメラの定数バッファ（gCamera2D相当）を設定する
    /// @details SceneEditorViewが毎フレーム呼ぶ。シーン内の実際のCamera2Dコンポーネントとは独立しており、
    ///          表示モードがTwoDOnlyの間だけGetEditorCamera2DBufferが非nullを返す
    void SetEditorCamera2DBuffer(ConstantBufferResource *cameraBuffer) { editorCamera2DBuffer_ = cameraBuffer; }
    /// @brief 指定描画先がエディター用描画先、かつ表示モードがTwoDOnlyの場合のみカメラ定数バッファを返す
    ConstantBufferResource *GetEditorCamera2DBuffer(const IRenderTarget *target) const {
        return (editorTarget_ && target == editorTarget_ && editorDisplayMode_ == EditorDisplayMode::TwoDOnly)
            ? editorCamera2DBuffer_ : nullptr;
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

    /// @brief シーンビューの表示モード（2D/3D併用・3Dのみ・2Dのみ）を設定する
    /// @details SceneEditorViewのツールバーから毎フレーム呼ばれる。変更時はキャッシュ済みエントリ
    ///          （targetObjectID未指定のMesh/SpriteRenderer分）を作り直す必要がある
    void SetEditorDisplayMode(EditorDisplayMode mode) {
        if (editorDisplayMode_ != mode) drawListDirty_ = true;
        editorDisplayMode_ = mode;
    }
    EditorDisplayMode GetEditorDisplayMode() const noexcept { return editorDisplayMode_; }

    /// @brief エディターのシーンビューで選択中オブジェクトへ付与する押し出しアウトラインの対象を設定する
    /// @details 対象オブジェクト自身、またはその子孫が持つ全MeshRendererに、既存の押し出しアウトライン
    ///          パイプライン（Object3D.Outline）を固定色で流用描画する。対象オブジェクト自身のマテリアルが
    ///          持つoutlineWidth設定（MaterialWantsOutline）とは完全に独立しており、editorTarget_
    ///          （シーンビュー用描画先）にのみ適用される。ゲーム画面や他の描画先には一切影響しない
    /// @param selectedObjects 選択中オブジェクト（空集合を渡すとアウトラインは描画されない）
    void SetEditorSelectedObjects(std::unordered_set<EmptyObject *> selectedObjects) {
        editorSelectedObjects_ = std::move(selectedObjects);
    }

    /// @brief Prefabのシーンビューへのドラッグ中プレビュー用の1メッシュ分の情報
    /// @details 実際のシーンオブジェクト（EmptyObject/MeshRenderer）を介さず、メッシュハンドルと
    ///          ワールド行列を直接指定する（ドラッグ中は配置が未確定のため）
    struct GhostPreviewMesh {
        ModelManager::ModelHandle meshHandle = ModelManager::kInvalidHandle;
        Matrix4x4 worldMatrix = Matrix4x4::Identity();
    };

    /// @brief エディターのシーンビューへドラッグ中のPrefabプレビューメッシュを設定する
    /// @details 半透明の専用パイプライン（Object3D.Ghost）で、editorTarget_（シーンビュー用描画先）
    ///          にのみ描画される。選択アウトライン（SetEditorSelectedObjects）と同様、
    ///          ゲーム画面や他の描画先には一切影響しない
    /// @param meshes プレビューするメッシュ一覧（空にするとプレビューは表示されない）
    void SetEditorGhostPreviewMeshes(std::vector<GhostPreviewMesh> meshes) {
        editorGhostPreviewMeshes_ = std::move(meshes);
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
    std::vector<ScreenBufferViewport *> screenBufferViewports_;

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
    ConstantBufferResource *editorCamera2DBuffer_ = nullptr;
    EditorCameraInfo editorCameraInfo_;
    /// @brief editorTarget_のオーナーオブジェクト（GetTargetOwner参照。ポストエフェクトの適用対象の絞り込みに使う）
    EmptyObject *editorTargetOwner_ = nullptr;
    EditorDebugDrawSettings editorDebugDraw_;
    EditorDisplayMode editorDisplayMode_ = EditorDisplayMode::Combined;

    /// @brief エディターのシーンビューでアウトライン表示する対象（SetEditorSelectedObjects参照）
    std::unordered_set<EmptyObject *> editorSelectedObjects_;
    /// @brief 選択アウトライン専用の内部マテリアル（初回のBuildSortedDrawListで遅延生成する）。
    ///        名前を"__"で始めることで、マテリアル一覧・保存対象から除外される（IsInternalMaterialName参照）
    MaterialManager::MaterialHandle editorSelectionOutlineMaterial_ = MaterialManager::kInvalidHandle;
    /// @brief editorSelectionOutlineMaterial_を必要に応じて生成し、そのハンドルを返す
    MaterialManager::MaterialHandle EnsureEditorSelectionOutlineMaterial();

    /// @brief エディターのシーンビューへドラッグ中のPrefabプレビューメッシュ（SetEditorGhostPreviewMeshes参照）
    std::vector<GhostPreviewMesh> editorGhostPreviewMeshes_;
    /// @brief プレビュー用の内部マテリアル（初回のBuildSortedDrawListで遅延生成する）。
    ///        名前を"__"で始めることで、マテリアル一覧・保存対象から除外される（IsInternalMaterialName参照）
    MaterialManager::MaterialHandle editorGhostPreviewMaterial_ = MaterialManager::kInvalidHandle;
    /// @brief editorGhostPreviewMaterial_を必要に応じて生成し、そのハンドルを返す
    MaterialManager::MaterialHandle EnsureEditorGhostPreviewMaterial();
};

REGISTER_COMPONENT_SCENE(SceneRenderer)

} // namespace KashipanEngine
