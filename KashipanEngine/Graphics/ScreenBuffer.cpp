#include "ScreenBuffer.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "Graphics/Resources/IGraphicsResource.h"
#include <algorithm>
#include <vector>

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

std::unordered_map<ScreenBuffer*, std::unique_ptr<ScreenBuffer>> ScreenBuffer::sBufferMap_{};

ScreenBuffer* ScreenBuffer::Create(Window* targetWindow, std::uint32_t width, std::uint32_t height,
    RenderDimension dimension, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat) {
    std::unique_ptr<ScreenBuffer> buffer(new ScreenBuffer());
    auto* raw = buffer.get();

    if (!raw->Initialize(targetWindow, width, height, dimension, colorFormat, depthFormat)) {
        return nullptr;
    }

    sBufferMap_.emplace(raw, std::move(buffer));
    return raw;
}

void ScreenBuffer::AllDestroy(Passkey<GameEngine>) {
    // Window と同様に、所有コンテナをクリアすることで一括破棄
    sBufferMap_.clear();
}

size_t ScreenBuffer::GetBufferCount() {
    return sBufferMap_.size();
}

bool ScreenBuffer::IsExist(ScreenBuffer* buffer) {
    if (!buffer) return false;
    return sBufferMap_.find(buffer) != sBufferMap_.end();
}

namespace {
struct RecordState {
    ID3D12GraphicsCommandList* list = nullptr;
    bool discard = false;
    bool started = false;
};

static std::unordered_map<ScreenBuffer*, RecordState> sRecordStates;
} // namespace

bool ScreenBuffer::IsRecording(Passkey<Renderer>) const noexcept {
    auto it = sRecordStates.find(const_cast<ScreenBuffer*>(this));
    if (it == sRecordStates.end()) return false;
    return it->second.started;
}

void ScreenBuffer::AllBeginRecord(Passkey<Renderer>) {
    sRecordStates.clear();
    sRecordStates.reserve(sBufferMap_.size());

    for (auto& [ptr, owning] : sBufferMap_) {
        if (!ptr || !owning) continue;
        auto* cl = ptr->BeginRecord();
        RecordState st;
        st.list = cl;
        st.discard = (cl == nullptr);
        st.started = (cl != nullptr);
        sRecordStates.emplace(ptr, st);
    }
}

std::vector<ID3D12CommandList*> ScreenBuffer::AllEndRecord(Passkey<Renderer>) {
    std::vector<ID3D12CommandList*> lists;
    lists.reserve(sRecordStates.size());

    for (auto& [ptr, st] : sRecordStates) {
        if (!ptr) continue;
        if (!st.started) continue;

        if (!ptr->EndRecord(st.discard)) {
            continue;
        }
        lists.push_back(st.list);
    }

    sRecordStates.clear();
    return lists;
}

ScreenBuffer::~ScreenBuffer() {
    Destroy();
}

bool ScreenBuffer::Initialize(Window* targetWindow, std::uint32_t width, std::uint32_t height,
    RenderDimension dimension, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat) {
    Destroy();

    targetWindow_ = targetWindow;
    width_ = width;
    height_ = height;
    dimension_ = dimension;
    colorFormat_ = colorFormat;
    depthFormat_ = depthFormat;

    if (!sDirectXCommon_) return false;

    commandSlotIndex_ = sDirectXCommon_->AcquireScreenBufferCommandObjects(Passkey<ScreenBuffer>{});
    auto* cmd = sDirectXCommon_->GetScreenBufferCommandObjects(Passkey<ScreenBuffer>{}, commandSlotIndex_);
    if (!cmd || !cmd->commandAllocator || !cmd->commandList) {
        commandSlotIndex_ = -1;
        return false;
    }

    commandAllocator_ = cmd->commandAllocator.Get();
    commandList_ = cmd->commandList.Get();

    renderTarget_ = std::make_unique<RenderTargetResource>(width_, height_, colorFormat_);
    depthStencil_ = std::make_unique<DepthStencilResource>(width_, height_, depthFormat_, 1.0f, static_cast<UINT8>(0));
    shaderResource_ = std::make_unique<ShaderResourceResource>(renderTarget_.get());

    return renderTarget_ && depthStencil_ && shaderResource_;
}

void ScreenBuffer::Destroy() {
    DetachFromRenderer();

    for (auto& c : postEffectComponents_) {
        if (!c) continue;
        c->Finalize();
    }
    postEffectComponents_.clear();

    shaderResource_.reset();
    depthStencil_.reset();
    renderTarget_.reset();

    commandList_ = nullptr;
    commandAllocator_ = nullptr;

    if (sDirectXCommon_ && commandSlotIndex_ >= 0) {
        sDirectXCommon_->ReleaseScreenBufferCommandObjects(Passkey<ScreenBuffer>{}, commandSlotIndex_);
    }
    commandSlotIndex_ = -1;

    width_ = 0;
    height_ = 0;
    targetWindow_ = nullptr;
}

ID3D12GraphicsCommandList* ScreenBuffer::BeginRecord() {
    if (!commandAllocator_ || !commandList_) return nullptr;

    HRESULT hr = commandAllocator_->Reset();
    if (FAILED(hr)) return nullptr;

    hr = commandList_->Reset(commandAllocator_, nullptr);
    if (FAILED(hr)) return nullptr;

    if (!renderTarget_ || !depthStencil_) return nullptr;

    renderTarget_->SetCommandList(commandList_);
    depthStencil_->SetCommandList(commandList_);

    // 明示遷移
    if (!renderTarget_->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) {
        return nullptr;
    }
    if (!depthStencil_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE)) {
        return nullptr;
    }

    const auto rtv = renderTarget_->GetCPUDescriptorHandle();
    const auto dsv = depthStencil_->GetCPUDescriptorHandle();

    commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    renderTarget_->ClearRenderTargetView();
    depthStencil_->ClearDepthStencilView();

    D3D12_VIEWPORT vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    D3D12_RECT sc{};
    sc.left = 0;
    sc.top = 0;
    sc.right = static_cast<LONG>(width_);
    sc.bottom = static_cast<LONG>(height_);

    commandList_->RSSetViewports(1, &vp);
    commandList_->RSSetScissorRects(1, &sc);

    auto* srvHeap = IGraphicsResource::GetSRVHeap(Passkey<ScreenBuffer>{});
    auto* samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<ScreenBuffer>{});
    if (srvHeap && samplerHeap) {
        ID3D12DescriptorHeap* ppHeaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
        commandList_->SetDescriptorHeaps(2, ppHeaps);
    }

    return commandList_;
}

bool ScreenBuffer::EndRecord(bool discard) {
    if (!commandList_) return false;

    // RenderTarget を SRV として使う想定なら、描画後に PixelShaderResource に遷移させる
    if (renderTarget_) {
        renderTarget_->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    if (depthStencil_) {
        depthStencil_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_READ);
    }

    HRESULT hr = commandList_->Close();
    if (FAILED(hr)) return false;

    (void)discard;
    return true;
}

bool ScreenBuffer::RegisterPostEffectComponent(std::unique_ptr<IPostEffectComponent> component) {
    if (!component) return false;

    const std::string type = component->GetComponentType();
    const size_t maxCount = component->GetMaxComponentCountPerBuffer();

    size_t existingCount = 0;
    for (auto& c : postEffectComponents_) {
        if (c && c->GetComponentType() == type) {
            ++existingCount;
        }
    }
    if (existingCount >= maxCount) return false;

    component->SetOwnerBuffer(this);
    component->Initialize();

    postEffectComponents_.push_back(std::move(component));

    std::stable_sort(postEffectComponents_.begin(), postEffectComponents_.end(),
        [](const std::unique_ptr<IPostEffectComponent>& a, const std::unique_ptr<IPostEffectComponent>& b) {
            if (!a) return false;
            if (!b) return true;
            return a->GetApplyPriority() < b->GetApplyPriority();
        });

    return true;
}

void ScreenBuffer::AttachToRenderer(const std::string& pipelineName, const std::string& passName) {
    if (!targetWindow_) return;

    auto* renderer = Window::GetRenderer(Passkey<ScreenBuffer>{});
    if (!renderer) return;

    if (persistentScreenPassHandle_) {
        renderer->UnregisterPersistentScreenPass(persistentScreenPassHandle_);
        persistentScreenPassHandle_ = {};
    }

    auto passOpt = CreateScreenPass(pipelineName, passName);
    if (!passOpt) return;

    persistentScreenPassHandle_ = renderer->RegisterPersistentScreenPass(std::move(*passOpt));
}

void ScreenBuffer::DetachFromRenderer() {
    if (!persistentScreenPassHandle_) return;

    auto* renderer = Window::GetRenderer(Passkey<ScreenBuffer>{});
    if (renderer) {
        renderer->UnregisterPersistentScreenPass(persistentScreenPassHandle_);
    }
    persistentScreenPassHandle_ = {};
}

std::optional<ScreenBufferPass> ScreenBuffer::CreateScreenPass(const std::string& pipelineName, const std::string& passName) {
    ScreenBufferPass pass(Passkey<ScreenBuffer>{});
    pass.buffer = this;
    pass.pipelineName = pipelineName;
    pass.passName = passName;

    pass.renderType = RenderType::Standard;
    pass.batchKey = 0;

    pass.batchedRenderFunction = [this](ShaderVariableBinder& binder, std::uint32_t instanceCount) -> bool {
        return RenderBatched(binder, instanceCount);
    };

    return pass;
}

bool ScreenBuffer::RenderBatched(ShaderVariableBinder& binder, std::uint32_t instanceCount) {
    (void)instanceCount;

    // コンポーネントからシェーダー変数バインドを行える設計。
    for (auto& c : postEffectComponents_) {
        if (!c) continue;
        auto r = c->BindShaderVariables(&binder);
        if (r != std::nullopt && r.value() == false) return false;
    }

    // エフェクト適用
    for (auto& c : postEffectComponents_) {
        if (!c) continue;
        auto r = c->Apply();
        if (r != std::nullopt && r.value() == false) return false;
    }

    return true;
}

void ScreenBuffer::MarkDiscard(Passkey<Renderer>, ScreenBuffer* buffer) {
    if (!buffer) return;
    auto it = sRecordStates.find(buffer);
    if (it == sRecordStates.end()) return;
    it->second.discard = true;
}

#if defined(USE_IMGUI)
namespace {
ImTextureID ToImGuiTextureIdFromGpuHandle(D3D12_GPU_DESCRIPTOR_HANDLE h) {
    return (ImTextureID)(uintptr_t)h.ptr;
}
}

void ScreenBuffer::ShowImGuiScreenBuffersWindow() {
    if (!ImGui::Begin("ScreenBuffer - Buffers")) {
        ImGui::End();
        return;
    }

    ImGui::Text("ScreenBuffers: %d", static_cast<int>(ScreenBuffer::GetBufferCount()));

    static ScreenBuffer* sSelected = nullptr;

    if (ImGui::BeginTable("##ScreenBufferList", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, 220))) {
        ImGui::TableSetupColumn("Ptr", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("SRV", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Select");
        ImGui::TableHeadersRow();

        for (auto& kv : sBufferMap_) {
            ScreenBuffer* ptr = kv.first;
            if (!ptr) continue;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%p", (void*)ptr);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%ux%u", ptr->GetWidth(), ptr->GetHeight());

            ImGui::TableSetColumnIndex(2);
            auto* srv = ptr->GetShaderResource();
            if (srv) {
                const auto h = srv->GetGPUDescriptorHandle();
                ImGui::Text("0x%llX", static_cast<unsigned long long>(h.ptr));
            } else {
                ImGui::TextUnformatted("-");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(ptr);
            const bool isSel = (sSelected == ptr);
            if (ImGui::Selectable("##select", isSel, ImGuiSelectableFlags_SpanAllColumns)) {
                sSelected = ptr;
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::Separator();

    if (sSelected && ScreenBuffer::IsExist(sSelected) && sSelected->GetShaderResource()) {
        const auto hdl = sSelected->GetShaderResource()->GetGPUDescriptorHandle();
        const ImVec2 size{
            static_cast<float>(sSelected->GetWidth()),
            static_cast<float>(sSelected->GetHeight())
        };

        ImGui::Text("Selected: %p", (void*)sSelected);
        ImGui::Image(ToImGuiTextureIdFromGpuHandle(hdl), size);
    } else {
        ImGui::TextUnformatted("No ScreenBuffer selected or SRV not ready.");
    }

    ImGui::End();
}
#endif

} // namespace KashipanEngine
