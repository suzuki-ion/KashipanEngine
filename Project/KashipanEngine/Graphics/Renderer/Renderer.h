#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Utilities/Passkeys.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Graphics/BlueNoiseGenerator.h"
#include "Graphics/Renderer/ResourceContainer.h"
#include "Assets/SamplerManager.h"
#include "Assets/TextureManager.h"

namespace KashipanEngine {

class ConstantBufferResource;
class DepthStencilResource;
class DirectXCommon;
class DX12Commands;
class EmptyObject;
class GraphicsEngine;
class LightRenderer;
class PipelineManager;
class PipelineBinder;
class RenderTargetResource;
class RWStructuredBufferResource;
class ResourceContainer;
class SceneContext;
class ScreenBuffer;
class ShaderResourceResource;
class ShaderVariableBinder;
class StructuredBufferResource;
class IRenderTarget;

namespace RendererInternal {
struct MultiPassDitherScratchSet;
} // namespace RendererInternal

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
    static constexpr std::uint32_t kMaxShadowLightsPerTarget = 32;
    /// @brief 1ライトあたりのビュー射影行列の最大数（ポイントライトのキューブ6面が最大）
    static constexpr std::uint32_t kMaxShadowViewProjections = 6;

    //==================================================
    // Forward+（タイルドライトカリング）
    //==================================================

    /// @brief ライトカリングのタイルサイズ（ピクセル）
    static constexpr std::uint32_t kTileSize = 16;
    /// @brief 1タイルあたりに保持できるライトの最大数
    static constexpr std::uint32_t kMaxLightsPerTile = 1024;

private:
    /// @brief シャドウマップ描画ジョブ1件分のデータ
    /// @details Directionalは（ライト×描画先カメラ）ごと、Spot/Pointはライトごとに1ジョブ。
    ///          シャドウマップ配列内のスライス範囲 [baseSlice, baseSlice + sliceCount) を占有する
    struct ShadowJobData {
        Matrix4x4 viewProjections[kMaxShadowViewProjections];
        /// @brief 各ビュー射影に対応するライト（視点）のワールド座標。シャドウマップPS側で
        ///        本体と同じディザ閾値テーブルを参照する際、カメラの代わりに距離の基準として使う
        Vector3 eyePositions[kMaxShadowViewProjections]{};
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

    /// @brief シャドウマップ用に準備済みの描画バッチ1件分（RenderShadowMaps参照）
    /// @details ジョブ構築・メッシュ/マテリアルの収集は1フレームにつき1回（RenderShadowMaps）だけ
    ///          行えばよく、位相（idSeed）だけがパスごとに変わる画面全体Nパスブレンド
    ///          （ScreenWideDitherBlendEffect）では、この構築済み内容を使い回してidSeedバッファだけを
    ///          都度更新し、描画だけを再実行する（RenderShadowMapsPhaseInto参照）
    struct PreparedShadowBatch {
        const ResourceContainer::MeshBuffers *meshBuffers = nullptr;
        RWStructuredBufferResource *skinnedVertexBuffer = nullptr;
        StructuredBufferResource *transformBuffer = nullptr;
        StructuredBufferResource *materialBuffer = nullptr;
        std::uint32_t instanceCount = 0;
        std::uint32_t indexStart = 0;
        std::uint32_t indexCount = 0;
        Vector3 boundsCenter{ 0.0f, 0.0f, 0.0f };
        float boundsRadius = 0.0f;
        /// @brief インスタンスごとの基準idSeed（位相オフセットは再生成のたびに加算する）
        std::vector<float> baseIdSeeds;
        /// @brief idSeedバッファのキャッシュキーの基本部分（位相ごとのサフィックスはRenderShadowMapsPhaseIntoが付与）
        std::string idSeedKeyBase;
    };
    /// @brief GPUパーティクルの影キャスター1エミッター分（RenderShadowMaps参照）
    /// @details GPUパーティクルは本体側もidSeedによる位相分離を持たないため、位相再生成では
    ///          そのまま毎パス再描画するだけでよい（idSeedの更新は不要）
    struct PreparedGpuParticleShadowBatch {
        const ResourceContainer::MeshBuffers *meshBuffers = nullptr;
        RWStructuredBufferResource *transformBuffer = nullptr;
        StructuredBufferResource *materialBuffer = nullptr;
        std::uint32_t instanceCount = 0;
    };

    /// @brief 画面全体Nパスブレンド（ScreenWideDitherBlendEffect）の対象1件分
    struct ScreenWideDitherRequest {
        ScreenBuffer *screenBuffer = nullptr;
        std::uint32_t passCount = 4;
    };

    /// @brief シーン内のComputeShaderProcessingコンポーネントを処理する（Dispatch実行）
    /// @details ComputeCommandProcessorが管理するフレーム共有コマンドリストへ記録する
    void ProcessComputeShaders(SceneContext *sceneContext);

    /// @brief 再生中の動画のうち、GPUへ新しいNV12フレームがアップロードされたものについて
    ///        コンピュートシェーダー(VideoYUVToRGBパイプライン)でYUV→RGB変換を行う
    /// @details シーンに依存しない（VideoManagerが管理する全VideoPlayer横断）。
    ///          描画リスト構築より先に実行し、変換結果を通常のマテリアルテクスチャとして
    ///          後続の描画パスから参照できるようにする
    void ProcessVideoConversions();

    /// @brief 描画先ごとのシャドウマップ描画パスを実行する
    /// @details 各描画先について「その描画先で使うカメラ」と「その描画先に適用されるライト」から
    ///          影を生成するライトを収集し（多すぎる場合はカメラに近い順に優先）、
    ///          Directional=4カスケード / Spot=1面 / Point=キューブ6面 のシャドウマップを
    ///          1つのTexture2DArrayへまとめて描画する
    void RenderShadowMaps(SceneContext *sceneContext, SceneRenderer *sceneRenderer,
        const std::vector<IRenderTarget *> &targets);

    /// @brief RenderShadowMapsが構築済みのジョブ・バッチ情報（shadowJobs_/shadowBatches_/
    ///        shadowGpuParticleBatches_）を再利用し、指定の位相オフセットでシャドウマップの
    ///        描画だけを再実行する（画面全体Nパスブレンド専用。ScreenWideDitherBlendEffect参照）
    /// @param screenBuffer 描画先（コマンドリストの取得にのみ使用。影自体は全ターゲット共有の
    ///        shadowMapArray_へ描画するため、この描画先固有の絞り込みは行わない）
    /// @param passIndex idSeedバッファのキャッシュキーへ含めるパス番号（バッファ競合回避用）
    /// @details shadowMapArray_（全描画先で共有）を直接書き換えるため、この関数を呼ぶ描画先
    ///          （画面全体Nパスブレンド対象）は、同一フレーム内で他の描画先より必ず後に処理すること
    ///          （RenderFrame参照）。ジョブ構築（視錐台フィッティング等）はやり直さない
    void RenderShadowMapsPhaseInto(ScreenBuffer *screenBuffer, PipelineBinder &pipelineBinder,
        float phaseOffset, int passIndex);

    /// @brief シャドウ用idSeedバッファを取得・構築する（RenderShadowMaps本体の描画と
    ///        RenderShadowMapsPhaseIntoの両方から呼ばれる共通処理）
    /// @param phaseOffset baseIdSeeds各要素へ加算する位相オフセット（通常描画では0.0）
    /// @param passIndex キャッシュキーへ含めるパス番号（-1の場合は付与しない）
    StructuredBufferResource *BuildShadowIdSeedBuffer(const PreparedShadowBatch &batch, float phaseOffset, int passIndex);

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

    /// @brief 直前にカメラ・ライトをバインドした(パイプライン名, PipelineBinder世代)を記録するキャッシュ
    /// @details 同一の描画先に対する一連の描画（DrawBatchのループ・RenderGpuParticles）の間で
    ///          共有し、パイプラインが実際には切り替わっていない
    ///          （ルート引数がまだ有効な）場合にカメラ・ライトの再バインドを省略するために使う。
    ///          RenderToTarget呼び出しごとに新規に用意すること（フレーム・描画先をまたいで使い回さない）。
    struct CameraLightsBindCache {
        std::string pipelineName;
        std::uint64_t generation = 0;
        bool valid = false;
    };

    /// @brief 単一の描画先への描画処理
    void RenderToTarget(IRenderTarget *target,
        std::span<const SceneRenderer::DrawEntry> entries,
        SceneRenderer *sceneRenderer);

    /// @brief このフレームで画面全体Nパスブレンド（ScreenWideDitherBlendEffect）が有効な
    ///        描画先を収集する。対象が1つも無ければ空を返す（既存の描画経路に一切影響しない）
    std::vector<ScreenWideDitherRequest> CollectScreenWideDitherTargets(
        SceneRenderer *sceneRenderer, const std::vector<IRenderTarget *> &targets);

    /// @brief 画面全体Nパスブレンド対象の描画先1件について、シャドウマップの再生成を含めて
    ///        フレーム全体をpassCount回撮り直し、結果を平均して最終出力へ書き込む
    /// @details RenderShadowMaps/RenderToTargetが共有するshadowMapArray_を直接書き換えながら
    ///          進めるため、呼び出し側（RenderFrame）は、drawList内の他の（画面全体Nパスブレンド
    ///          対象ではない）描画先を全て処理し終えた後にこの関数を呼ぶこと
    void RenderScreenWideDitherTarget(ScreenBuffer *screenBuffer,
        std::span<const SceneRenderer::DrawEntry> entries,
        SceneRenderer *sceneRenderer,
        std::uint32_t passCount);

    /// @brief シーン内容（背景・通常バッチ・オブジェクト単位Nパスディザ・テキスト・GPUパーティクル）
    ///        を描画する。post-process（RenderPostProcess）はここに含まない
    /// @details RenderToTargetの通常経路とRenderScreenWideDitherTargetの両方から呼ばれる共通部分。
    ///          呼び出し前にOMSetRenderTargetsで書き込み先（通常は描画先自身の書き込み中バッファ、
    ///          画面全体Nパスブレンド中はスクラッチバッファ）を設定しておくこと（この関数自体は
    ///          RTV/DSVの設定を行わない）
    /// @param extraSeedOffset 画面全体Nパスブレンド専用: 全エントリのobjectIdSeedへ加算するオフセット
    /// @param seedPassIndex 画面全体Nパスブレンド専用: idSeed用構造化バッファのキャッシュキーへ
    ///        含めるパス番号（-1の場合は付与しない＝通常描画と同じキーのまま）
    /// @param disableNestedMultiPassDither 画面全体Nパスブレンド専用: trueの場合、マテリアルの
    ///        enableMultiPassDitherが有効なエントリもネストしたNパス化はせず、他のエントリと
    ///        同様に1回だけ（このextraSeedOffsetで）描画する。RenderMultiPassDither自体は
    ///        描画先の「実際の」書き込み面（GetWriteRenderTarget）へ最終合成する設計のため、
    ///        画面全体Nパスブレンドのスクラッチ書き込み先とは両立できない（RTV/DSVの取り合いに
    ///        なるため）。画面全体側で既に同等の粒状感低減が働くため、機能的な欠落にはならない
    void RenderSceneContent(IRenderTarget *target,
        std::span<const SceneRenderer::DrawEntry> entries,
        SceneRenderer *sceneRenderer,
        PipelineBinder &pipelineBinder,
        CameraLightsBindCache &lightsCache,
        float extraSeedOffset = 0.0f,
        int seedPassIndex = -1,
        bool disableNestedMultiPassDither = false);

    /// @brief 同一（パイプライン・メッシュ・マテリアル）バッチのインスタンシング描画
    /// @param extraSeedOffset RenderMultiPassDither/画面全体Nパスブレンド専用: 各インスタンスの
    ///        objectIdSeedへ加算するパス固有のオフセット（通常描画では0.0のまま渡す）
    /// @param seedPassIndex RenderMultiPassDither/画面全体Nパスブレンド専用: idSeed用構造化バッファの
    ///        キャッシュキーへ含めるパス番号（-1の場合は付与しない＝通常描画と同じキーのまま）。
    ///        同一フレーム内で位相だけ異なる複数パスを描画する際、バッファを使い回すと
    ///        CPU側の書き込みがGPU実行前に競合してしまうため、パスごとに別バッファへ分離する
    void DrawBatch(IRenderTarget *target,
        PipelineBinder &pipelineBinder,
        std::span<const SceneRenderer::DrawEntry> batch,
        SceneRenderer *sceneRenderer,
        CameraLightsBindCache &lightsCache,
        float extraSeedOffset = 0.0f,
        int seedPassIndex = -1);

    /// @brief 半透明ディザ対象（マテリアルのenableMultiPassDither）を、位相違いでNパス描画して
    ///        結果をブレンドし、粒状感を1フレーム内で滑らかにする（ScreenBuffer限定）
    /// @param passCount パス数N（呼び出し側でマテリアルのmultiPassDitherCountを1〜8にクランプ
    ///        した値を渡すこと。同一呼び出し内のentriesは全て同じNを要求するグループであることが前提）
    /// @param baseSeedOffset 画面全体Nパスブレンド専用: この呼び出し全体（内部の全パス）へ
    ///        さらに加算する外側の位相オフセット（通常描画では0.0のまま渡す）
    /// @param baseSeedPassIndex 画面全体Nパスブレンド専用: 外側のパス番号（-1の場合は無視）。
    ///        内部のidSeedバッファキーへ外側・内側両方のパス番号を含めることで、外側・内側どちらの
    ///        Nパスループが動いていてもバッファの取り合いが起きないようにする
    /// @details 各パスはスクラッチの色・深度バッファへ独立して描画する（深度はオーナーの
    ///          既存深度＝不透明・通常ディザ描画済みの状態を毎パス複製して使う）ため、
    ///          パス内でのオブジェクト同士の深度順・idSeedによる分離は既存ロジックがそのまま働く。
    ///          各パスの結果を1/N重みで加算合成バッファへ蓄積し、最後にオーナーの実際の
    ///          カラーへ「over」合成する
    void RenderMultiPassDither(ScreenBuffer *screenBuffer,
        PipelineBinder &pipelineBinder,
        std::span<const SceneRenderer::DrawEntry> entries,
        SceneRenderer *sceneRenderer,
        CameraLightsBindCache &lightsCache,
        std::uint32_t passCount,
        float baseSeedOffset = 0.0f,
        int baseSeedPassIndex = -1);

    /// @brief GPU Simulation有効なParticleSystem2D/3Dの専用描画パス
    /// @details ProcessGpuParticlesが書き込んだgInstanceMatricesをSRVとして
    ///          そのままgTransformationMatricesにバインドし、既存のObject2D/Object3D
    ///          頂点シェーダーを無改造で利用する。常にプール容量分描画し、死亡中の
    ///          パーティクルはスケール0の行列で非表示になる（コンパクションは行わない）。
    ///          RenderToTargetの通常バッチ描画の後に呼ぶ
    void RenderGpuParticles(IRenderTarget *target,
        PipelineBinder &pipelineBinder,
        SceneRenderer *sceneRenderer,
        CameraLightsBindCache &lightsCache);

    /// @brief 指定描画先・パイプラインに対するカメラ・ライトの定数バッファバインド
    /// @details lightsCacheが直前と同じ(pipelineName, pipelineBinder.Generation())を指している場合、
    ///          ルート引数はまだ有効なはずなので実際のバインド処理をスキップする
    void BindCameraAndLights(ID3D12GraphicsCommandList *commandList,
        IRenderTarget *target,
        const std::string &pipelineName,
        SceneRenderer *sceneRenderer,
        PipelineBinder &pipelineBinder,
        CameraLightsBindCache &lightsCache);

    /// @brief ポイント/スポットライトの構造化バッファ・個数定数・シャドウマップのバインド
    void BindLightBuffersAndShadowMap(IRenderTarget *target,
        const std::string &pipelineName,
        SceneRenderer *sceneRenderer,
        ShaderVariableBinder &shaderBinder);

    /// @brief ScreenBuffer へのポストプロセス適用
    /// @param ownerObject ScreenBuffer を所有するオブジェクト（ポストエフェクトコンポーネントの取得元）
    /// @param sceneRenderer このスクリーンバッファへ描画したカメラの情報を解決するために使う（AO等が使用）
    void RenderPostProcess(ScreenBuffer *screenBuffer,
        PipelineBinder &pipelineBinder,
        EmptyObject *ownerObject,
        SceneRenderer *sceneRenderer);

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
    /// @brief ブルーノイズによるディザ閾値テーブルの生成・保持（構築時に1回だけ生成する）
    BlueNoiseGenerator blueNoiseGenerator_;

    //==================================================
    // Nパス蓄積による半透明ディザのブレンド（RenderMultiPassDither参照）
    //==================================================

    /// @brief RenderMultiPassDitherのスクラッチ/蓄積GPUリソースを描画先（ScreenBuffer）ごとに保持する。
    /// @details Renderer全体で1組だけ共有すると、サイズの異なる複数ScreenBufferを同一フレーム内で
    ///          処理した際に、片方の描画先向けの再作成がもう片方の未実行コマンドから参照中の
    ///          リソースを破棄してしまう（詳細はRendererInternal::MultiPassDitherScratchSet参照）。
    ///          描画先が破棄された後のエントリはRenderFrameの冒頭でGCする
    std::unordered_map<ScreenBuffer *, RendererInternal::MultiPassDitherScratchSet> multiPassDitherScratch_;

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
    /// @brief 今フレームのシャドウマップ解像度（RenderShadowMapsPhaseIntoのビューポート計算に使う）
    std::uint32_t shadowResolutionForRedraw_ = 0;
    /// @brief 今フレームの準備済みシャドウ描画バッチ（RenderShadowMaps参照。位相再生成用に保持する）
    std::vector<PreparedShadowBatch> shadowBatches_;
    /// @brief 今フレームの準備済みGPUパーティクル影キャスター（RenderShadowMaps参照）
    std::vector<PreparedGpuParticleShadowBatch> shadowGpuParticleBatches_;

    //==================================================
    // 画面全体Nパスブレンド（RenderScreenWideDitherTarget/ScreenWideDitherBlendEffect参照）
    //==================================================

    /// @brief 1パス分のシーン全体を書き込むスクラッチカラー（毎パスクリアして使い回す）
    std::unique_ptr<RenderTargetResource> screenWideScratchColor_;
    std::unique_ptr<ShaderResourceResource> screenWideScratchColorSrv_;
    /// @brief Nパス分を1/N重みで加算合成した蓄積カラー（ループ開始前にクリア）
    std::unique_ptr<RenderTargetResource> screenWideAccumColor_;
    std::unique_ptr<ShaderResourceResource> screenWideAccumColorSrv_;
    /// @brief 毎パスのシーン全体描画で使う作業用深度（毎パスクリアして使い回す）
    std::unique_ptr<DepthStencilResource> screenWideScratchDepth_;

    /// @brief 直近のRenderFrameで発行されたDrawIndexedInstanced呼び出し回数（RenderFrame冒頭でリセットする）
    std::uint32_t drawCallCount_ = 0;
};

} // namespace KashipanEngine
