#include "RendererInternal.h"

namespace KashipanEngine {

using namespace RendererInternal;

void Renderer::RenderToTarget(IRenderTarget *target,
    std::span<const SceneRenderer::DrawEntry> entries,
    SceneRenderer *sceneRenderer) {
    if (!target) return;

    target->BeginDraw();
    auto *commandList = target->GetCommandList();
    if (!commandList) return;

    PipelineBinder pipelineBinder(commandList, pipelineManager_);

    // エディター用描画先の場合、他の描画より先に背景（単色 or テクスチャ）を描画する
    if (target->GetRenderTargetKind() == RenderTargetKind::ScreenBuffer) {
        RenderEditorBackground(static_cast<ScreenBuffer *>(target), pipelineBinder, sceneRenderer);
    }

    // 同一パイプライン・描画先が連続する間はカメラ・ライトの再バインドを省略するための共有キャッシュ
    // （DrawBatchのループ・RenderTextRenderers・RenderGpuParticlesの全体で1つを使い回す）
    CameraLightsBindCache lightsCache;

    // 同一（パイプライン・メッシュ・サブメッシュ・マテリアル）の連続範囲をバッチとしてまとめて描画
    size_t begin = 0;
    while (begin < entries.size()) {
        const auto &first = entries[begin];
        size_t end = begin;
        while (end < entries.size()) {
            const auto &other = entries[end];
            if (other.pipelineName != first.pipelineName ||
                other.meshHandle != first.meshHandle ||
                other.materialHandle != first.materialHandle ||
                other.indexStart != first.indexStart ||
                other.indexCount != first.indexCount ||
                other.skinnedVertexBuffer != first.skinnedVertexBuffer) {
                break;
            }
            ++end;
        }

        DrawBatch(target, pipelineBinder, entries.subspan(begin, end - begin), sceneRenderer, lightsCache);
        begin = end;
    }

    // TextRenderer（文字ごとにアトラス内UVが異なるため通常のバッチには乗らない）は専用パスで描画する
    RenderTextRenderers(target, pipelineBinder, sceneRenderer, lightsCache);

    // GPU Simulation有効なParticleSystem2D/3D（ProcessGpuParticlesが結果を書き込み済み）も専用パスで描画する
    RenderGpuParticles(target, pipelineBinder, sceneRenderer, lightsCache);

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

void Renderer::DrawBatch(IRenderTarget *target,
    PipelineBinder &pipelineBinder,
    std::span<const SceneRenderer::DrawEntry> batch,
    SceneRenderer *sceneRenderer,
    CameraLightsBindCache &lightsCache) {
    if (batch.empty()) return;

    const auto &first = batch.front();
    const std::string &pipelineName = first.pipelineName;
    // Object2D系パイプラインかどうか（マテリアル構造体のレイアウトが異なる）
    const bool isObject2D = pipelineName.rfind("Object2D", 0) == 0;
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
        if (isObject2D) {
            Material2DElement element;
            if (material) {
                element.color = material->color;
                element.uvTransform = material->uvTransform;
                element.useTexture = (material->textureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
            }
            std::vector<Material2DElement> elements(instanceCount, element);
            auto *materialBuffer = resourceContainer_->GetOrUpdateStructuredBuffer(key, sizeof(Material2DElement), instanceCount, elements.data());
            if (materialBuffer) {
                shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
            }
        } else {
            MaterialElement element;
            if (material) {
                element.enableLighting = material->enableLighting ? 1.0f : 0.0f;
                element.enableEnvironmentMapping = (material->environmentHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                element.enableShadowMapProjection = material->enableShadowMapProjection ? 1.0f : 0.0f;
                element.useTexture = (material->textureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                element.color = material->color;
                element.uvTransform = material->uvTransform;
                element.shininess = material->shininess;
                element.specularColor = material->specularColor;
                element.environmentCoefficient = material->environmentCoefficient;
                element.rimColor = material->rimColor;
                element.rimPower = material->rimPower;
                element.rimIntensity = material->rimIntensity;
                element.useNormalMap = (material->normalMapHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
            }
            // instanceColor/instanceColorBlendModeはインスタンス（MeshRenderer）ごとに異なり得るため、
            // マテリアル本体の値のみ共有した上で、インスタンスごとに個別の値を書き込む
            std::vector<MaterialElement> elements(instanceCount, element);
            for (std::uint32_t i = 0; i < instanceCount; ++i) {
                elements[i].instanceColor = batch[i].instanceColor;
                elements[i].instanceColorBlendMode = static_cast<float>(batch[i].instanceColorBlendMode);
            }
            auto *materialBuffer = resourceContainer_->GetOrUpdateStructuredBuffer(key, sizeof(MaterialElement), instanceCount, elements.data());
            if (materialBuffer) {
                shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
            }
        }

        // マテリアルのテクスチャ・サンプラーバインド（未設定の場合は既定値をバインドする）
        if (material && material->textureHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", material->textureHandle);
        } else {
            const auto fallbackHandle = TextureManager::GetTextureFromFileName("white1x1.png");
            if (fallbackHandle != TextureManager::kInvalidHandle) {
                TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", fallbackHandle);
            }
        }
        if (material && material->environmentHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gEnvironmentMap", material->environmentHandle);
        }
        if (material && material->normalMapHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gNormalMap", material->normalMapHandle);
        }
        if (material && material->samplerHandle != SamplerManager::kInvalidHandle) {
            SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", material->samplerHandle);
        } else {
            SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearWrap);
        }
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

void Renderer::RenderTextRenderers(IRenderTarget *target, PipelineBinder &pipelineBinder, SceneRenderer *sceneRenderer,
    CameraLightsBindCache &lightsCache) {
    if (!target || !sceneRenderer) return;

    //--------- このターゲットに適用されるTextRendererを収集する（CollectSortableEntriesと同じフィルタ条件） ---------//
    struct TextTargetEntry {
        TextRenderer *renderer = nullptr;
        std::string pipelineName;
    };
    std::vector<TextTargetEntry> applicable;
    std::vector<IRenderTarget *> collectedTargets;
    auto *editorTarget = sceneRenderer->GetEditorTarget();

    for (auto *renderer : sceneRenderer->GetTextRenderers()) {
        if (!renderer || !renderer->IsActive()) continue;
        const std::string &pipelineName = renderer->GetPipelineName();
        if (pipelineName.empty() || !pipelineManager_->HasPipeline(pipelineName)) continue;

        if (IsExcludedAsEditorOnly(renderer, target, sceneRenderer)) continue;

        auto *targetObject = renderer->GetTargetObject();
        SceneRenderer::CollectRenderTargets(targetObject, collectedTargets);
        if (editorTarget && editorTarget->IsRenderTargetAvailable()) {
            collectedTargets.push_back(editorTarget);
        }

        bool matches = false;
        for (auto *candidate : collectedTargets) {
            if (candidate != target || !target->IsRenderTargetAvailable()) continue;
            if (target != editorTarget && !renderer->IsRenderTargetIncluded(target)) continue;
            matches = true;
            break;
        }
        if (!matches) continue;

        applicable.push_back(TextTargetEntry{ renderer, pipelineName });
    }
    if (applicable.empty()) return;

    //--------- (パイプライン名, フォントハンドル) ごとにグループ化して文字インスタンスをまとめる ---------//
    std::map<std::pair<std::string, FontManager::FontHandle>, std::vector<TextRenderer::RenderCharacterInstance>> groups;
    for (const auto &entry : applicable) {
        const auto fontHandle = entry.renderer->GetFontHandle();
        if (fontHandle == FontManager::kInvalidHandle) continue;
        auto instances = entry.renderer->GetRenderInstances();
        if (instances.empty()) continue;
        auto &bucket = groups[std::make_pair(entry.pipelineName, fontHandle)];
        bucket.insert(bucket.end(), instances.begin(), instances.end());
    }
    if (groups.empty()) return;

    static const ModelManager::ModelHandle kRect2DMeshHandle = ModelManager::GetModelHandleFromAssetPath("PrimitiveMesh-Rect2D");
    const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(kRect2DMeshHandle);
    if (!meshBuffers || !meshBuffers->vertexBuffer || !meshBuffers->indexBuffer) return;

    auto *commandList = target->GetCommandList();

    for (const auto &[key, instances] : groups) {
        const std::string &pipelineName = key.first;
        const FontManager::FontHandle fontHandle = key.second;
        if (instances.empty()) continue;

        pipelineBinder.UsePipeline(pipelineName);
        auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pipelineName);
        shaderBinder.SetCommandList(commandList);

        // カメラの定数バッファバインド（ライト関連バッファも一緒にバインドされるが、
        // Text2DのシェーダーはgPointLights等を参照しないため無害。Object2D系の描画と同じ扱い）
        BindCameraAndLights(commandList, target, pipelineName, sceneRenderer, pipelineBinder, lightsCache);

        const std::uint32_t instanceCount = static_cast<std::uint32_t>(instances.size());

        // ワールド行列のインスタンスバッファ
        {
            auto key2 = MakeBatchKey(target, pipelineName, kRect2DMeshHandle, fontHandle, "text_transform");
            auto *instanceBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key2, sizeof(Matrix4x4), instanceCount);
            if (!instanceBuffer) continue;
            auto *mapped = static_cast<Matrix4x4 *>(instanceBuffer->Map());
            if (!mapped) continue;
            for (std::uint32_t i = 0; i < instanceCount; ++i) {
                mapped[i] = instances[i].worldMatrix;
            }
            shaderBinder.Bind("Vertex:gTransformationMatrices", instanceBuffer);
        }

        // 文字ごとの色・UV矩形・SDFパラメータ
        {
            auto key2 = MakeBatchKey(target, pipelineName, kRect2DMeshHandle, fontHandle, "text_material");
            auto *materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key2, sizeof(TextCharacterElement), instanceCount);
            if (!materialBuffer) continue;
            auto *mapped = static_cast<TextCharacterElement *>(materialBuffer->Map());
            if (!mapped) continue;
            for (std::uint32_t i = 0; i < instanceCount; ++i) {
                const auto &src = instances[i];
                TextCharacterElement dst;
                dst.color = src.color;
                dst.uvRect = Vector4(src.u0, src.v0, src.u1, src.v1);
                dst.boldWeight = src.boldWeight;
                mapped[i] = dst;
            }
            shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
        }

        TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", FontManager::GetAtlasTextureHandle(fontHandle));
        SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearWrap);

        pipelineBinder.SetVertexBuffer(meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
        pipelineBinder.SetIndexBuffer(meshBuffers->indexBuffer.get());
        commandList->DrawIndexedInstanced(meshBuffers->indexCount, instanceCount, 0, 0, 0);
        ++drawCallCount_;
    }
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

        const bool isObject2D = pipelineName.rfind("Object2D", 0) == 0;

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
            if (isObject2D) {
                Material2DElement element;
                if (material) {
                    element.color = material->color;
                    element.uvTransform = material->uvTransform;
                    element.useTexture = (material->textureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                }
                auto *materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(Material2DElement), instanceCount);
                if (materialBuffer) {
                    auto *mapped = static_cast<Material2DElement *>(materialBuffer->Map());
                    if (mapped) {
                        for (std::uint32_t i = 0; i < instanceCount; ++i) mapped[i] = element;
                        shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
                    }
                }
            } else {
                MaterialElement element;
                if (material) {
                    element.enableLighting = material->enableLighting ? 1.0f : 0.0f;
                    element.enableEnvironmentMapping = (material->environmentHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                    element.enableShadowMapProjection = material->enableShadowMapProjection ? 1.0f : 0.0f;
                    element.useTexture = (material->textureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                    element.color = material->color;
                    element.uvTransform = material->uvTransform;
                    element.shininess = material->shininess;
                    element.specularColor = material->specularColor;
                    element.environmentCoefficient = material->environmentCoefficient;
                    element.rimColor = material->rimColor;
                    element.rimPower = material->rimPower;
                    element.rimIntensity = material->rimIntensity;
                    element.useNormalMap = (material->normalMapHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;
                }
                auto *materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(MaterialElement), instanceCount);
                if (materialBuffer) {
                    auto *mapped = static_cast<MaterialElement *>(materialBuffer->Map());
                    if (mapped) {
                        for (std::uint32_t i = 0; i < instanceCount; ++i) mapped[i] = element;
                        shaderBinder.Bind("Pixel:gMaterials", materialBuffer);
                    }
                }
            }

            if (material && material->textureHandle != TextureManager::kInvalidHandle) {
                TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", material->textureHandle);
            } else {
                const auto fallbackHandle = TextureManager::GetTextureFromFileName("white1x1.png");
                if (fallbackHandle != TextureManager::kInvalidHandle) {
                    TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", fallbackHandle);
                }
            }
            if (material && material->environmentHandle != TextureManager::kInvalidHandle) {
                TextureManager::BindTexture(&shaderBinder, "Pixel:gEnvironmentMap", material->environmentHandle);
            }
            if (material && material->normalMapHandle != TextureManager::kInvalidHandle) {
                TextureManager::BindTexture(&shaderBinder, "Pixel:gNormalMap", material->normalMapHandle);
            }
            if (material && material->samplerHandle != SamplerManager::kInvalidHandle) {
                SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", material->samplerHandle);
            } else {
                SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearWrap);
            }
        }

        pipelineBinder.SetVertexBuffer(meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
        pipelineBinder.SetIndexBuffer(meshBuffers->indexBuffer.get());
        commandList->DrawIndexedInstanced(meshBuffers->indexCount, instanceCount, 0, 0, 0);
        ++drawCallCount_;
    }
}


} // namespace KashipanEngine
