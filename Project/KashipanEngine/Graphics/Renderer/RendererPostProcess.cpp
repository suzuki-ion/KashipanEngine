#include "RendererInternal.h"

namespace KashipanEngine {

using namespace RendererInternal;

void Renderer::RenderPostProcessOnlyTargets(SceneContext *sceneContext,
    const std::unordered_set<const IRenderTarget *> &renderedTargets) {
    auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;
    for (const auto &object : sceneContext->GetSceneObjects()) {
        if (!object || !object->IsActive()) continue;

        // アクティブなポストエフェクトコンポーネントを持つオブジェクトのみ対象
        bool hasActivePostProcess = false;
        for (auto *component : sceneRenderer->GetPostProcessComponentsFor(object.get())) {
            if (component && component->IsActive()) {
                hasActivePostProcess = true;
                break;
            }
        }
        if (!hasActivePostProcess) continue;

        for (auto *screenBufferObject : object->GetComponents<ScreenBufferObject>()) {
            if (!screenBufferObject || !screenBufferObject->IsActive()) continue;
            auto *buffer = screenBufferObject->GetScreenBuffer();
            if (!buffer || !ScreenBuffer::IsExist(buffer)) continue;
            if (!buffer->IsRenderTargetAvailable()) continue;
            // 描画リスト経由で既に描画済みの場合はポストエフェクトも適用済み
            if (renderedTargets.find(buffer) != renderedTargets.end()) continue;

            buffer->BeginDraw();
            auto *commandList = buffer->GetCommandList();
            if (!commandList) continue;
            PipelineBinder pipelineBinder(commandList, pipelineManager_);
            RenderPostProcess(buffer, pipelineBinder, object.get(), sceneRenderer);
            buffer->EndDraw();
            // 今フレームの最終確定SRVをビューア用に記録する（詳細はScreenBuffer::SetPreviewSrvHandle参照）
            buffer->SetPreviewSrvHandle(Passkey<Renderer>{}, buffer->GetSrvHandle());
        }
    }
}


void Renderer::RenderEditorDebugOverlay(ScreenBuffer *screenBuffer,
    PipelineBinder &pipelineBinder,
    SceneRenderer *sceneRenderer) {
    if (!screenBuffer || !sceneRenderer) return;

    // エディター用描画先でなければ何もしない（通常の描画先にはデバッグ表示を出さない）
    auto *cameraBuffer = sceneRenderer->GetEditorCameraBuffer(screenBuffer);
    if (!cameraBuffer) return;

    const auto &settings = sceneRenderer->GetEditorDebugDraw();
    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    if (settings.showGrid && pipelineManager_->HasPipeline("DebugGrid")) {
        struct GridConstant {
            float fadeDistance = 100.0f;
            float padding[3]{};
        };
        GridConstant gridConstant{};
        gridConstant.fadeDistance = std::max(1.0f, settings.gridFadeDistance);

        auto *gridBuffer = resourceContainer_->GetOrCreateConstantBuffer("EditorDebugGrid", sizeof(GridConstant));
        if (gridBuffer) {
            if (auto *mapped = gridBuffer->Map()) {
                std::memcpy(mapped, &gridConstant, sizeof(gridConstant));
            }

            pipelineBinder.UsePipeline("DebugGrid");
            auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "DebugGrid");
            shaderBinder.SetCommandList(commandList);
            shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
            shaderBinder.Bind("Pixel:gCamera3D", cameraBuffer);
            shaderBinder.Bind("Vertex:GridCB", gridBuffer);
            shaderBinder.Bind("Pixel:GridCB", gridBuffer);
            // カメラ位置を中心とした大きな板ポリゴン（2三角形）を全画面ではなくワールド空間へ直接描画する
            commandList->DrawInstanced(6, 1, 0, 0);
        }
    }

    if (settings.showColliderGizmos && !settings.lines.empty() && pipelineManager_->HasPipeline("DebugLines")) {
        const size_t vertexCount = settings.lines.size();
        const size_t byteSize = sizeof(DebugLineVertex) * vertexCount;
        auto *vertexBuffer = resourceContainer_->GetOrCreateVertexBuffer("EditorDebugLines", byteSize);
        if (vertexBuffer) {
            if (auto *mapped = vertexBuffer->Map()) {
                std::memcpy(mapped, settings.lines.data(), byteSize);
            }

            pipelineBinder.UsePipeline("DebugLines");
            auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "DebugLines");
            shaderBinder.SetCommandList(commandList);
            shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
            pipelineBinder.SetVertexBuffer(vertexBuffer, sizeof(DebugLineVertex));
            commandList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);
        }
    }
}

void Renderer::RenderEditorBackground(ScreenBuffer *screenBuffer,
    PipelineBinder &pipelineBinder,
    SceneRenderer *sceneRenderer) {
    if (!screenBuffer || !sceneRenderer) return;

    // エディター用描画先でなければ何もしない（通常の描画先の背景には影響しない）
    if (!sceneRenderer->GetEditorCameraBuffer(screenBuffer)) return;
    if (!pipelineManager_->HasPipeline("Background")) return;

    const auto &settings = sceneRenderer->GetEditorDebugDraw();
    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    struct BackgroundConstant {
        Vector4 color{ 0.0f, 0.0f, 0.0f, 1.0f };
        float useTexture = 0.0f;
        float padding[3]{};
    };
    BackgroundConstant constant{};
    constant.color = settings.backgroundColor;
    constant.useTexture = (settings.backgroundTextureHandle != TextureManager::kInvalidHandle) ? 1.0f : 0.0f;

    auto *constantBuffer = resourceContainer_->GetOrCreateConstantBuffer("EditorBackground", sizeof(BackgroundConstant));
    if (!constantBuffer) return;
    if (auto *mapped = constantBuffer->Map()) {
        std::memcpy(mapped, &constant, sizeof(constant));
    }

    pipelineBinder.UsePipeline("Background");
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, "Background");
    shaderBinder.SetCommandList(commandList);
    shaderBinder.Bind("Pixel:BackgroundCB", constantBuffer);

    if (constant.useTexture > 0.5f) {
        TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", settings.backgroundTextureHandle);
    } else {
        const auto fallbackHandle = TextureManager::GetTextureFromFileName("white1x1.png");
        if (fallbackHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", fallbackHandle);
        }
    }
    SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearClamp);

    commandList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::RenderPostProcess(ScreenBuffer *screenBuffer,
    PipelineBinder &pipelineBinder,
    EmptyObject *ownerObject,
    SceneRenderer *sceneRenderer) {
    if (!screenBuffer || !ownerObject || !sceneRenderer) return;

    auto postProcessComponents = sceneRenderer->GetPostProcessComponentsFor(ownerObject);
    if (postProcessComponents.empty()) return;

    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList) return;

    // このスクリーンバッファへ描画したカメラの情報（AOの座標再構成、Outlineの深度線形化等に使う）
    IPostProcessComponent::CameraInfo cameraInfo;
    if (sceneRenderer) {
        cameraInfo = ResolveCameraInfoForPostProcess(sceneRenderer, screenBuffer);
    }

    for (auto *component : postProcessComponents) {
        if (!component || !component->IsActive()) continue;
        // 除外設定されているスクリーンバッファには適用しない
        if (!component->IsScreenBufferIncluded(screenBuffer)) continue;

        // ユーザーがカメラ設定（Near/Far等）を手動で複製せずに済むよう、カメラ情報を先に注入する
        component->SetCameraInfoInterface(Passkey<Renderer>{}, cameraInfo);

        // 中間レンダーターゲットを使う多段パス等はコンポーネント側のカスタム描画で行う
        IPostProcessComponent::CustomRenderContext customContext;
        customContext.screenBuffer = screenBuffer;
        customContext.commandList = commandList;
        customContext.pipelineManager = pipelineManager_;
        customContext.pipelineBinder = &pipelineBinder;
        customContext.getShaderBinder = [this, commandList](const std::string &name) -> ShaderVariableBinder & {
            auto &binder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, name);
            binder.SetCommandList(commandList);
            return binder;
        };
        if (component->RenderCustomInterface(Passkey<Renderer>{}, customContext)) {
            continue;
        }

        auto passes = component->BuildPassesInterface(Passkey<Renderer>{});
        for (const auto &pass : passes) {
            if (pass.pipelineName.empty() || !pipelineManager_->HasPipeline(pass.pipelineName)) continue;

            // 直前パスの結果をSRVとして参照可能にし、次の書き込み面へ切り替える
            screenBuffer->NextPass();

            pipelineBinder.UsePipeline(pass.pipelineName);
            auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, pass.pipelineName);
            shaderBinder.SetCommandList(commandList);

            // 直前パスの描画結果と既定サンプラー
            shaderBinder.Bind("Pixel:gTexture", screenBuffer->GetSrvHandle());
            SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", DefaultSampler::LinearClamp);

            // コンポーネントが要求する追加テクスチャ
            for (const auto &requirement : pass.textureBindRequirements) {
                if (!requirement.getHandle) continue;
                const auto handle = requirement.getHandle();
                if (handle.ptr == 0) continue;
                shaderBinder.Bind(requirement.variableName, handle);
            }

            // コンポーネントが要求するサンプラー
            for (const auto &requirement : pass.samplerBindRequirements) {
                SamplerManager::BindSampler(&shaderBinder, requirement.variableName, requirement.sampler);
            }

            // コンポーネントが要求する定数バッファ
            for (const auto &requirement : pass.constantBufferRequirements) {
                if (requirement.byteSize == 0 || !requirement.dataPtr) continue;
                const std::string key = MakeBatchKey(component, pass.pipelineName, 0, 0, requirement.variableName.c_str());
                auto *constantBuffer = resourceContainer_->GetOrCreateConstantBuffer(key, requirement.byteSize);
                if (!constantBuffer) continue;
                void *mapped = constantBuffer->Map();
                if (!mapped) continue;
                std::memcpy(mapped, requirement.dataPtr, requirement.byteSize);
                shaderBinder.Bind(requirement.variableName, constantBuffer);
            }

            // コンポーネントが要求するインスタンスバッファ（要素数1固定）
            for (const auto &requirement : pass.instanceBufferRequirements) {
                if (requirement.elementStride == 0 || !requirement.dataPtr) continue;
                const std::string key = MakeBatchKey(component, pass.pipelineName, 0, 1, requirement.variableName.c_str());
                auto *instanceBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, requirement.elementStride, 1);
                if (!instanceBuffer) continue;
                void *mapped = instanceBuffer->Map();
                if (!mapped) continue;
                std::memcpy(mapped, requirement.dataPtr, requirement.elementStride);
                shaderBinder.Bind(requirement.variableName, instanceBuffer);
            }

            // フルスクリーン三角形描画
            commandList->DrawInstanced(3, 1, 0, 0);
        }
    }
}


} // namespace KashipanEngine
