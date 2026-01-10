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

namespace {
// Window と同様に「破棄要求→フレーム終端で実破棄」のための pending リスト
static std::vector<ScreenBuffer*> sPendingDestroy;
} // namespace

D3D12_GPU_DESCRIPTOR_HANDLE ScreenBuffer::GetSrvHandle() const noexcept {
    const auto idx = GetReadIndex();
    return shaderResources_[idx] ? shaderResources_[idx]->GetGPUDescriptorHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

ScreenBuffer* ScreenBuffer::Create(std::uint32_t width, std::uint32_t height,
    DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat) {
    std::unique_ptr<ScreenBuffer> buffer(new ScreenBuffer());
    auto* raw = buffer.get();

    if (!raw->Initialize(width, height, colorFormat, depthFormat)) {
        return nullptr;
    }

    sBufferMap_.emplace(raw, std::move(buffer));
    return raw;
}

void ScreenBuffer::AllDestroy(Passkey<GameEngine>) {
    sBufferMap_.clear();
}

size_t ScreenBuffer::GetBufferCount() {
    return sBufferMap_.size();
}

bool ScreenBuffer::IsExist(ScreenBuffer* buffer) {
    if (!buffer) return false;
    return sBufferMap_.find(buffer) != sBufferMap_.end();
}

void ScreenBuffer::DestroyNotify(ScreenBuffer* buffer) {
    if (!buffer) return;
    if (!IsExist(buffer)) return;
    if (IsPendingDestroy(buffer)) return;
    sPendingDestroy.push_back(buffer);
}

bool ScreenBuffer::IsPendingDestroy(ScreenBuffer* buffer) {
    if (!buffer) return false;
    return std::find(sPendingDestroy.begin(), sPendingDestroy.end(), buffer) != sPendingDestroy.end();
}

void ScreenBuffer::CommitDestroy(Passkey<GameEngine>) {
    if (sPendingDestroy.empty()) return;

    // 重複があっても安全にする
    std::stable_sort(sPendingDestroy.begin(), sPendingDestroy.end());
    sPendingDestroy.erase(std::unique(sPendingDestroy.begin(), sPendingDestroy.end()), sPendingDestroy.end());

    for (auto* ptr : sPendingDestroy) {
        if (!ptr) continue;
        auto it = sBufferMap_.find(ptr);
        if (it == sBufferMap_.end()) continue;

        // recording 中に消すと危険なので、commit はゲームループ終端から呼ばれる前提
        // persistent pass 等は ScreenBuffer::Destroy() 内で DetachToRenderer される
        sBufferMap_.erase(it);
    }

    sPendingDestroy.clear();
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
        if (IsPendingDestroy(ptr)) continue;

        // ping-pong: 今フレームの write 面へ切り替え（read は前フレームを保持）
        ptr->AdvanceFrameBufferIndex();

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

bool ScreenBuffer::Initialize(std::uint32_t width, std::uint32_t height,
    DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat) {
    Destroy();

    width_ = width;
    height_ = height;
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

    writeIndex_ = 0;

    for (size_t i = 0; i < kBufferCount_; ++i) {
        renderTargets_[i] = std::make_unique<RenderTargetResource>(width_, height_, colorFormat_);
        depthStencils_[i] = std::make_unique<DepthStencilResource>(width_, height_, depthFormat_, 1.0f, static_cast<UINT8>(0), nullptr, true, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
        shaderResources_[i] = std::make_unique<ShaderResourceResource>(renderTargets_[i].get());
    }

    for (size_t i = 0; i < kBufferCount_; ++i) {
        if (!renderTargets_[i] || !depthStencils_[i] || !shaderResources_[i]) return false;
    }

    // 初回フレームで read 面が RT レイアウトのまま SRV バインドされるのを防ぐため、
    // 両面の RenderTarget を明示的に PixelShaderResource へ揃える。
    sDirectXCommon_->ExecuteOneShotCommandsForScreenBuffer(Passkey<ScreenBuffer>{},
        [this](ID3D12GraphicsCommandList* cl) {
            for (size_t i = 0; i < kBufferCount_; ++i) {
                if (!renderTargets_[i]) continue;
                renderTargets_[i]->SetCommandList(cl);
                renderTargets_[i]->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
        });

    return true;
}

void ScreenBuffer::Destroy() {
    DetachFromRenderer();

    for (auto& c : postEffectComponents_) {
        if (!c) continue;
        c->Finalize();
    }
    postEffectComponents_.clear();

    for (size_t i = 0; i < kBufferCount_; ++i) {
        shaderResources_[i].reset();
        depthStencils_[i].reset();
        renderTargets_[i].reset();
    }

    commandList_ = nullptr;
    commandAllocator_ = nullptr;

    if (sDirectXCommon_ && commandSlotIndex_ >= 0) {
        sDirectXCommon_->ReleaseScreenBufferCommandObjects(Passkey<ScreenBuffer>{}, commandSlotIndex_);
    }
    commandSlotIndex_ = -1;

    width_ = 0;
    height_ = 0;
}

ID3D12GraphicsCommandList* ScreenBuffer::BeginRecord() {
    if (!commandAllocator_ || !commandList_) return nullptr;

    HRESULT hr = commandAllocator_->Reset();
    if (FAILED(hr)) return nullptr;

    hr = commandList_->Reset(commandAllocator_, nullptr);
    if (FAILED(hr)) return nullptr;

    auto* rt = renderTargets_[GetWriteIndex()].get();
    auto* ds = depthStencils_[GetWriteIndex()].get();
    if (!rt || !ds) return nullptr;

    rt->SetCommandList(commandList_);
    ds->SetCommandList(commandList_);

    // 明示遷移（write 面のみ RT にする）
    if (!rt->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) {
        return nullptr;
    }
    if (!ds->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE)) {
        return nullptr;
    }

    const auto rtv = rt->GetCPUDescriptorHandle();
    const auto dsv = ds->GetCPUDescriptorHandle();

    commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    rt->ClearRenderTargetView();
    ds->ClearDepthStencilView();

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

    auto* rt = renderTargets_[GetWriteIndex()].get();
    auto* ds = depthStencils_[GetWriteIndex()].get();

    // write 面を SRV で読める状態へ（この面が次フレーム read になる）
    if (rt) {
        rt->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    if (ds->HasSrv()) {
        ds->TransitionToShaderResource();
    } else {
        ds->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_READ);
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
    static bool sShowViewer = false;

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
            const auto h = ptr->GetSrvHandle();
            if (h.ptr != 0) {
                ImGui::Text("0x%llX", static_cast<unsigned long long>(h.ptr));
            } else {
                ImGui::TextUnformatted("-");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(ptr);
            const bool isSel = (sSelected == ptr);
            if (ImGui::Selectable("##select", isSel, ImGuiSelectableFlags_SpanAllColumns)) {
                sSelected = ptr;
                sShowViewer = true;
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::Separator();

    if (sSelected && ScreenBuffer::IsExist(sSelected)) {
        ImGui::Text("Selected: %p", (void*)sSelected);
        ImGui::SameLine();
        if (ImGui::Button("Open Viewer")) {
            sShowViewer = true;
        }
    } else {
        ImGui::TextUnformatted("No ScreenBuffer selected or SRV not ready.");
        sShowViewer = false;
    }

    ImGui::End();

    if (!sShowViewer) return;

    if (!sSelected || !ScreenBuffer::IsExist(sSelected)) {
        sShowViewer = false;
        return;
    }

    if (ImGui::Begin("ScreenBuffer Viewer", &sShowViewer)) {
        const auto hdl = sSelected->GetSrvHandle();
        if (hdl.ptr != 0) {
            ImGui::Text("Ptr: %p", (void*)sSelected);
            ImGui::Text("Size: %ux%u", sSelected->GetWidth(), sSelected->GetHeight());
            ImGui::Separator();

            ImVec2 avail = ImGui::GetContentRegionAvail();
            const float w = static_cast<float>(sSelected->GetWidth());
            const float h = static_cast<float>(sSelected->GetHeight());

            ImVec2 drawSize = avail;
            if (w > 0.0f && h > 0.0f && avail.x > 0.0f && avail.y > 0.0f) {
                const float sx = avail.x / w;
                const float sy = avail.y / h;
                const float s = (sx < sy) ? sx : sy;
                drawSize = ImVec2(w * s, h * s);
            }

            ImGui::Image(ToImGuiTextureIdFromGpuHandle(hdl), drawSize);
        } else {
            ImGui::TextUnformatted("SRV not ready.");
        }
    }
    ImGui::End();
}
#endif

} // namespace KashipanEngine
