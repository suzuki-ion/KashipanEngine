#include "ScreenBuffer.h"
#include "Core/DirectXCommon.h"
#include "Graphics/Resources/IGraphicsResource.h"
#include "Assets/TextureManager.h"
#include "Debug/Logger.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace KashipanEngine {

namespace {
// ScreenBuffer インスタンス管理用マップ
std::unordered_map<ScreenBuffer *, std::unique_ptr<ScreenBuffer>> sBufferMap{};
// 「破棄要求→フレーム終端で実破棄」のための pending リスト
std::vector<const ScreenBuffer *> sPendingDestroy;
// 自動命名用カウンタ
std::uint32_t sAutoNameCounter = 0;
} // namespace

D3D12_GPU_DESCRIPTOR_HANDLE ScreenBuffer::GetSrvHandle() const noexcept {
    const auto idx = GetRtvReadIndex();
    return shaderResources_[idx] ? shaderResources_[idx]->GetGPUDescriptorHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

D3D12_GPU_DESCRIPTOR_HANDLE ScreenBuffer::GetDepthSrvHandle() const noexcept {
    const auto idx = GetDsvReadIndex();
    auto *ds = depthStencils_[idx].get();
    return (ds && ds->HasSrv()) ? ds->GetSrvGPUHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

ScreenBuffer *ScreenBuffer::Create(std::uint32_t width, std::uint32_t height,
    const std::string &name, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat) {
    std::unique_ptr<ScreenBuffer> buffer(new ScreenBuffer());
    auto *raw = buffer.get();

    if (!raw->Initialize(width, height, colorFormat, depthFormat)) {
        return nullptr;
    }

    raw->RegisterToTextureManager(name);

    sBufferMap.emplace(raw, std::move(buffer));
    return raw;
}

void ScreenBuffer::SetRenderTargetName(const std::string &name) {
    if (name.empty() || name == name_) return;
    UnregisterFromTextureManager();
    RegisterToTextureManager(name);
}

void ScreenBuffer::RegisterToTextureManager(const std::string &name) {
    // 名前が空の場合は自動生成する
    std::string registerName = name;
    if (registerName.empty()) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "ScreenBuffer_%u", sAutoNameCounter++);
        registerName = buf;
    }
    // 同名衝突時はサフィックスを付けて再試行する
    textureHandle_ = TextureManager::RegisterExternalTexture(registerName, this);
    std::uint32_t suffix = 1;
    while (textureHandle_ == TextureManager::kInvalidHandle && suffix < 1000) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "_%u", suffix++);
        textureHandle_ = TextureManager::RegisterExternalTexture(registerName + buf, this);
        if (textureHandle_ != TextureManager::kInvalidHandle) {
            registerName += buf;
        }
    }
    name_ = registerName;
}

void ScreenBuffer::UnregisterFromTextureManager() {
    if (textureHandle_ != TextureManager::kInvalidHandle) {
        TextureManager::UnregisterExternalTexture(textureHandle_);
        textureHandle_ = TextureManager::kInvalidHandle;
    }
}

void ScreenBuffer::AllDestroy(Passkey<GameEngine>) {
    sBufferMap.clear();
    sPendingDestroy.clear();
}

bool ScreenBuffer::IsExist(ScreenBuffer *buffer) {
    if (!buffer) return false;
    return sBufferMap.find(buffer) != sBufferMap.end();
}

void ScreenBuffer::DestroyNotify() const {
    if (!IsExist(const_cast<ScreenBuffer *>(this))) return;
    if (IsPendingDestroy()) return;
    sPendingDestroy.push_back(this);
}

bool ScreenBuffer::IsPendingDestroy() const {
    return std::find(sPendingDestroy.begin(), sPendingDestroy.end(), this) != sPendingDestroy.end();
}

void ScreenBuffer::CommitDestroy(Passkey<GameEngine>) {
    if (sPendingDestroy.empty()) return;

    // 重複があっても安全にする
    std::stable_sort(sPendingDestroy.begin(), sPendingDestroy.end());
    sPendingDestroy.erase(std::unique(sPendingDestroy.begin(), sPendingDestroy.end()), sPendingDestroy.end());

    for (const auto *ptr : sPendingDestroy) {
        if (!ptr) continue;
        auto it = sBufferMap.find(const_cast<ScreenBuffer *>(ptr));
        if (it == sBufferMap.end()) continue;
        sBufferMap.erase(it);
    }

    sPendingDestroy.clear();
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
    auto *cmd = sDirectXCommon_->GetCommandObjects(Passkey<ScreenBuffer>{}, commandSlotIndex_);
    if (!cmd || !cmd->GetCommandAllocator() || !cmd->GetCommandList()) {
        commandSlotIndex_ = -1;
        return false;
    }
    dx12Commands_ = cmd;

    rtvWriteIndex_ = 0;
    dsvWriteIndex_ = 0;
    isLastBeginDisableDepthWrite_ = false;
    isFirstBeginRecord_ = true;

    for (size_t i = 0; i < kBufferCount; ++i) {
        renderTargets_[i] = std::make_unique<RenderTargetResource>(width_, height_, colorFormat_);
        depthStencils_[i] = std::make_unique<DepthStencilResource>(width_, height_, depthFormat_, 1.0f, static_cast<UINT8>(0), nullptr, true, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
        shaderResources_[i] = std::make_unique<ShaderResourceResource>(renderTargets_[i].get());

        renderTargets_[i]->SetCommandList(cmd->GetCommandList());
        depthStencils_[i]->SetCommandList(cmd->GetCommandList());
    }

    for (size_t i = 0; i < kBufferCount; ++i) {
        if (!renderTargets_[i] || !depthStencils_[i] || !shaderResources_[i]) return false;
    }

    return true;
}

void ScreenBuffer::Destroy() {
    UnregisterFromTextureManager();
    for (size_t i = 0; i < kBufferCount; ++i) {
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

void ScreenBuffer::BeginDraw() {
    BeginRecord(!isDepthWriteEnabled_);
}

void ScreenBuffer::NextPass() {
    if (!EndRecord()) return;
    BeginRecord(true);
}

void ScreenBuffer::EndDraw() {
    if (!EndRecord()) return;

    // コマンドリストを閉じてフレーム終端実行用に登録する
    if (!dx12Commands_ || !dx12Commands_->EndRecord()) return;
    if (sDirectXCommon_) {
        sDirectXCommon_->AddRecordCommandList(Passkey<ScreenBuffer>{}, dx12Commands_->GetCommandList());
    }
}

ID3D12GraphicsCommandList *ScreenBuffer::BeginRecord(bool disableDepthWrite) {
    LogScope scope;
    if (!dx12Commands_) return nullptr;

    // 既に記録中なら継続、未記録なら記録開始
    auto *cmd = dx12Commands_->BeginRecord();
    if (!cmd) return nullptr;

    isLastBeginDisableDepthWrite_ = disableDepthWrite;

    auto *rt = renderTargets_[GetRtvWriteIndex()].get();
    auto *ds = depthStencils_[GetDsvWriteIndex()].get();
    if (!rt) return nullptr;
    if (!disableDepthWrite && !ds) return nullptr;

    // 初回のみRead面のバリアも設定
    if (isFirstBeginRecord_) {
        auto *rtRead = renderTargets_[GetRtvReadIndex()].get();
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

    auto *srvHeap = IGraphicsResource::GetSRVHeap(Passkey<ScreenBuffer>{});
    auto *samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<ScreenBuffer>{});
    if (srvHeap && samplerHeap) {
        ID3D12DescriptorHeap *ppHeaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
        cmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
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

} // namespace KashipanEngine
