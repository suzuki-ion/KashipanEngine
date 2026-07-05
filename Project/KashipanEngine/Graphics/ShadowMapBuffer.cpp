#include "ShadowMapBuffer.h"
#include "Core/DirectXCommon.h"
#include "Graphics/Resources/IGraphicsResource.h"
#include "Assets/TextureManager.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace KashipanEngine {

namespace {
// ShadowMapBuffer インスタンス管理用マップ
std::unordered_map<ShadowMapBuffer *, std::unique_ptr<ShadowMapBuffer>> sBufferMap{};
// 「破棄要求→フレーム終端で実破棄」のための pending リスト
std::vector<const ShadowMapBuffer *> sPendingDestroy;
// 自動命名用カウンタ
std::uint32_t sAutoNameCounter = 0;
} // namespace

D3D12_GPU_DESCRIPTOR_HANDLE ShadowMapBuffer::GetSrvHandle() const noexcept {
    return depth_ ? depth_->GetSrvGPUHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

ShadowMapBuffer *ShadowMapBuffer::Create(std::uint32_t width, std::uint32_t height, const std::string &name, DXGI_FORMAT depthFormat, DXGI_FORMAT srvFormat) {
    std::unique_ptr<ShadowMapBuffer> buffer(new ShadowMapBuffer());
    auto *raw = buffer.get();

    if (!raw->Initialize(width, height, depthFormat, srvFormat)) {
        return nullptr;
    }

    raw->RegisterToTextureManager(name);

    sBufferMap.emplace(raw, std::move(buffer));
    return raw;
}

void ShadowMapBuffer::SetRenderTargetName(const std::string &name) {
    if (name.empty() || name == name_) return;
    UnregisterFromTextureManager();
    RegisterToTextureManager(name);
}

void ShadowMapBuffer::RegisterToTextureManager(const std::string &name) {
    // 名前が空の場合は自動生成する
    std::string registerName = name;
    if (registerName.empty()) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "ShadowMapBuffer_%u", sAutoNameCounter++);
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

void ShadowMapBuffer::UnregisterFromTextureManager() {
    if (textureHandle_ != TextureManager::kInvalidHandle) {
        TextureManager::UnregisterExternalTexture(textureHandle_);
        textureHandle_ = TextureManager::kInvalidHandle;
    }
}

void ShadowMapBuffer::AllDestroy(Passkey<GameEngine>) {
    sBufferMap.clear();
    sPendingDestroy.clear();
}

bool ShadowMapBuffer::IsExist(ShadowMapBuffer *buffer) {
    if (!buffer) return false;
    return sBufferMap.find(buffer) != sBufferMap.end();
}

void ShadowMapBuffer::DestroyNotify() const {
    if (!IsExist(const_cast<ShadowMapBuffer *>(this))) return;
    if (IsPendingDestroy()) return;
    sPendingDestroy.push_back(this);
}

bool ShadowMapBuffer::IsPendingDestroy() const {
    return std::find(sPendingDestroy.begin(), sPendingDestroy.end(), this) != sPendingDestroy.end();
}

void ShadowMapBuffer::CommitDestroy(Passkey<GameEngine>) {
    if (sPendingDestroy.empty()) return;

    std::stable_sort(sPendingDestroy.begin(), sPendingDestroy.end());
    sPendingDestroy.erase(std::unique(sPendingDestroy.begin(), sPendingDestroy.end()), sPendingDestroy.end());

    for (const auto *ptr : sPendingDestroy) {
        if (!ptr) continue;
        auto it = sBufferMap.find(const_cast<ShadowMapBuffer *>(ptr));
        if (it == sBufferMap.end()) continue;
        sBufferMap.erase(it);
    }

    sPendingDestroy.clear();
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
    auto *cmd = sDirectXCommon_->GetCommandObjects(Passkey<ShadowMapBuffer>{}, commandSlotIndex_);
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
    UnregisterFromTextureManager();
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

void ShadowMapBuffer::BeginDraw() {
    BeginRecord();
}

void ShadowMapBuffer::EndDraw() {
    if (!EndRecord()) return;

    // コマンドリストを閉じてフレーム終端実行用に登録する
    if (!dx12Commands_ || !dx12Commands_->EndRecord()) return;
    if (sDirectXCommon_) {
        sDirectXCommon_->AddRecordCommandList(Passkey<ShadowMapBuffer>{}, dx12Commands_->GetCommandList());
    }
}

ID3D12GraphicsCommandList *ShadowMapBuffer::BeginRecord() {
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

    auto *srvHeap = IGraphicsResource::GetSRVHeap(Passkey<ShadowMapBuffer>{});
    auto *samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<ShadowMapBuffer>{});
    if (srvHeap && samplerHeap) {
        ID3D12DescriptorHeap *ppHeaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
        cmd->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    }

    return cmd;
}

bool ShadowMapBuffer::EndRecord() {
    if (!dx12Commands_) return false;

    if (depth_) {
        // シェーダーからサンプル可能な状態へ遷移
        if (depth_->HasSrv()) {
            depth_->TransitionToShaderResource();
        } else {
            depth_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_READ);
        }
    }

    return true;
}

} // namespace KashipanEngine
