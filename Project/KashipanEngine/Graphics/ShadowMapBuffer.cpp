#include "ShadowMapBuffer.h"
#include "Core/DirectXCommon.h"
#include "Graphics/Resources/IGraphicsResource.h"
#include <algorithm>
#include <vector>
#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

<<<<<<< HEAD:Project/KashipanEngine/Graphics/ShadowMapBuffer.cpp
std::unordered_map<ShadowMapBuffer*, std::unique_ptr<ShadowMapBuffer>> ShadowMapBuffer::sBufferMap_{};

=======
>>>>>>> TD2_3:KashipanEngine/Graphics/ShadowMapBuffer.cpp
namespace {
struct RecordState {
    ID3D12GraphicsCommandList* list = nullptr;
    bool discard = false;
    bool started = false;
};

<<<<<<< HEAD:Project/KashipanEngine/Graphics/ShadowMapBuffer.cpp
static std::unordered_map<ShadowMapBuffer*, RecordState> sRecordStates;

// Window と同様に「破棄要求→フレーム終端で実破棄」のための pending リスト
static std::vector<ShadowMapBuffer*> sPendingDestroy;
=======
std::unordered_map<ShadowMapBuffer *, std::unique_ptr<ShadowMapBuffer>> sBufferMap{};
std::unordered_map<ShadowMapBuffer*, RecordState> sRecordStates;

// Window と同様に「破棄要求→フレーム終端で実破棄」のための pending リスト
std::vector<ShadowMapBuffer*> sPendingDestroy;
>>>>>>> TD2_3:KashipanEngine/Graphics/ShadowMapBuffer.cpp
} // namespace

D3D12_GPU_DESCRIPTOR_HANDLE ShadowMapBuffer::GetSrvHandle() const noexcept {
    return depth_ ? depth_->GetSrvGPUHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

ShadowMapBuffer* ShadowMapBuffer::Create(std::uint32_t width, std::uint32_t height, DXGI_FORMAT depthFormat, DXGI_FORMAT srvFormat) {
    std::unique_ptr<ShadowMapBuffer> buffer(new ShadowMapBuffer());
    auto* raw = buffer.get();

    if (!raw->Initialize(width, height, depthFormat, srvFormat)) {
        return nullptr;
    }

<<<<<<< HEAD:Project/KashipanEngine/Graphics/ShadowMapBuffer.cpp
    sBufferMap_.emplace(raw, std::move(buffer));
=======
    sBufferMap.emplace(raw, std::move(buffer));
>>>>>>> TD2_3:KashipanEngine/Graphics/ShadowMapBuffer.cpp
    return raw;
}

void ShadowMapBuffer::AllDestroy(Passkey<GameEngine>) {
<<<<<<< HEAD:Project/KashipanEngine/Graphics/ShadowMapBuffer.cpp
    sBufferMap_.clear();
}

size_t ShadowMapBuffer::GetBufferCount() {
    return sBufferMap_.size();
=======
    sBufferMap.clear();
}

size_t ShadowMapBuffer::GetBufferCount() {
    return sBufferMap.size();
>>>>>>> TD2_3:KashipanEngine/Graphics/ShadowMapBuffer.cpp
}

bool ShadowMapBuffer::IsExist(ShadowMapBuffer* buffer) {
    if (!buffer) return false;
<<<<<<< HEAD:Project/KashipanEngine/Graphics/ShadowMapBuffer.cpp
    return sBufferMap_.find(buffer) != sBufferMap_.end();
=======
    return sBufferMap.find(buffer) != sBufferMap.end();
>>>>>>> TD2_3:KashipanEngine/Graphics/ShadowMapBuffer.cpp
}

void ShadowMapBuffer::DestroyNotify(ShadowMapBuffer* buffer) {
    if (!buffer) return;
    if (!IsExist(buffer)) return;
    if (IsPendingDestroy(buffer)) return;
    sPendingDestroy.push_back(buffer);
}

bool ShadowMapBuffer::IsPendingDestroy(ShadowMapBuffer* buffer) {
    if (!buffer) return false;
    return std::find(sPendingDestroy.begin(), sPendingDestroy.end(), buffer) != sPendingDestroy.end();
}

void ShadowMapBuffer::CommitDestroy(Passkey<GameEngine>) {
    if (sPendingDestroy.empty()) return;

    std::stable_sort(sPendingDestroy.begin(), sPendingDestroy.end());
    sPendingDestroy.erase(std::unique(sPendingDestroy.begin(), sPendingDestroy.end()), sPendingDestroy.end());

    for (auto* ptr : sPendingDestroy) {
        if (!ptr) continue;
<<<<<<< HEAD:Project/KashipanEngine/Graphics/ShadowMapBuffer.cpp
        auto it = sBufferMap_.find(ptr);
        if (it == sBufferMap_.end()) continue;
        sBufferMap_.erase(it);
=======
        auto it = sBufferMap.find(ptr);
        if (it == sBufferMap.end()) continue;
        sBufferMap.erase(it);
>>>>>>> TD2_3:KashipanEngine/Graphics/ShadowMapBuffer.cpp
    }

    sPendingDestroy.clear();
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

<<<<<<< HEAD:Project/KashipanEngine/Graphics/ShadowMapBuffer.cpp
        for (auto &kv : sBufferMap_) {
=======
        for (auto &kv : sBufferMap) {
>>>>>>> TD2_3:KashipanEngine/Graphics/ShadowMapBuffer.cpp
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
<<<<<<< HEAD:Project/KashipanEngine/Graphics/ShadowMapBuffer.cpp
    sRecordStates.reserve(sBufferMap_.size());

    for (auto& [ptr, owning] : sBufferMap_) {
=======
    sRecordStates.reserve(sBufferMap.size());

    for (auto& [ptr, owning] : sBufferMap) {
>>>>>>> TD2_3:KashipanEngine/Graphics/ShadowMapBuffer.cpp
        if (!ptr || !owning) continue;
        if (IsPendingDestroy(ptr)) continue;

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

        // Renderer の処理が終わるタイミングで Close
        if (!ptr->dx12Commands_ || !ptr->dx12Commands_->EndRecord()) {
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

    commandSlotIndex_ = sDirectXCommon_->AcquireCommandObjects(Passkey<ShadowMapBuffer>{});
    auto* cmd = sDirectXCommon_->GetCommandObjects(Passkey<ShadowMapBuffer>{}, commandSlotIndex_);
    if (!cmd || !cmd->GetCommandAllocator() || !cmd->GetCommandList()) {
        commandSlotIndex_ = -1;
        return false;
    }
    dx12Commands_ = cmd;

    depth_ = std::make_unique<DepthStencilResource>(width_, height_, depthFormat_, 1.0f, static_cast<UINT8>(0), nullptr, true, srvFormat_);
    if (!depth_ || !depth_->HasSrv()) return false;

    return true;
}

void ShadowMapBuffer::Destroy() {
    depth_.reset();

    dx12Commands_ = nullptr;

    if (sDirectXCommon_ && commandSlotIndex_ >= 0) {
        sDirectXCommon_->ReleaseCommandObjects(Passkey<ShadowMapBuffer>{}, commandSlotIndex_);
    }
    commandSlotIndex_ = -1;

    width_ = 0;
    height_ = 0;
    depthFormat_ = DXGI_FORMAT_UNKNOWN;
    srvFormat_ = DXGI_FORMAT_UNKNOWN;
}

ID3D12GraphicsCommandList* ShadowMapBuffer::BeginRecord() {
    if (!dx12Commands_) return nullptr;
    if (!depth_) return nullptr;

    auto *cmd = dx12Commands_->BeginRecord();
    if (!cmd) return nullptr;

    depth_->SetCommandList(cmd);

    if (!depth_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE)) {
        return nullptr;
    }

    const auto dsv = depth_->GetCPUDescriptorHandle();
    cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
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

    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    auto* srvHeap = IGraphicsResource::GetSRVHeap(Passkey<ShadowMapBuffer>{});
    auto* samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<ShadowMapBuffer>{});
    if (srvHeap && samplerHeap) {
        ID3D12DescriptorHeap* ppHeaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
        cmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    }

    return cmd;
}

bool ShadowMapBuffer::EndRecord(bool discard) {
    if (!dx12Commands_) return false;

    if (depth_) {
        // ImGui などで SRV として参照されるため、シェーダーからサンプル可能な状態へ遷移
        if (depth_->HasSrv()) {
            depth_->TransitionToShaderResource();
        } else {
            depth_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_READ);
        }
    }

    // NOTE: Close は Renderer 側でフレーム終端にまとめて行う
    (void)discard;
    return true;
}

} // namespace KashipanEngine
