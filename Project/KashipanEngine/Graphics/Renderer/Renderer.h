#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Utilities/Passkeys.h"
#include "Scene/Components/Render/SceneRenderer.h"

namespace KashipanEngine {

class DepthStencilResource;
class DirectXCommon;
class DX12Commands;
class GraphicsEngine;
class LightRenderer;
class PipelineManager;
class PipelineBinder;
class ResourceContainer;
class SceneContext;
class ScreenBuffer;
class ShaderVariableBinder;
class IRenderTarget;

/// @brief 描画用のレンダラークラス
/// @details SceneContext から SceneRenderer コンポーネントを取得し、
///          SceneRenderer が構築するソート済み描画リストを元に描画処理を行う。
class Renderer final {
public:
    Renderer(Passkey<GraphicsEngine>, DirectXCommon *directXCommon, PipelineManager *pipelineManager);
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    Renderer &operator=(Renderer &&) = delete;

    /// @brief フレーム描画処理
    /// @param sceneContext 描画対象シーンのコンテキスト
    void RenderFrame(Passkey<GraphicsEngine>, SceneContext *sceneContext);

    /// @brief GPUリソースの全開放
    void ReleaseAllResources(Passkey<GraphicsEngine>);

    /// @brief 直近のRenderFrameで実際に発行されたDrawIndexedInstanced呼び出し回数
    /// @details パフォーマンス調査用（インスタンシングでバッチがまとまらず想定より
    ///          ドローコールが分割されていないかを確認する目的）
    std::uint32_t GetLastFrameDrawCallCount() const { return drawCallCount_; }

    /// @brief カスケードシャドウ（Directionalライト）のカスケード数
    static constexpr std::uint32_t kShadowCascadeCount = 4;
    /// @brief 1描画先で同時に影を生成できるライト数の上限（定数バッファサイズによる実質上限）
    static constexpr std::uint32_t kMaxShadowLightsPerTarget = 16;
    /// @brief 1ライトあたりのビュー射影行列の最大数（ポイントライトのキューブ6面が最大）
    static constexpr std::uint32_t kMaxShadowViewProjections = 6;

    //==================================================
    // Forward+（タイルドライトカリング）
    //==================================================

    /// @brief ライトカリングのタイルサイズ（ピクセル）
    static constexpr std::uint32_t kTileSize = 16;
    /// @brief 1タイルあたりに保持できるライトの最大数
    static constexpr std::uint32_t kMaxLightsPerTile = 256;

private:
    /// @brief シャドウマップ描画ジョブ1件分のデータ
    /// @details Directionalは（ライト×描画先カメラ）ごと、Spot/Pointはライトごとに1ジョブ。
    ///          シャドウマップ配列内のスライス範囲 [baseSlice, baseSlice + sliceCount) を占有する
    struct ShadowJobData {
        Matrix4x4 viewProjections[kMaxShadowViewProjections];
        float cascadeSplits[kShadowCascadeCount]{};
        /// @brief カスケードごとの深度バイアス係数（1テクセルのワールドサイズをNDC深度へ換算した値。Directional用）
        float cascadeBiasScales[kShadowCascadeCount]{};
        /// @brief 透視投影の深度バイアス係数（シェーダー側でビュー深度で割って1テクセル相当のNDCバイアスになる。Spot/Point用）
        float perspectiveBiasScale = 0.0f;
        std::uint32_t baseSlice = 0;
        std::uint32_t sliceCount = 0;
        int lightType = 0;  ///< 0: Directional / 1: Spot / 2: Point（HLSL側と対応）
    };
    /// @brief 描画先ごとの「影を生成するライト」の割り当て（並び順がシェーダーのshadowMapIndexに対応）
    struct TargetShadowEntry {
        const LightRenderer *lightRenderer = nullptr;
        int jobIndex = -1;
    };

    /// @brief シーン内のComputeShaderProcessingコンポーネントを処理する（Dispatch実行）
    /// @details 専用コマンドリスト（ComputeCommandProcessor）上で全てまとめて記録・実行される
    void ProcessComputeShaders(SceneContext *sceneContext);

    /// @brief 描画先ごとのシャドウマップ描画パスを実行する
    /// @details 各描画先について「その描画先で使うカメラ」と「その描画先に適用されるライト」から
    ///          影を生成するライトを収集し（多すぎる場合はカメラに近い順に優先）、
    ///          Directional=4カスケード / Spot=1面 / Point=キューブ6面 のシャドウマップを
    ///          1つのTexture2DArrayへまとめて描画する
    void RenderShadowMaps(SceneContext *sceneContext, SceneRenderer *sceneRenderer,
        const std::vector<IRenderTarget *> &targets);

    /// @brief シーン内のSkinnedMeshRendererに対してGPUスキニング（Computeシェーダー）を実行する
    /// @details 描画リスト構築・描画より先に実行し、各インスタンスのスキニング結果バッファを
    ///          後続の描画パスで頂点バッファとして参照できるようにする
    void ProcessSkinning(SceneContext *sceneContext);

    /// @brief GPU Simulation有効なParticleSystem2D/3Dに対してコンピュートシェーダー
    ///        （ParticleSpawn→ParticleUpdate）を実行する
    /// @details 描画リスト構築より先に実行し、gInstanceMatricesを後続のRenderGpuParticlesが
    ///          頂点シェーダーのgTransformationMatricesとしてそのまま参照する
    void ProcessGpuParticles(SceneContext *sceneContext);

    /// @brief タイル単位のライトカリング（Forward+）を実行する
    /// @details 描画リスト構築後、RenderShadowMaps/RenderToTargetより前に呼ぶ。3D描画に使われる
    ///          (描画先, パイプライン名) の組ごとに、Point/Spotライトをタイル分割してカリングし、
    ///          結果をタイルごとのライトインデックス配列としてGPUへ書き出す。
    ///          BindLightBuffersAndShadowMapが同じキーでこの結果を読み出してバインドする
    void ProcessLightCulling(SceneContext *sceneContext, SceneRenderer *sceneRenderer,
        std::span<const SceneRenderer::DrawEntry> drawList);

    /// @brief 単一の描画先への描画処理
    void RenderToTarget(IRenderTarget *target,
        std::span<const SceneRenderer::DrawEntry> entries,
        SceneRenderer *sceneRenderer);

    /// @brief 同一（パイプライン・メッシュ・マテリアル）バッチのインスタンシング描画
    void DrawBatch(IRenderTarget *target,
        PipelineBinder &pipelineBinder,
        std::span<const SceneRenderer::DrawEntry> batch,
        SceneRenderer *sceneRenderer);

    /// @brief TextRendererの専用描画パス
    /// @details 文字ごとにアトラス内UVが異なるためDrawBatchのバッチングには乗せず、
    ///          (パイプライン名, フォントハンドル) の組ごとに全TextRendererの文字インスタンスを
    ///          まとめて1回のDrawIndexedInstancedで描画する。RenderToTargetの通常バッチ描画の後に呼ぶ
    void RenderTextRenderers(IRenderTarget *target,
        PipelineBinder &pipelineBinder,
        SceneRenderer *sceneRenderer);

    /// @brief GPU Simulation有効なParticleSystem2D/3Dの専用描画パス
    /// @details ProcessGpuParticlesが書き込んだgInstanceMatricesをSRVとして
    ///          そのままgTransformationMatricesにバインドし、既存のObject2D/Object3D
    ///          頂点シェーダーを無改造で利用する。常にプール容量分描画し、死亡中の
    ///          パーティクルはスケール0の行列で非表示になる（コンパクションは行わない）。
    ///          RenderToTargetの通常バッチ描画・RenderTextRenderersの後に呼ぶ
    void RenderGpuParticles(IRenderTarget *target,
        PipelineBinder &pipelineBinder,
        SceneRenderer *sceneRenderer);

    /// @brief 指定描画先・パイプラインに対するカメラ・ライトの定数バッファバインド
    void BindCameraAndLights(ID3D12GraphicsCommandList *commandList,
        IRenderTarget *target,
        const std::string &pipelineName,
        SceneRenderer *sceneRenderer);

    /// @brief ポイント/スポットライトの構造化バッファ・個数定数・シャドウマップのバインド
    void BindLightBuffersAndShadowMap(IRenderTarget *target,
        const std::string &pipelineName,
        SceneRenderer *sceneRenderer,
        ShaderVariableBinder &shaderBinder);

    /// @brief ScreenBuffer へのポストプロセス適用
    /// @param ownerObject ScreenBuffer を所有するオブジェクト（ポストエフェクトコンポーネントの取得元）
    void RenderPostProcess(ScreenBuffer *screenBuffer,
        PipelineBinder &pipelineBinder,
        EmptyObject *ownerObject);

    /// @brief エディター用描画先にのみ、デバッグ表示（グリッド・当たり判定）を描画する
    /// @details target がエディター用描画先でない場合は何もしない
    void RenderEditorDebugOverlay(ScreenBuffer *screenBuffer,
        PipelineBinder &pipelineBinder,
        SceneRenderer *sceneRenderer);

    /// @brief エディター用描画先にのみ、他の描画より先に背景（単色 or テクスチャ）を描画する
    /// @details target がエディター用描画先でない場合は何もしない
    void RenderEditorBackground(ScreenBuffer *screenBuffer,
        PipelineBinder &pipelineBinder,
        SceneRenderer *sceneRenderer);

    /// @brief 描画リストに含まれない ScreenBuffer へのポストエフェクトのみの適用
    /// @details オブジェクトの描画が無くともポストエフェクトコンポーネントがあれば実行する
    void RenderPostProcessOnlyTargets(SceneContext *sceneContext,
        const std::unordered_set<const IRenderTarget *> &renderedTargets);

    /// @brief DirectX共通クラスへのポインタ
    DirectXCommon *directXCommon_ = nullptr;
    /// @brief パイプラインマネージャーへのポインタ
    PipelineManager *pipelineManager_ = nullptr;
    /// @brief GPUリソースキャッシュ
    std::unique_ptr<ResourceContainer> resourceContainer_;

    //==================================================
    // シャドウマップ
    //==================================================

    /// @brief 今フレームのシャドウマップ描画ジョブ
    std::vector<ShadowJobData> shadowJobs_;
    /// @brief 描画先ごとの「影を生成するライト」割り当て
    std::unordered_map<const IRenderTarget *, std::vector<TargetShadowEntry>> targetShadowEntries_;
    /// @brief 全シャドウマップをスライスとしてまとめたTexture2DArray（エンジン内部で自動生成・管理）
    std::unique_ptr<DepthStencilResource> shadowMapArray_;
    std::uint32_t shadowArrayResolution_ = 0;
    std::uint32_t shadowArraySliceCount_ = 0;
    /// @brief シャドウマップ配列がシェーダーから参照可能な状態（SRV遷移済み）かどうか
    bool shadowArrayReady_ = false;
    /// @brief シャドウパス記録用のコマンドスロット
    int shadowCommandSlotIndex_ = -1;
    DX12Commands *shadowCommands_ = nullptr;

    /// @brief GPUパーティクル用コンピュートディスパッチ記録専用のコマンドスロット
    /// @details ComputeCommandProcessorの共有コマンドリストは1フレーム中に複数回
    ///          （ComputeShaderProcessing/Skinning/LightCullingなど）Begin/EndRecordされる
    ///          設計になっており、同じコマンドリストを使い回すとBeginRecord内のReset()で
    ///          直前のフェーズが記録した（まだ提出されていない）内容が上書きされてしまう。
    ///          GPUパーティクルのDispatchは専用のコマンドリストへ独立して記録することでこれを回避する
    int particleComputeCommandSlotIndex_ = -1;
    DX12Commands *particleComputeCommands_ = nullptr;

    /// @brief 直近のRenderFrameで発行されたDrawIndexedInstanced呼び出し回数（RenderFrame冒頭でリセットする）
    std::uint32_t drawCallCount_ = 0;
};

} // namespace KashipanEngine
