#include "RendererInternal.h"
#include "Objects/Components/PostProcessing/ScreenWideDitherBlendEffect.h"

namespace KashipanEngine {

using namespace RendererInternal;

namespace {
constexpr std::uint32_t kDefaultMultiPassDitherCount = 4;
constexpr std::uint32_t kMinMultiPassDitherCount = 1;
constexpr std::uint32_t kMaxMultiPassDitherCount = 8;
} // namespace

void Renderer::RenderToTarget(IRenderTarget *target,
    std::span<const SceneRenderer::DrawEntry> entries,
    SceneRenderer *sceneRenderer) {
    if (!target) return;

    target->BeginDraw();
    auto *commandList = target->GetCommandList();
    if (!commandList) return;

    PipelineBinder pipelineBinder(commandList, pipelineManager_);
    CameraLightsBindCache lightsCache;

    RenderSceneContent(target, entries, sceneRenderer, pipelineBinder, lightsCache);

    // ScreenBuffer の場合は所有オブジェクトのポストプロセスを適用
    if (target->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer) {
        auto *screenBuffer = static_cast<ScreenBuffer *>(target);
        // エディター用描画先の場合、デバッグ表示（グリッド・当たり判定）をポストプロセスより先に描画する
        RenderEditorDebugOverlay(screenBuffer, pipelineBinder, sceneRenderer);
        RenderPostProcess(screenBuffer, pipelineBinder, sceneRenderer->GetTargetOwner(target), sceneRenderer);
    }

    // ウィンドウのコマンドリストはこの後 ImGui 等の描画にも使われるため、
    // 描画終了処理はスワップチェーン側（DirectXCommon::EndDraw）に任せる
    // （ScreenBufferの場合、ビューア用プレビューバッファの更新はEndDraw内部で自動的に行われる。
    // 詳細はScreenBuffer::GetPreviewSrvHandle参照）
    if (target->GetRenderTargetKind() != RenderTargetKind::Window) {
        target->EndDraw();
    }
}

void Renderer::RenderSceneContent(IRenderTarget *target,
    std::span<const SceneRenderer::DrawEntry> entries,
    SceneRenderer *sceneRenderer,
    PipelineBinder &pipelineBinder,
    CameraLightsBindCache &lightsCache,
    float extraSeedOffset,
    int seedPassIndex,
    bool disableNestedMultiPassDither) {
    if (!target) return;

    // エディター用描画先の場合、他の描画より先に背景（単色 or テクスチャ）を描画する
    if (target->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer) {
        RenderEditorBackground(static_cast<ScreenBuffer *>(target), pipelineBinder, sceneRenderer);
    }

    // マテリアルのenableMultiPassDitherが有効なエントリを分離する（RenderMultiPassDither参照）。
    // ScreenBuffer以外の描画先ではスクラッチ管理を持たないため、通常描画のまま扱う。
    // マテリアルのmultiPassDitherCountでパス数Nを指定できるため、Nが異なるエントリ同士は
    // 別グループへ分ける（同一グループ内では元の並び順を保つ）。画面全体Nパスブレンド中
    // （disableNestedMultiPassDither）は、ネストしたNパス化をせず通常描画扱いにする
    // （RenderMultiPassDitherは描画先の実際の書き込み面へ直接合成する設計のため、
    // 画面全体側のスクラッチ書き込み先と両立できないことによる意図的な制限。ヘッダー参照）
    std::vector<SceneRenderer::DrawEntry> normalEntries;
    std::map<std::uint32_t, std::vector<SceneRenderer::DrawEntry>> multiPassEntriesByCount;
    normalEntries.reserve(entries.size());
    const bool supportsMultiPassDither = !disableNestedMultiPassDither &&
        target->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer;
    for (const auto &entry : entries) {
        bool wantsMultiPass = false;
        std::uint32_t passCount = kDefaultMultiPassDitherCount;
        if (supportsMultiPassDither) {
            if (auto *material = MaterialManager::GetMaterial(entry.materialHandle)) {
                if (auto found = material->extraParameters.find("enableMultiPassDither"); found != material->extraParameters.end()) {
                    if (const bool *value = found->second.AnyCastPtr<bool>()) wantsMultiPass = *value;
                }
                if (auto found = material->extraParameters.find("multiPassDitherCount"); found != material->extraParameters.end()) {
                    if (const std::int32_t *value = found->second.AnyCastPtr<std::int32_t>(); value && *value > 0) {
                        passCount = std::clamp(static_cast<std::uint32_t>(*value), kMinMultiPassDitherCount, kMaxMultiPassDitherCount);
                    }
                }
            }
        }
        if (wantsMultiPass) {
            multiPassEntriesByCount[passCount].push_back(entry);
        } else {
            normalEntries.push_back(entry);
        }
    }

    // 同一（パイプライン・メッシュ・サブメッシュ・マテリアル）の連続範囲をバッチとしてまとめて描画
    // （ただしallowInstancing==falseのエントリは他と結合せず必ず単独のドローコールにする）。
    // テクスチャ・サンプラーはバインドレス化（gTextures[]/gSamplers[]、DrawBatch参照）により
    // インスタンスごとに異なっていてもよいため、バッチ結合条件には含めない
    size_t begin = 0;
    while (begin < normalEntries.size()) {
        const auto &first = normalEntries[begin];
        size_t end = begin + 1;
        if (first.allowInstancing) {
            while (end < normalEntries.size()) {
                const auto &other = normalEntries[end];
                if (!other.allowInstancing ||
                    other.pipelineName != first.pipelineName ||
                    other.meshHandle != first.meshHandle ||
                    other.materialHandle != first.materialHandle ||
                    other.indexStart != first.indexStart ||
                    other.indexCount != first.indexCount ||
                    other.skinnedVertexBuffer != first.skinnedVertexBuffer) {
                    break;
                }
                ++end;
            }
        }

        DrawBatch(target, pipelineBinder, std::span<const SceneRenderer::DrawEntry>(normalEntries).subspan(begin, end - begin),
            sceneRenderer, lightsCache, extraSeedOffset, seedPassIndex);
        begin = end;
    }

    // Nパス蓄積対象（半透明ディザを複数回描画してブレンドする）を、要求パス数のグループごとに描画する
    for (auto &[passCount, group] : multiPassEntriesByCount) {
        if (group.empty()) continue;
        RenderMultiPassDither(static_cast<ScreenBuffer *>(target), pipelineBinder, group, sceneRenderer, lightsCache,
            passCount, extraSeedOffset, seedPassIndex);
    }

    // TextRenderer（文字単位のDrawEntryとしてSceneRenderer::BuildSortedDrawListが生成する）は
    // 上のnormalEntriesバッチループで他のRendererと同じ扱いで既に描画済み（DrawBatch参照）

    // GPU Simulation有効なParticleSystem2D/3D（ProcessGpuParticlesが結果を書き込み済み）も専用パスで描画する
    RenderGpuParticles(target, pipelineBinder, sceneRenderer, lightsCache);
}

void Renderer::DrawBatch(IRenderTarget *target,
    PipelineBinder &pipelineBinder,
    std::span<const SceneRenderer::DrawEntry> batch,
    SceneRenderer *sceneRenderer,
    CameraLightsBindCache &lightsCache,
    float extraSeedOffset,
    int seedPassIndex) {
    if (batch.empty()) return;

    const auto &first = batch.front();
    const std::string &pipelineName = first.pipelineName;
    auto *commandList = target->GetCommandList();

    const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(first.meshHandle);
    if (!meshBuffers || !meshBuffers->indexBuffer) return;
    // SkinnedMeshRendererから作られたエントリの場合は静的な頂点バッファではなく
    // GPUスキニング結果（インスタンス専用）を頂点バッファとして使用する
    if (!first.skinnedVertexBuffer && !meshBuffers->vertexBuffer) return;

    pipelineBinder.UsePipeline(pipelineName);
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
    shaderBinder.SetCommandList(commandList);

    // カメラ・ライトの定数バッファバインド（直前と同じパイプラインのままなら内部でスキップされる）
    BindCameraAndLights(commandList, target, pipelineName, sceneRenderer, pipelineBinder, lightsCache);

    const std::uint32_t instanceCount = static_cast<std::uint32_t>(batch.size());

    // ワールド行列のインスタンスバッファ
    {
        // サブメッシュ（同一メッシュ・同一マテリアルでもインデックス範囲が異なる）ごとに
        // 別バッファを使うよう、キーにインデックス範囲を含める
        char transformSuffix[48];
        std::snprintf(transformSuffix, sizeof(transformSuffix), "transform|%u", first.indexStart);
        auto key = MakeBatchKey(target, pipelineName, first.meshHandle, first.materialHandle, transformSuffix);
        if (first.skinnedVertexBuffer) {
            // SkinnedMeshRendererのエントリはインスタンス結合されず必ずinstanceCount=1で
            // 個別にDrawBatchが呼ばれるが、同じメッシュ/マテリアル/パイプライン/描画先を
            // 参照する別インスタンスがあると上記キーが完全に一致してしまい、
            // resourceContainer_にキャッシュされた同一GPUバッファを取り合って上書きし合う
            // （結果、全インスタンスが最後に書き込まれた1つのワールド行列を参照して
            // 同じ位置に描画されてしまう）。インスタンス固有のスキニング出力バッファの
            // ポインタをキーに含めることで、インスタンスごとに専用の変換行列バッファを使う。
            char suffix[32];
            std::snprintf(suffix, sizeof(suffix), "|%p", static_cast<void *>(first.skinnedVertexBuffer));
            key += suffix;
        }
        // 動かない静的オブジェクトのみのバッチでは前フレームと内容が完全に一致するため、
        // GetOrUpdateStructuredBufferが内容比較によりMap+memcpyを省略する
        std::vector<Matrix4x4> transforms(instanceCount);
        for (size_t i = 0; i < batch.size(); ++i) {
            transforms[i] = batch[i].worldMatrix;
        }
        auto *instanceBuffer = resourceContainer_->GetOrUpdateStructuredBuffer(key, sizeof(Matrix4x4), instanceCount, transforms.data());
        if (!instanceBuffer) return;
        shaderBinder.Bind("Vertex:gTransformationMatrices", instanceBuffer);
    }

    // オブジェクト固有シード値のインスタンスバッファ（ディザリングの位相分離等に使用。
    // ObjectPS.hlslのディザ抜き半透明を持たないパイプラインでは対応するバインディングが
    // 存在せず、shaderBinder.Bind内部で何もせずfalseを返すだけなので無害）
    {
        char idSeedSuffix[48];
        std::snprintf(idSeedSuffix, sizeof(idSeedSuffix), "idSeed|%u", first.indexStart);
        auto key = MakeBatchKey(target, pipelineName, first.meshHandle, first.materialHandle, idSeedSuffix);
        if (first.skinnedVertexBuffer) {
            char suffix[32];
            std::snprintf(suffix, sizeof(suffix), "|%p", static_cast<void *>(first.skinnedVertexBuffer));
            key += suffix;
        }
        if (seedPassIndex >= 0) {
            // RenderMultiPassDither専用: 同一フレーム内で位相だけ異なる複数パスが同じ
            // （ターゲット・パイプライン・メッシュ・マテリアル）キーを共有してしまうと、
            // CPU側の書き込み（Map+memcpy）がGPU実行前に競合し、後のパスの内容で
            // 前のパスの描画結果まで上書きされてしまうため、パスごとに別バッファへ分離する
            char passSuffix[24];
            std::snprintf(passSuffix, sizeof(passSuffix), "|mp%d", seedPassIndex);
            key += passSuffix;
        }
        std::vector<float> idSeeds(instanceCount);
        for (size_t i = 0; i < batch.size(); ++i) {
            idSeeds[i] = batch[i].objectIdSeed + extraSeedOffset;
        }
        auto *idSeedBuffer = resourceContainer_->GetOrUpdateStructuredBuffer(key, sizeof(float), instanceCount, idSeeds.data());
        if (idSeedBuffer) {
            shaderBinder.Bind("Vertex:gObjectIdSeeds", idSeedBuffer);
        }
    }

    // マテリアルの構造化バッファ（シェーダーはインスタンスIDで参照するため個数分並べる）
    {
        auto *material = MaterialManager::GetMaterial(first.materialHandle);
        if (material) {
            // 読み込み時に未解決だったテクスチャハンドルの解決を試みる
            material->ResolveTextureHandles();
        }

        char materialSuffix[48];
        std::snprintf(materialSuffix, sizeof(materialSuffix), "material|%u", first.indexStart);
        auto key = MakeBatchKey(target, pipelineName, first.meshHandle, first.materialHandle, materialSuffix);
        if (first.skinnedVertexBuffer) {
            // 上記の変換行列バッファと同じ理由で、スキニングインスタンスごとに専用バッファを使う
            char suffix[32];
            std::snprintf(suffix, sizeof(suffix), "|%p", static_cast<void *>(first.skinnedVertexBuffer));
            key += suffix;
        }
        // マテリアルの固定フィールドは、パイプラインのPixelシェーダーが定義する struct Material の
        // バイトレイアウト（PipelineInfo::GetMaterialLayout）に従って汎用的にパックする。これにより
        // Object3D/Object2D/Velocity等、異なるMaterial定義を持つシェーダーを同一ロジックで扱える
        const auto &pipelineInfo = pipelineManager_->GetPipeline(pipelineName);
        auto elementTemplate = BuildMaterialElementBytes(pipelineInfo, material);
        const std::uint32_t stride = pipelineInfo.GetMaterialLayout().totalByteSize;
        if (stride > 0) {
            std::vector<std::byte> allBytes(static_cast<size_t>(stride) * instanceCount);
            for (std::uint32_t i = 0; i < instanceCount; ++i) {
                std::byte *elementBytes = allBytes.data() + static_cast<size_t>(i) * stride;
                std::memcpy(elementBytes, elementTemplate.data(), stride);
                // instanceColor/instanceColorBlendModeはインスタンス（MeshRenderer）ごとに異なり得るため、
                // マテリアル本体の値のみ共有した上で、インスタンスごとに個別の値を書き込む
                WriteMaterialField(pipelineInfo, elementBytes, stride, "instanceColor", batch[i].instanceColor);
                float blendMode = static_cast<float>(batch[i].instanceColorBlendMode);
                WriteMaterialField(pipelineInfo, elementBytes, stride, "instanceColorBlendMode", blendMode);
                // 任意マテリアル対応以前のTextシェーダーは文字色をcharacterColorとして受け取る。
                // 既存プロジェクトに古いKashipanEngineアセットのコピーが残っていても全文字が
                // alpha=0にならないよう、レイアウトに存在する場合だけ同じ色を書き込む。
                WriteMaterialField(pipelineInfo, elementBytes, stride, "characterColor", batch[i].instanceColor);
                // uvRect/boldWeight/アウトライン値はTextRendererのみが使うフィールド（Text2D/Text3D
                // パイプラインのMaterial構造体にのみ存在する）。それ以外のパイプラインではGetMaterialLayout
                // にヒットせずWriteMaterialFieldが黙ってスキップするため、他のRendererには影響しない
                WriteMaterialField(pipelineInfo, elementBytes, stride, "uvRect", batch[i].uvRect);
                WriteMaterialField(pipelineInfo, elementBytes, stride, "boldWeight", batch[i].boldWeight);
                WriteMaterialField(pipelineInfo, elementBytes, stride, "outlineWidth", batch[i].textOutlineWidth);
                WriteMaterialField(pipelineInfo, elementBytes, stride, "outlineColor", batch[i].textOutlineColor);
                // テクスチャ/サンプラーはバッチ結合条件から外れている（textureOverrideHandle/
                // samplerOverrideHandleが異なる複数インスタンスが同一バッチに混在し得る）ため、
                // インスタンスごとにバインドレス配列内のインデックスとして書き込む（DrawBatch冒頭コメント参照）
                WriteMaterialField(pipelineInfo, elementBytes, stride, "textureIndex",
                    ResolveInstanceTextureIndex(batch[i].textureOverrideHandle, material));
                WriteMaterialField(pipelineInfo, elementBytes, stride, "samplerIndex",
                    ResolveInstanceSamplerIndex(batch[i].samplerOverrideHandle, material));
            }
            auto *materialBuffer = resourceContainer_->GetOrUpdateStructuredBuffer(key, stride, instanceCount, allBytes.data());
            if (materialBuffer) {
                shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
                // 押し出しアウトライン等、頂点シェーダー側でもマテリアル値を必要とするカスタムシェーダー向け。
                // 該当バインディングを持たないパイプラインでは何もせず false を返すだけなので無害
                shaderBinder.Bind("Vertex:gMaterials", materialBuffer);
            }
        }

        // バインドレステクスチャ配列（Texture2D gTextures[]）のテーブルをバインドする。
        // 静的サイズではなくヒープ先頭の予約レンジ全体を指すため、パイプライン・バッチによらず
        // 常に同じハンドルであり、該当バインディングを持たないパイプラインでは何もせずfalseを返すだけなので無害
        shaderBinder.Bind("Pixel:gTextures", directXCommon_->GetTextureBindlessBaseHandleForRenderer(Passkey<Renderer>{}));
        // 既定6種のサンプラー配列（gSamplers[6]）も同様に、SamplerHeap予約レンジを指す固定ハンドルを
        // 1回だけバインドする（静的サンプラーでは実行時インデックスに対応できないため通常のテーブル扱い）
        shaderBinder.Bind("Pixel:gSamplers", directXCommon_->GetSamplerBindlessBaseHandleForRenderer(Passkey<Renderer>{}));
        // モジュール（NormalMap等）が使う専用テクスチャスロットは、gTexture/gSamplerとは別の
        // 固定register番号を個別に持つため、引き続き毎ドローコールでの動的バインドが必要
        BindExtraTextureParameters(&shaderBinder, material);
    }

    // メッシュのバインドと描画
    if (first.skinnedVertexBuffer) {
        first.skinnedVertexBuffer->SetCommandList(commandList);
        D3D12_VERTEX_BUFFER_VIEW skinnedView = first.skinnedVertexBuffer->GetView(sizeof(ResourceContainer::MeshVertex));
        pipelineBinder.SetVertexBufferView(0, 1, &skinnedView);
    } else {
        pipelineBinder.SetVertexBuffer(meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
    }
    pipelineBinder.SetIndexBuffer(meshBuffers->indexBuffer.get());
    // サブメッシュ範囲が指定されている場合はその範囲のみ描画する（indexCount==0はメッシュ全体）
    const std::uint32_t drawIndexCount = first.indexCount > 0 ? first.indexCount : meshBuffers->indexCount;
    commandList->DrawIndexedInstanced(drawIndexCount, instanceCount, first.indexStart, 0, 0);
    ++drawCallCount_;
}

namespace {
/// @brief 黄金比の共役。各パスのidSeedオフセットに使い、少ないパス数でも低差異に分散させる
constexpr float kMultiPassDitherGoldenRatioConjugate = 0.6180339887f;

/// @brief PostEffect.MultiPassDitherAccumulateCB.hlsliと同レイアウトの構造体
struct MultiPassDitherAccumulateConstants {
    float invPassCount = 1.0f;
    float padding[3]{};
};
} // namespace

void Renderer::RenderMultiPassDither(ScreenBuffer *screenBuffer,
    PipelineBinder &pipelineBinder,
    std::span<const SceneRenderer::DrawEntry> entries,
    SceneRenderer *sceneRenderer,
    CameraLightsBindCache &lightsCache,
    std::uint32_t passCount,
    float baseSeedOffset,
    int baseSeedPassIndex) {
    if (!screenBuffer || entries.empty() || passCount == 0) return;

    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    const std::uint32_t width = screenBuffer->GetRenderTargetWidth();
    const std::uint32_t height = screenBuffer->GetRenderTargetHeight();
    if (width == 0 || height == 0) return;

    // GetRenderTarget()/GetDepthStencil()はダブルバッファのうち「直前に描画が完了した読み取り用の面」
    // （ポストエフェクトの中間パスが直前のパス結果を参照する用途）を返してしまい、通常の3D描画が
    // 今まさに書き込んでいる面（不透明・通常ディザ描画済みの内容を含む）とは異なる。
    // ここでは書き込み中の面を直接参照する必要があるためGetWriteRenderTarget/GetWriteDepthStencilを使う
    auto *realColor = screenBuffer->GetWriteRenderTarget();
    auto *realDepth = screenBuffer->GetWriteDepthStencil();
    if (!realColor || !realDepth) return;

    static const char *kAccumulatePipeline = "PostEffect.MultiPassDitherAccumulate";
    static const char *kCompositePipeline = "PostEffect.MultiPassDitherComposite";
    if (!pipelineManager_->HasPipeline(kAccumulatePipeline) || !pipelineManager_->HasPipeline(kCompositePipeline)) return;

    // スクラッチ・蓄積用リソースは描画先（screenBuffer）ごとに個別保持する（詳細は
    // RendererInternal::MultiPassDitherScratchSetのコメント参照。サイズの異なる複数の
    // ScreenBufferをRenderer全体で1組のリソースを使い回すと、同一フレーム内で他の
    // 描画先が積んだ未実行コマンドから参照中のリソースを破棄してしまうため）
    auto &scratch = multiPassDitherScratch_[screenBuffer];

    // スクラッチ・蓄積用リソースをオーナーのサイズに合わせて用意する（深度フォーマットは
    // ScreenBufferの既定値であるD24_UNORM_S8_UINTを前提とする）
    if (!scratch.scratchColor || scratch.scratchColor->GetWidth() != width || scratch.scratchColor->GetHeight() != height) {
        const FLOAT transparentClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        scratch.scratchColor = std::make_unique<RenderTargetResource>(width, height, realColor->GetFormat(), nullptr, transparentClear);
        scratch.accumColor = std::make_unique<RenderTargetResource>(width, height, realColor->GetFormat(), nullptr, transparentClear);
        scratch.scratchColorSrv = std::make_unique<ShaderResourceResource>(scratch.scratchColor.get());
        scratch.accumColorSrv = std::make_unique<ShaderResourceResource>(scratch.accumColor.get());
        scratch.depthSnapshot = std::make_unique<DepthStencilResource>(width, height, DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, static_cast<UINT8>(0));
        scratch.scratchDepth = std::make_unique<DepthStencilResource>(width, height, DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, static_cast<UINT8>(0));
        // DepthStencilResourceはコンストラクタでDEPTH_WRITE/DEPTH_READ（createSrv時はSRV状態）しか
        // 遷移先として登録しない。COPY_SOURCE/COPY_DESTを登録しないままTransitionToで遷移すると、
        // バリア自体は発行されるが内部の追跡インデックスが同期されず（IGraphicsResource::TransitionTo参照）、
        // 後続の「DEPTH_WRITEへ戻す」呼び出しが「既にDEPTH_WRITEのはず」と誤認してバリアをスキップし、
        // 実際にはCOPY_DEST/COPY_SOURCEのまま深度バッファとして使われてしまう
        // （描画結果が完全に消える不具合の原因だった）。ここで明示的に登録しておく
        scratch.depthSnapshot->AddTransitionState(D3D12_RESOURCE_STATE_COPY_DEST);
        scratch.depthSnapshot->AddTransitionState(D3D12_RESOURCE_STATE_COPY_SOURCE);
        scratch.scratchDepth->AddTransitionState(D3D12_RESOURCE_STATE_COPY_DEST);
    }
    // realDepthはScreenBuffer側が所有するダブルバッファのため、ここで取得したインスタンスに
    // COPY_SOURCEが登録済みとは限らない（フレームごとに読み取り面が入れ替わるため）。
    // 同じ理由で毎回明示的に登録する（重複登録は無害）
    realDepth->AddTransitionState(D3D12_RESOURCE_STATE_COPY_SOURCE);
    if (!scratch.scratchColor || !scratch.accumColor || !scratch.scratchColorSrv ||
        !scratch.accumColorSrv || !scratch.depthSnapshot || !scratch.scratchDepth) {
        return;
    }

    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    D3D12_RECT scissor{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

    // オーナーの現在の深度（不透明・通常ディザ描画済み）をスナップショットへコピーする
    realDepth->SetCommandList(commandList);
    scratch.depthSnapshot->SetCommandList(commandList);
    realDepth->TransitionTo(D3D12_RESOURCE_STATE_COPY_SOURCE);
    scratch.depthSnapshot->TransitionTo(D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->CopyResource(scratch.depthSnapshot->GetResource(), realDepth->GetResource());

    // 蓄積バッファを透明クリアする
    scratch.accumColor->SetCommandList(commandList);
    scratch.accumColor->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
    scratch.accumColor->ClearRenderTargetView();

    for (std::uint32_t pass = 0; pass < passCount; ++pass) {
        // 深度をスナップショット（不透明・通常ディザ描画済み）へ巻き戻す
        scratch.scratchDepth->SetCommandList(commandList);
        scratch.depthSnapshot->SetCommandList(commandList);
        scratch.scratchDepth->TransitionTo(D3D12_RESOURCE_STATE_COPY_DEST);
        scratch.depthSnapshot->TransitionTo(D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandList->CopyResource(scratch.scratchDepth->GetResource(), scratch.depthSnapshot->GetResource());
        scratch.scratchDepth->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);

        // スクラッチカラーを透明クリアする
        scratch.scratchColor->SetCommandList(commandList);
        scratch.scratchColor->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
        scratch.scratchColor->ClearRenderTargetView();

        {
            const auto rtv = scratch.scratchColor->GetCPUDescriptorHandle();
            const auto dsv = scratch.scratchDepth->GetCPUDescriptorHandle();
            commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissor);
        }

        // このパス専用の位相オフセットを各エントリのobjectIdSeedへ加算した状態でバッチ描画する。
        // パス内ではオブジェクト同士の深度順・idSeedによる分離は既存のDrawBatchロジックがそのまま働く。
        // 画面全体Nパスブレンド（baseSeedOffset/baseSeedPassIndex）と組み合わさる場合は、
        // 外側の位相・パス番号もさらに加算・合成する
        const float seedOffset = baseSeedOffset + static_cast<float>(pass) * kMultiPassDitherGoldenRatioConjugate;
        const int combinedPassIndex = (baseSeedPassIndex >= 0) ? (baseSeedPassIndex * 1000 + static_cast<int>(pass)) : static_cast<int>(pass);
        size_t begin = 0;
        while (begin < entries.size()) {
            const auto &first = entries[begin];
            size_t end = begin + 1;
            if (first.allowInstancing) {
                while (end < entries.size()) {
                    const auto &other = entries[end];
                    if (!other.allowInstancing ||
                        other.pipelineName != first.pipelineName ||
                        other.meshHandle != first.meshHandle ||
                        other.materialHandle != first.materialHandle ||
                        other.textureOverrideHandle != first.textureOverrideHandle ||
                        other.samplerOverrideHandle != first.samplerOverrideHandle ||
                        other.indexStart != first.indexStart ||
                        other.indexCount != first.indexCount ||
                        other.skinnedVertexBuffer != first.skinnedVertexBuffer) {
                        break;
                    }
                    ++end;
                }
            }
            DrawBatch(screenBuffer, pipelineBinder, entries.subspan(begin, end - begin), sceneRenderer, lightsCache,
                seedOffset, combinedPassIndex);
            begin = end;
        }

        // このパスの結果を1/N重みで蓄積バッファへ加算合成する
        scratch.scratchColor->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        scratch.accumColor->SetCommandList(commandList);
        scratch.accumColor->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
        {
            const auto accumRtv = scratch.accumColor->GetCPUDescriptorHandle();
            commandList->OMSetRenderTargets(1, &accumRtv, FALSE, nullptr);
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissor);

            pipelineBinder.UsePipeline(kAccumulatePipeline);
            auto &binder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, kAccumulatePipeline);
            binder.SetCommandList(commandList);

            // passCountごとに別バッファへ分離する（1回のRenderToTargetでmultiPassDitherCountが
            // 異なる複数グループを描画する場合、同一バッファを使い回すとCPU側の書き込みが
            // GPU実行前に競合し、全グループが最後に書き込まれたinvPassCountを参照してしまうため）
            MultiPassDitherAccumulateConstants cbData{};
            cbData.invPassCount = 1.0f / static_cast<float>(passCount);
            char cbKey[48];
            std::snprintf(cbKey, sizeof(cbKey), "MultiPassDitherAccumulateCB|%u", passCount);
            auto *accumulateConstantBuffer = resourceContainer_->GetOrCreateConstantBuffer(cbKey, sizeof(MultiPassDitherAccumulateConstants));
            if (accumulateConstantBuffer) {
                if (auto *mapped = accumulateConstantBuffer->Map()) {
                    std::memcpy(mapped, &cbData, sizeof(cbData));
                }
            }
            binder.Bind("Pixel:MultiPassDitherAccumulateCB", accumulateConstantBuffer);
            binder.Bind("Pixel:gSceneTexture", scratch.scratchColorSrv->GetGPUDescriptorHandle());
            SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
            commandList->DrawInstanced(3, 1, 0, 0);
            ++drawCallCount_;
        }
        scratch.accumColor->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    // 蓄積結果をオーナーの実際のカラーへ「over」合成する
    realDepth->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);
    realColor->SetCommandList(commandList);
    realColor->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
    {
        const auto rtv = realColor->GetCPUDescriptorHandle();
        commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);

        pipelineBinder.UsePipeline(kCompositePipeline);
        auto &binder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, kCompositePipeline);
        binder.SetCommandList(commandList);
        binder.Bind("Pixel:gAccumTexture", scratch.accumColorSrv->GetGPUDescriptorHandle());
        SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
        commandList->DrawInstanced(3, 1, 0, 0);
        ++drawCallCount_;
    }

    // オーナーの色・深度を通常の描画状態へ戻す（この後RenderGpuParticles/
    // RenderPostProcessが続けて描画するため、RTV/DSVを正しく再設定しておく必要がある）
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    {
        const auto rtv = realColor->GetCPUDescriptorHandle();
        const auto dsv = realDepth->GetCPUDescriptorHandle();
        commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    }

    // この関数内で任意のパイプラインへ切り替えたため、カメラ・ライトの再バインドキャッシュを無効化する
    lightsCache.valid = false;
}

std::vector<Renderer::ScreenWideDitherRequest> Renderer::CollectScreenWideDitherTargets(
    SceneRenderer *sceneRenderer, const std::vector<IRenderTarget *> &targets) {
    std::vector<ScreenWideDitherRequest> result;
    if (!sceneRenderer) return result;
    for (auto *target : targets) {
        if (!target || target->GetRenderTargetKind() != RenderTargetKind::ScreenBuffer) continue;
        auto *owner = sceneRenderer->GetTargetOwner(target);
        if (!owner) continue;
        for (auto *component : sceneRenderer->GetPostProcessComponentsFor(owner)) {
            auto *effect = dynamic_cast<ScreenWideDitherBlendEffect *>(component);
            if (!effect || !effect->IsActive()) continue;
            auto *screenBuffer = static_cast<ScreenBuffer *>(target);
            if (!effect->IsScreenBufferIncluded(screenBuffer)) continue;
            result.push_back(ScreenWideDitherRequest{ screenBuffer, effect->GetPassCount() });
            break; // 同一描画先に複数付いていても最初の1つだけを使う
        }
    }
    return result;
}

void Renderer::RenderScreenWideDitherTarget(ScreenBuffer *screenBuffer,
    std::span<const SceneRenderer::DrawEntry> entries,
    SceneRenderer *sceneRenderer,
    std::uint32_t passCount) {
    if (!screenBuffer) return;
    passCount = std::clamp(passCount, 1u, 8u);

    screenBuffer->BeginDraw();
    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    PipelineBinder pipelineBinder(commandList, pipelineManager_);
    CameraLightsBindCache lightsCache;

    const std::uint32_t width = screenBuffer->GetRenderTargetWidth();
    const std::uint32_t height = screenBuffer->GetRenderTargetHeight();
    auto *realColor = screenBuffer->GetWriteRenderTarget();
    auto *realDepth = screenBuffer->GetWriteDepthStencil();

    static const char *kAccumulatePipeline = "PostEffect.MultiPassDitherAccumulate";
    // 蓄積結果の最終合成は単純な上書きコピーでよい（このパスは半透明マスクではなく画面全体を
    // 毎回フルに描いて平均するだけのため、TemporalBlendEffect用の等倍コピーパイプラインを流用する）
    static const char *kCopyPipeline = "PostEffect.TemporalBlend.Copy";
    const bool resourcesReady = passCount > 1 && width > 0 && height > 0 && realColor && realDepth &&
        pipelineManager_ && pipelineManager_->HasPipeline(kAccumulatePipeline) && pipelineManager_->HasPipeline(kCopyPipeline);

    if (resourcesReady && (!screenWideScratchColor_ || screenWideScratchColor_->GetWidth() != width || screenWideScratchColor_->GetHeight() != height)) {
        const FLOAT transparentClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        screenWideScratchColor_ = std::make_unique<RenderTargetResource>(width, height, realColor->GetFormat(), nullptr, transparentClear);
        screenWideAccumColor_ = std::make_unique<RenderTargetResource>(width, height, realColor->GetFormat(), nullptr, transparentClear);
        screenWideScratchColorSrv_ = std::make_unique<ShaderResourceResource>(screenWideScratchColor_.get());
        screenWideAccumColorSrv_ = std::make_unique<ShaderResourceResource>(screenWideAccumColor_.get());
        screenWideScratchDepth_ = std::make_unique<DepthStencilResource>(width, height, DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, static_cast<UINT8>(0));
        // DepthStencilResourceはコンストラクタでDEPTH_WRITE/DEPTH_READしか遷移先として登録しない。
        // 最終合成でrealDepthへCopyResourceする際にCOPY_SOURCEへ遷移するため、ここで明示的に
        // 登録しておく必要がある（登録が無いとTransitionTo自体は成功するが内部の追跡インデックスが
        // 同期されず、次フレームのDEPTH_WRITEへの遷移がバリア無しでスキップされてしまう。
        // RenderMultiPassDitherで一度踏んだ不具合と同じ原因。IGraphicsResource::TransitionTo参照）
        screenWideScratchDepth_->AddTransitionState(D3D12_RESOURCE_STATE_COPY_SOURCE);
    }
    const bool canBlend = resourcesReady && screenWideScratchColor_ && screenWideAccumColor_ &&
        screenWideScratchColorSrv_ && screenWideAccumColorSrv_ && screenWideScratchDepth_;

    if (!canBlend) {
        // N=1、パイプライン未ロード、リソース作成失敗等の場合は通常の1パス描画にフォールバックする
        // （影も撮り直さず、既存のRenderShadowMapsが構築した内容をそのまま使う）
        RenderSceneContent(screenBuffer, entries, sceneRenderer, pipelineBinder, lightsCache);
    } else {
        D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
        D3D12_RECT scissor{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

        screenWideAccumColor_->SetCommandList(commandList);
        screenWideAccumColor_->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
        screenWideAccumColor_->ClearRenderTargetView();

        for (std::uint32_t pass = 0; pass < passCount; ++pass) {
            const float phaseOffset = static_cast<float>(pass) * kMultiPassDitherGoldenRatioConjugate;

            // 影も含めてこのパスの位相でシャドウマップを撮り直す。shadowMapArray_は全描画先で
            // 共有されるため、この描画先の処理を終えるまで他の描画先はこのフレーム内でシャドウマップを
            // 参照しないこと（RenderFrameが画面全体Nパスブレンド対象を常に最後に処理することで保証する）
            RenderShadowMapsPhaseInto(screenBuffer, pipelineBinder, phaseOffset, static_cast<int>(pass));

            screenWideScratchDepth_->SetCommandList(commandList);
            screenWideScratchDepth_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);
            screenWideScratchDepth_->ClearDepthStencilView();
            screenWideScratchColor_->SetCommandList(commandList);
            screenWideScratchColor_->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
            screenWideScratchColor_->ClearRenderTargetView();

            {
                const auto rtv = screenWideScratchColor_->GetCPUDescriptorHandle();
                const auto dsv = screenWideScratchDepth_->GetCPUDescriptorHandle();
                commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
                commandList->RSSetViewports(1, &viewport);
                commandList->RSSetScissorRects(1, &scissor);
            }

            // シーン内容（背景・通常バッチ・テキスト・GPUパーティクル）をこの位相で描画する。
            // ネストしたオブジェクト単位Nパスディザ（enableMultiPassDither）は無効化する
            // （RenderSceneContentのdisableNestedMultiPassDither引数のヘッダーコメント参照）
            RenderSceneContent(screenBuffer, entries, sceneRenderer, pipelineBinder, lightsCache,
                phaseOffset, static_cast<int>(pass), /*disableNestedMultiPassDither=*/true);

            // このパスの結果を1/N重みで蓄積バッファへ加算合成する
            screenWideScratchColor_->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            screenWideAccumColor_->SetCommandList(commandList);
            screenWideAccumColor_->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
            {
                const auto accumRtv = screenWideAccumColor_->GetCPUDescriptorHandle();
                commandList->OMSetRenderTargets(1, &accumRtv, FALSE, nullptr);
                commandList->RSSetViewports(1, &viewport);
                commandList->RSSetScissorRects(1, &scissor);

                pipelineBinder.UsePipeline(kAccumulatePipeline);
                auto &binder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, kAccumulatePipeline);
                binder.SetCommandList(commandList);

                MultiPassDitherAccumulateConstants cbData{};
                cbData.invPassCount = 1.0f / static_cast<float>(passCount);
                auto *accumulateConstantBuffer = resourceContainer_->GetOrCreateConstantBuffer(
                    "ScreenWideDitherAccumulateCB", sizeof(MultiPassDitherAccumulateConstants));
                if (accumulateConstantBuffer) {
                    if (auto *mapped = accumulateConstantBuffer->Map()) {
                        std::memcpy(mapped, &cbData, sizeof(cbData));
                    }
                }
                binder.Bind("Pixel:MultiPassDitherAccumulateCB", accumulateConstantBuffer);
                binder.Bind("Pixel:gSceneTexture", screenWideScratchColorSrv_->GetGPUDescriptorHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
                commandList->DrawInstanced(3, 1, 0, 0);
                ++drawCallCount_;
            }
            screenWideAccumColor_->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        // 蓄積結果（平均済みの画面全体）をオーナーの実際のカラー・深度へコピーする。
        // 深度は最後のパスのスクラッチ深度をそのまま複製する（後続のポストエフェクトが
        // 深度を参照する場合に備える。realDepthはScreenBuffer側の実体のためCOPY_DESTを
        // 都度明示的に登録しておく必要がある。RenderMultiPassDither参照）
        realDepth->AddTransitionState(D3D12_RESOURCE_STATE_COPY_DEST);
        realDepth->SetCommandList(commandList);
        screenWideScratchDepth_->SetCommandList(commandList);
        realDepth->TransitionTo(D3D12_RESOURCE_STATE_COPY_DEST);
        screenWideScratchDepth_->TransitionTo(D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandList->CopyResource(realDepth->GetResource(), screenWideScratchDepth_->GetResource());
        realDepth->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);

        realColor->SetCommandList(commandList);
        realColor->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
        {
            const auto rtv = realColor->GetCPUDescriptorHandle();
            commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissor);

            pipelineBinder.UsePipeline(kCopyPipeline);
            auto &binder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, kCopyPipeline);
            binder.SetCommandList(commandList);
            // TemporalBlendCopyPS.hlslの変数名はgSceneTexture（gTextureではない）
            binder.Bind("Pixel:gSceneTexture", screenWideAccumColorSrv_->GetGPUDescriptorHandle());
            SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
            commandList->DrawInstanced(3, 1, 0, 0);
            ++drawCallCount_;
        }

        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
        {
            const auto rtv = realColor->GetCPUDescriptorHandle();
            const auto dsv = realDepth->GetCPUDescriptorHandle();
            commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        }

        lightsCache.valid = false;
    }

    RenderEditorDebugOverlay(screenBuffer, pipelineBinder, sceneRenderer);
    RenderPostProcess(screenBuffer, pipelineBinder, sceneRenderer->GetTargetOwner(screenBuffer), sceneRenderer);
    screenBuffer->EndDraw();
}

void Renderer::RenderGpuParticles(IRenderTarget *target, PipelineBinder &pipelineBinder, SceneRenderer *sceneRenderer,
    CameraLightsBindCache &lightsCache) {
    if (!target || !sceneRenderer) return;
    const auto &emitters = sceneRenderer->GetGpuParticleEmitters();
    if (emitters.empty()) return;

    auto *editorTarget = sceneRenderer->GetEditorTarget();
    auto *commandList = target->GetCommandList();

    for (auto *emitter : emitters) {
        if (!emitter || !emitter->IsActive()) continue;
        const std::string &pipelineName = emitter->GetPipelineName();
        if (pipelineName.empty() || !pipelineManager_->HasPipeline(pipelineName)) continue;
        if (IsExcludedAsEditorOnly(emitter, target, sceneRenderer)) continue;

        std::vector<IRenderTarget *> collectedTargets;
        auto *targetObject = emitter->GetTargetObject();
        SceneRenderer::CollectRenderTargets(targetObject, collectedTargets);
        if (editorTarget && editorTarget->IsRenderTargetAvailable()) {
            collectedTargets.push_back(editorTarget);
        }
        bool matches = false;
        for (auto *candidate : collectedTargets) {
            if (candidate != target || !target->IsRenderTargetAvailable()) continue;
            if (target != editorTarget && !emitter->IsRenderTargetIncluded(target)) continue;
            matches = true;
            break;
        }
        if (!matches) continue;

        const auto meshHandle = emitter->GetMeshHandle();
        if (meshHandle == ModelManager::kInvalidHandle) continue;
        const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(meshHandle);
        if (!meshBuffers || !meshBuffers->vertexBuffer || !meshBuffers->indexBuffer) continue;

        auto *instanceMatrixBuffer = emitter->GetGpuInstanceMatrixBuffer(Passkey<Renderer>{});
        if (!instanceMatrixBuffer) continue;
        const std::uint32_t instanceCount = emitter->GetGpuParticleCapacity(Passkey<Renderer>{});
        if (instanceCount == 0) continue;

        pipelineBinder.UsePipeline(pipelineName);
        auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
        shaderBinder.SetCommandList(commandList);

        BindCameraAndLights(commandList, target, pipelineName, sceneRenderer, pipelineBinder, lightsCache);

        instanceMatrixBuffer->SetCommandList(commandList);
        shaderBinder.Bind("Vertex:gTransformationMatrices", instanceMatrixBuffer);

        // マテリアルはDrawBatchと同じくバッチ全体で1つ（パーティクルごとの色は無し）
        {
            const auto materialHandle = emitter->GetMaterialHandle();
            auto *material = MaterialManager::GetMaterial(materialHandle);
            if (material) material->ResolveTextureHandles();

            auto key = MakeBatchKey(target, pipelineName, meshHandle, materialHandle, "gpu_particle_material");

            // マテリアルはDrawBatchと同じくバッチ全体で1つ（パーティクルごとの色は無し）ため、
            // パックした1要素分のバイト列をそのままインスタンス数分複製する
            const auto &pipelineInfo = pipelineManager_->GetPipeline(pipelineName);
            auto elementTemplate = BuildMaterialElementBytes(pipelineInfo, material);
            const std::uint32_t stride = pipelineInfo.GetMaterialLayout().totalByteSize;
            if (stride > 0) {
                // エミッター単位でマテリアルは1つ（パーティクルごとのテクスチャオーバーライドは無い）ため、
                // テンプレート1回分だけ書き込めば、以降の per-instance memcpy で全パーティクルに伝播する
                WriteMaterialField(pipelineInfo, elementTemplate.data(), stride,
                    "textureIndex", ResolveInstanceTextureIndex(TextureManager::kInvalidHandle, material));
                WriteMaterialField(pipelineInfo, elementTemplate.data(), stride,
                    "samplerIndex", ResolveInstanceSamplerIndex(SamplerManager::kInvalidHandle, material));
                auto *materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, stride, instanceCount);
                if (materialBuffer) {
                    auto *mapped = static_cast<std::byte *>(materialBuffer->Map());
                    if (mapped) {
                        for (std::uint32_t i = 0; i < instanceCount; ++i) {
                            std::memcpy(mapped + static_cast<size_t>(i) * stride, elementTemplate.data(), stride);
                        }
                        shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
                    }
                }
            }

            // バインドレステクスチャ配列（Texture2D gTextures[]）のテーブル。DrawBatchと同じ固定ハンドルのため
            // 該当バインディングを持たないパイプラインでは何もせずfalseを返すだけなので無害
            shaderBinder.Bind("Pixel:gTextures", directXCommon_->GetTextureBindlessBaseHandleForRenderer(Passkey<Renderer>{}));
        // 既定6種のサンプラー配列（gSamplers[6]）も同様に、SamplerHeap予約レンジを指す固定ハンドルを
        // 1回だけバインドする（静的サンプラーでは実行時インデックスに対応できないため通常のテーブル扱い）
        shaderBinder.Bind("Pixel:gSamplers", directXCommon_->GetSamplerBindlessBaseHandleForRenderer(Passkey<Renderer>{}));
            BindExtraTextureParameters(&shaderBinder, material);
        }

        pipelineBinder.SetVertexBuffer(meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
        pipelineBinder.SetIndexBuffer(meshBuffers->indexBuffer.get());
        commandList->DrawIndexedInstanced(meshBuffers->indexCount, instanceCount, 0, 0, 0);
        ++drawCallCount_;
    }
}


} // namespace KashipanEngine
