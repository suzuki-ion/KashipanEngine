#include "Renderer.h"
#include "Core/Window.h"
#include "Core/DirectXCommon.h"
#include "Graphics/PipelineManager.h"

namespace KashipanEngine {

void Renderer::RenderFrame(Passkey<GraphicsEngine>) {
    for (auto &[hwnd, binder] : windowBinders_) {
        binder.Invalidate();
    }
    RenderPasses3D();
    RenderPasses2D();
}

void Renderer::RegisterWindow(Passkey<Window>, HWND hwnd, ID3D12GraphicsCommandList *commandList) {
    if (!hwnd) return;
    auto it = windowBinders_.find(hwnd);
    if (it == windowBinders_.end()) {
        PipelineBinder binder;
        binder.SetManager(pipelineManager_);
        if (commandList) binder.SetCommandList(commandList);
        binder.Invalidate();
        windowBinders_.emplace(hwnd, std::move(binder));
    }
}

void Renderer::RenderPasses2D() {
    if (renderPasses2D_.empty()) return;
    for (const auto &passInfo : renderPasses2D_) {
        if (!passInfo.window ||
            passInfo.window->IsPendingDestroy() || passInfo.window->IsMinimized() ||
            !passInfo.renderFunction ||
            (passInfo.renderConditionFunction && !passInfo.renderConditionFunction())) {
            continue;
        }
        HWND hwnd = passInfo.window->GetWindowHandle();
        auto it = windowBinders_.find(hwnd);
        if (it == windowBinders_.end()) continue;
        it->second.UsePipeline(passInfo.pipelineName);
        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, passInfo.pipelineName);
        auto &pipelineBinder = it->second;
        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, passInfo.window->GetWindowHandle());
        shaderVariableBinder.SetCommandList(commandList);
        auto renderCommand = passInfo.renderFunction(shaderVariableBinder, pipelineBinder);
        IssueRenderCommand(commandList, renderCommand);
    }
    renderPasses2D_.clear();
}

void Renderer::RenderPasses3D() {
    if (renderPasses3D_.empty()) return;
    for (const auto &passInfo : renderPasses3D_) {
        if (!passInfo.window ||
            passInfo.window->IsPendingDestroy() || passInfo.window->IsMinimized() || !passInfo.window->IsActive() ||
            !passInfo.renderFunction ||
            (passInfo.renderConditionFunction && !passInfo.renderConditionFunction())) {
            continue;
        }
        HWND hwnd = passInfo.window->GetWindowHandle();
        auto it = windowBinders_.find(hwnd);
        if (it == windowBinders_.end()) continue;
        it->second.UsePipeline(passInfo.pipelineName);
        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, passInfo.pipelineName);
        auto &pipelineBinder = it->second;
        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, passInfo.window->GetWindowHandle());
        shaderVariableBinder.SetCommandList(commandList);
        auto renderCommand = passInfo.renderFunction(shaderVariableBinder, pipelineBinder);
        IssueRenderCommand(commandList, renderCommand);
    }
    renderPasses3D_.clear();
}

void Renderer::IssueRenderCommand(ID3D12GraphicsCommandList *commandList, const RenderCommand &renderCommand) {
    if (!commandList) return;
    if (renderCommand.indexCount > 0) {
        commandList->DrawIndexedInstanced(
            renderCommand.indexCount,
            renderCommand.instanceCount,
            renderCommand.startIndexLocation,
            renderCommand.baseVertexLocation,
            renderCommand.startInstanceLocation
        );
    } else if (renderCommand.vertexCount > 0) {
        commandList->DrawInstanced(
            renderCommand.vertexCount,
            renderCommand.instanceCount,
            renderCommand.startVertexLocation,
            renderCommand.startInstanceLocation
        );
    }
}

} // namespace KashipanEngine