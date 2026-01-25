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

namespace {
// ScreenBuffer インスタンス管理用マップ
std::unordered_map<ScreenBuffer *, std::unique_ptr<ScreenBuffer>> sBufferMap{};
// Window と同様に「破棄要求→フレーム終端で実破棄」のための pending リスト
static std::vector<ScreenBuffer*> sPendingDestroy;
} // namespace

D3D12_GPU_DESCRIPTOR_HANDLE ScreenBuffer::GetSrvHandle() const noexcept {
    const auto idx = GetRtvReadIndex();
    return shaderResources_[idx] ? shaderResources_[idx]->GetGPUDescriptorHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

D3D12_GPU_DESCRIPTOR_HANDLE ScreenBuffer::GetDepthSrvHandle() const noexcept {
    const auto idx = GetDsvReadIndex();
    auto* ds = depthStencils_[idx].get();
    return (ds && ds->HasSrv()) ? ds->GetSrvGPUHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

std::vector<PostEffectPass> ScreenBuffer::BuildPostEffectPasses(Passkey<Renderer>) const {
    std::vector<PostEffectPass> out;
    out.reserve(postEffectComponents_.size());

    for (const auto& c : postEffectComponents_) {
        if (!c) continue;
        auto passes = c->BuildPostEffectPasses();
        if (passes.empty()) continue;
        for (auto& p : passes) {
            out.push_back(std::move(p));
        }
    }

    return out;
}

ScreenBuffer* ScreenBuffer::Create(std::uint32_t width, std::uint32_t height,
    DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat) {
    std::unique_ptr<ScreenBuffer> buffer(new ScreenBuffer());
    auto* raw = buffer.get();

    if (!raw->Initialize(width, height, colorFormat, depthFormat)) {
        return nullptr;
    }

    sBufferMap.emplace(raw, std::move(buffer));
    return raw;
}

void ScreenBuffer::AllDestroy(Passkey<GameEngine>) {
    sBufferMap.clear();
}

size_t ScreenBuffer::GetBufferCount() {
    return sBufferMap.size();
}

bool ScreenBuffer::IsExist(ScreenBuffer* buffer) {
    if (!buffer) return false;
    return sBufferMap.find(buffer) != sBufferMap.end();
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
        auto it = sBufferMap.find(ptr);
        if (it == sBufferMap.end()) continue;

        // recording 中に消すと危険なので、commit はゲームループ終端から呼ばれる前提
        // persistent pass 等は ScreenBuffer::Destroy() 内で DetachToRenderer される
        sBufferMap.erase(it);
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

void ScreenBuffer::MarkRecordingStarted(Passkey<Renderer>) {
    auto it = sRecordStates.find(this);
    if (it == sRecordStates.end()) {
        RecordState st;
        st.list = dx12Commands_ ? dx12Commands_->GetCommandList() : nullptr;
        st.discard = false;
        st.started = true;
        sRecordStates.emplace(this, st);
    } else {
        it->second.list = dx12Commands_ ? dx12Commands_->GetCommandList() : it->second.list;
        it->second.started = true;
    }
}

void ScreenBuffer::AllBeginRecord(Passkey<Renderer>) {
    sRecordStates.clear();
    sRecordStates.reserve(sBufferMap.size());

    for (auto& [ptr, owning] : sBufferMap) {
        if (!ptr || !owning) continue;
        if (IsPendingDestroy(ptr)) continue;

        RecordState st;
        st.list = ptr->dx12Commands_ ? ptr->dx12Commands_->BeginRecord() : nullptr;
        if (!st.list) {
            continue;
        }
        st.discard = false;
        st.started = false;
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

    return lists;
}

void ScreenBuffer::AllCloseRecord(Passkey<Renderer>) {
    for (auto& [ptr, st] : sRecordStates) {
        if (!ptr) continue;
        if (!st.started) continue;
        if (!ptr->dx12Commands_ || !ptr->dx12Commands_->EndRecord()) {
            continue;
        }
    }
    sRecordStates.clear();
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

    commandSlotIndex_ = sDirectXCommon_->AcquireCommandObjects(Passkey<ScreenBuffer>{});
    auto* cmd = sDirectXCommon_->GetCommandObjects(Passkey<ScreenBuffer>{}, commandSlotIndex_);
    if (!cmd || !cmd->GetCommandAllocator() || !cmd->GetCommandList()) {
        commandSlotIndex_ = -1;
        return false;
    }
    dx12Commands_ = cmd;

    rtvWriteIndex_ = 0;
    dsvWriteIndex_ = 0;
    isLastBeginDisableDepthWrite_ = false;

    for (size_t i = 0; i < kBufferCount_; ++i) {
        renderTargets_[i] = std::make_unique<RenderTargetResource>(width_, height_, colorFormat_);
        depthStencils_[i] = std::make_unique<DepthStencilResource>(width_, height_, depthFormat_, 1.0f, static_cast<UINT8>(0), nullptr, true, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
        shaderResources_[i] = std::make_unique<ShaderResourceResource>(renderTargets_[i].get());

        renderTargets_[i]->SetCommandList(cmd->GetCommandList());
        depthStencils_[i]->SetCommandList(cmd->GetCommandList());
    }

    for (size_t i = 0; i < kBufferCount_; ++i) {
        if (!renderTargets_[i] || !depthStencils_[i] || !shaderResources_[i]) return false;
    }

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
    dx12Commands_ = nullptr;

    if (sDirectXCommon_ && commandSlotIndex_ >= 0) {
        sDirectXCommon_->ReleaseCommandObjects(Passkey<ScreenBuffer>{}, commandSlotIndex_);
    }
    commandSlotIndex_ = -1;

    width_ = 0;
    height_ = 0;
}

ID3D12GraphicsCommandList* ScreenBuffer::BeginRecord(Passkey<Renderer>, bool disableDepthWrite) {
    auto* cmd = BeginRecord(disableDepthWrite);
    if (!cmd) return nullptr;

    auto &st = sRecordStates[this];
    st.list = cmd;
    st.started = true;

    return cmd;
}

ID3D12GraphicsCommandList* ScreenBuffer::BeginRecord(bool disableDepthWrite) {
    LogScope scope;
    if (!dx12Commands_) return nullptr;

    isLastBeginDisableDepthWrite_ = disableDepthWrite;

    auto *cmd = dx12Commands_->GetCommandList();
    auto* rt = renderTargets_[GetRtvWriteIndex()].get();
    auto* ds = depthStencils_[GetDsvWriteIndex()].get();
    if (!rt) return nullptr;
    if (!disableDepthWrite && !ds) return nullptr;

    // 初回のみRead面のバリアも設定
    if (isFirstBeginRecord_) {
        auto* rtRead = renderTargets_[GetRtvReadIndex()].get();
        auto *dsRead = depthStencils_[GetDsvReadIndex()].get();
        if (rtRead) rtRead->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        if (!disableDepthWrite && dsRead) dsRead->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_READ);
        isFirstBeginRecord_ = false;
    }

    if (!rt->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) {
        return nullptr;
    }
    if (!disableDepthWrite) {
        if (!ds->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE)) {
            return nullptr;
        }
    }

    const auto rtv = rt->GetCPUDescriptorHandle();

    if (disableDepthWrite) {
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        rt->ClearRenderTargetView();
    } else {
        const auto dsv = ds->GetCPUDescriptorHandle();
        cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        rt->ClearRenderTargetView();
        ds->ClearDepthStencilView();
    }

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

    auto* srvHeap = IGraphicsResource::GetSRVHeap(Passkey<ScreenBuffer>{});
    auto* samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<ScreenBuffer>{});
    if (srvHeap && samplerHeap) {
        ID3D12DescriptorHeap* ppHeaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
        cmd->SetDescriptorHeaps(2, ppHeaps);
    }

    return cmd;
}

bool ScreenBuffer::EndRecord(bool discard) {
    LogScope scope;
    if (!dx12Commands_) return false;

    auto *rt = renderTargets_[GetRtvWriteIndex()].get();
    auto *ds = depthStencils_[GetDsvWriteIndex()].get();

    if (rt) {
        rt->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    // BeginRecord で depth を触っていない場合は、ここでも触らない
    if (!isLastBeginDisableDepthWrite_ && ds) {
        if (ds->HasSrv()) {
            ds->TransitionToShaderResource();
        } else {
            ds->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_READ);
        }
    }

    (void)discard;
    const bool updateRtv = true;
    const bool updateDsv = !isLastBeginDisableDepthWrite_;
    AdvanceFrameBufferIndex(updateRtv, updateDsv);

    return true;
}

bool ScreenBuffer::EndRecord(Passkey<Renderer>, bool discard) {
    return EndRecord(discard);
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
    return true;
}

void ScreenBuffer::AttachToRenderer(const std::string& passName) {
    auto* renderer = sRenderer;
    if (!renderer) return;

    if (persistentScreenPassHandle_) {
        renderer->UnregisterPersistentScreenPass(persistentScreenPassHandle_);
        persistentScreenPassHandle_ = {};
    }

    auto passOpt = CreateScreenPass(passName);
    if (!passOpt) return;

    persistentScreenPassHandle_ = renderer->RegisterPersistentScreenPass(std::move(*passOpt));
}

void ScreenBuffer::DetachFromRenderer() {
    if (!persistentScreenPassHandle_) return;

    auto* renderer = sRenderer;
    if (renderer) {
        renderer->UnregisterPersistentScreenPass(persistentScreenPassHandle_);
    }
    persistentScreenPassHandle_ = {};
}

std::optional<ScreenBufferPass> ScreenBuffer::CreateScreenPass(const std::string& passName) {
    ScreenBufferPass pass(Passkey<ScreenBuffer>{});
    pass.screenBuffer = this;
    pass.passName = passName;

    pass.renderType = RenderType::Standard;
    pass.batchKey = 0;

    // 既存の ScreenBufferPass は「このScreenBufferに対するポストエフェクトチェーンの入口」として残す。
    // 実際の描画は Renderer 側で各 PostEffectPass を個別実行する。
    pass.batchedRenderFunction = [](ShaderVariableBinder&, std::uint32_t) -> bool {
        return true;
    };

    return pass;
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
    static bool sViewDepth = false;

    if (ImGui::BeginTable("##ScreenBufferList", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, 220))) {
        ImGui::TableSetupColumn("Ptr", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("SRV(Color)", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("SRV(Depth)", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Select");
        ImGui::TableHeadersRow();

        for (auto& kv : sBufferMap) {
            ScreenBuffer* ptr = kv.first;
            if (!ptr) continue;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%p", (void*)ptr);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%ux%u", ptr->GetWidth(), ptr->GetHeight());

            ImGui::TableSetColumnIndex(2);
            {
                const auto h = ptr->GetSrvHandle();
                if (h.ptr != 0) {
                    ImGui::Text("0x%llX", static_cast<unsigned long long>(h.ptr));
                } else {
                    ImGui::TextUnformatted("-");
                }
            }

            ImGui::TableSetColumnIndex(3);
            {
                const auto h = ptr->GetDepthSrvHandle();
                if (h.ptr != 0) {
                    ImGui::Text("0x%llX", static_cast<unsigned long long>(h.ptr));
                } else {
                    ImGui::TextUnformatted("-");
                }
            }

            ImGui::TableSetColumnIndex(4);
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
        const auto colorHdl = sSelected->GetSrvHandle();
        const auto depthHdl = sSelected->GetDepthSrvHandle();

        ImGui::Text("Ptr: %p", (void*)sSelected);
        ImGui::Text("Size: %ux%u", sSelected->GetWidth(), sSelected->GetHeight());
        ImGui::Separator();

        if (ImGui::CollapsingHeader("PostEffects", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto &effects = sSelected->GetPostEffectComponents();
            if (effects.empty()) {
                ImGui::TextUnformatted("(none)");
            } else {
                for (const auto &c : effects) {
                    if (!c) continue;

                    if (ImGui::TreeNode(c->GetComponentType().c_str())) {
                        c->ShowImGui();
                        ImGui::TreePop();
                    }
                }
            }
        }

        ImGui::Separator();

        const bool canViewColor = (colorHdl.ptr != 0);
        const bool canViewDepth = (depthHdl.ptr != 0);

        if (!canViewColor && !canViewDepth) {
            ImGui::TextUnformatted("SRV not ready.");
            ImGui::End();
            return;
        }

        // 表示タイプ切り替え
        if (canViewColor && canViewDepth) {
            const char* modeLabel = sViewDepth ? "Mode: Depth" : "Mode: Color";
            ImGui::TextUnformatted(modeLabel);
            ImGui::SameLine();
            if (ImGui::Button(sViewDepth ? "Show Color" : "Show Depth")) {
                sViewDepth = !sViewDepth;
            }
        } else if (canViewDepth && !canViewColor) {
            sViewDepth = true;
            ImGui::TextUnformatted("Mode: Depth");
        } else {
            sViewDepth = false;
            ImGui::TextUnformatted("Mode: Color");
        }

        const auto hdl = sViewDepth ? depthHdl : colorHdl;
        if (hdl.ptr != 0) {
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
