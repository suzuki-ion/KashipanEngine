#include "ShadowMapBuffer.h"
#include "Core/DirectXCommon.h"
#include "Graphics/Resources/IGraphicsResource.h"
#include <imgui.h>

namespace KashipanEngine {

std::unordered_map<ShadowMapBuffer*, std::unique_ptr<ShadowMapBuffer>> ShadowMapBuffer::sBufferMap_{};

namespace {
struct RecordState {
    ID3D12GraphicsCommandList* list = nullptr;
    bool discard = false;
    bool started = false;
};

static std::unordered_map<ShadowMapBuffer*, RecordState> sRecordStates;
}

D3D12_GPU_DESCRIPTOR_HANDLE ShadowMapBuffer::GetSrvHandle() const noexcept {
    return depth_ ? depth_->GetSrvGPUHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

ShadowMapBuffer* ShadowMapBuffer::Create(std::uint32_t width, std::uint32_t height, DXGI_FORMAT depthFormat, DXGI_FORMAT srvFormat) {
    std::unique_ptr<ShadowMapBuffer> buffer(new ShadowMapBuffer());
    auto* raw = buffer.get();

    if (!raw->Initialize(width, height, depthFormat, srvFormat)) {
        return nullptr;
    }

    sBufferMap_.emplace(raw, std::move(buffer));
    return raw;
}

void ShadowMapBuffer::AllDestroy(Passkey<GameEngine>) {
    sBufferMap_.clear();
}

size_t ShadowMapBuffer::GetBufferCount() {
    return sBufferMap_.size();
}

bool ShadowMapBuffer::IsExist(ShadowMapBuffer* buffer) {
    if (!buffer) return false;
    return sBufferMap_.find(buffer) != sBufferMap_.end();
}

bool ShadowMapBuffer::IsRecording(Passkey<Renderer>) const noexcept {
    auto it = sRecordStates.find(const_cast<ShadowMapBuffer*>(this));
    if (it == sRecordStates.end()) return false;
    return it->second.started;
}

#if defined(USE_IMGUI)
namespace {
ImTextureID ToImGuiTextureIdFromGpuHandle(D3D12_GPU_DESCRIPTOR_HANDLE h) {
    return (ImTextureID)(uintptr_t)h.ptr;
}
}

void ShadowMapBuffer::ShowImGuiShadowMapBuffersWindow() {
    if (!ImGui::Begin("ShadowMapBuffer - Buffers")) {
        ImGui::End();
        return;
    }

    ImGui::Text("ShadowMapBuffers: %d", static_cast<int>(ShadowMapBuffer::GetBufferCount()));

    static ShadowMapBuffer *sSelected = nullptr;
    static bool sShowViewer = false;

    if (ImGui::BeginTable("##ShadowMapBufferList", 4,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
        ImVec2(0, 220))) {
        ImGui::TableSetupColumn("Ptr", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("SRV", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Select");
        ImGui::TableHeadersRow();

        for (auto &kv : sBufferMap_) {
            ShadowMapBuffer *ptr = kv.first;
            if (!ptr) continue;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%p", (void *)ptr);

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

    if (sSelected && ShadowMapBuffer::IsExist(sSelected)) {
        ImGui::Text("Selected: %p", (void *)sSelected);
        ImGui::SameLine();
        if (ImGui::Button("Open Viewer")) {
            sShowViewer = true;
        }
    } else {
        ImGui::TextUnformatted("No ShadowMapBuffer selected or SRV not ready.");
        sShowViewer = false;
    }

    ImGui::End();

    if (!sShowViewer) return;

    if (!sSelected || !ShadowMapBuffer::IsExist(sSelected)) {
        sShowViewer = false;
        return;
    }

    if (ImGui::Begin("ShadowMapBuffer Viewer", &sShowViewer)) {
        const auto hdl = sSelected->GetSrvHandle();
        if (hdl.ptr != 0) {
            ImGui::Text("Ptr: %p", (void *)sSelected);
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

void ShadowMapBuffer::AllBeginRecord(Passkey<Renderer>) {
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

std::vector<ID3D12CommandList*> ShadowMapBuffer::AllEndRecord(Passkey<Renderer>) {
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

ShadowMapBuffer::~ShadowMapBuffer() {
    Destroy();
}

bool ShadowMapBuffer::Initialize(std::uint32_t width, std::uint32_t height, DXGI_FORMAT depthFormat, DXGI_FORMAT srvFormat) {
    Destroy();

    width_ = width;
    height_ = height;
    depthFormat_ = depthFormat;
    srvFormat_ = srvFormat;

    if (!sDirectXCommon_) return false;

    commandSlotIndex_ = sDirectXCommon_->AcquireShadowMapBufferCommandObjects(Passkey<ShadowMapBuffer>{});
    auto* cmd = sDirectXCommon_->GetShadowMapBufferCommandObjects(Passkey<ShadowMapBuffer>{}, commandSlotIndex_);
    if (!cmd || !cmd->commandAllocator || !cmd->commandList) {
        commandSlotIndex_ = -1;
        return false;
    }

    commandAllocator_ = cmd->commandAllocator.Get();
    commandList_ = cmd->commandList.Get();

    depth_ = std::make_unique<DepthStencilResource>(width_, height_, depthFormat_, 1.0f, static_cast<UINT8>(0), nullptr, true, srvFormat_);
    if (!depth_ || !depth_->HasSrv()) return false;

    return true;
}

void ShadowMapBuffer::Destroy() {
    depth_.reset();

    commandList_ = nullptr;
    commandAllocator_ = nullptr;

    if (sDirectXCommon_ && commandSlotIndex_ >= 0) {
        sDirectXCommon_->ReleaseShadowMapBufferCommandObjects(Passkey<ShadowMapBuffer>{}, commandSlotIndex_);
    }
    commandSlotIndex_ = -1;

    width_ = 0;
    height_ = 0;
    depthFormat_ = DXGI_FORMAT_UNKNOWN;
    srvFormat_ = DXGI_FORMAT_UNKNOWN;
}

ID3D12GraphicsCommandList* ShadowMapBuffer::BeginRecord() {
    if (!commandAllocator_ || !commandList_) return nullptr;

    HRESULT hr = commandAllocator_->Reset();
    if (FAILED(hr)) return nullptr;

    hr = commandList_->Reset(commandAllocator_, nullptr);
    if (FAILED(hr)) return nullptr;

    if (!depth_) return nullptr;

    depth_->SetCommandList(commandList_);

    if (!depth_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE)) {
        return nullptr;
    }

    const auto dsv = depth_->GetCPUDescriptorHandle();
    commandList_->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
    depth_->ClearDepthStencilView();

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

    auto* srvHeap = IGraphicsResource::GetSRVHeap(Passkey<ShadowMapBuffer>{});
    auto* samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<ShadowMapBuffer>{});
    if (srvHeap && samplerHeap) {
        ID3D12DescriptorHeap* ppHeaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
        commandList_->SetDescriptorHeaps(2, ppHeaps);
    }

    return commandList_;
}

bool ShadowMapBuffer::EndRecord(bool discard) {
    if (!commandList_) return false;

    if (depth_) {
        // ImGui などで SRV として参照されるため、シェーダーからサンプル可能な状態へ遷移
        if (depth_->HasSrv()) {
            depth_->TransitionToShaderResource();
        } else {
            depth_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_READ);
        }
    }

    HRESULT hr = commandList_->Close();
    if (FAILED(hr)) return false;

    (void)discard;
    return true;
}

} // namespace KashipanEngine
