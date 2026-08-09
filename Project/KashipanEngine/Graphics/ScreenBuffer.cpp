#include "ScreenBuffer.h"
#include "Core/DirectXCommon.h"
#include "Graphics/ImageExporter.h"
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

bool ScreenBuffer::SaveToFile(const std::string &filePath) const {
    if (!sDirectXCommon_) return false;
    auto *resource = previewTarget_ ? previewTarget_->GetResource() : nullptr;
    if (!resource) return false;

    auto *commandQueue = sDirectXCommon_->GetCommandQueueForScreenBuffer(Passkey<ScreenBuffer>{});
    return ImageExporter::SaveTextureToFile(commandQueue, resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, filePath);
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

void ScreenBuffer::DestroyNotify() {
    if (!IsExist(const_cast<ScreenBuffer *>(this))) return;
    if (IsPendingDestroy()) return;
    UnregisterFromTextureManager();
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
        // プレビュー安定バッファへのコピー元として使うため（CopyToPreviewTarget参照）
        renderTargets_[i]->AddTransitionState(D3D12_RESOURCE_STATE_COPY_SOURCE);
        depthStencils_[i] = std::make_unique<DepthStencilResource>(width_, height_, depthFormat_, 1.0f, static_cast<UINT8>(0), nullptr, true, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
        shaderResources_[i] = std::make_unique<ShaderResourceResource>(renderTargets_[i].get());

        renderTargets_[i]->SetCommandList(cmd->GetCommandList());
        depthStencils_[i]->SetCommandList(cmd->GetCommandList());
    }
    rtBufferWidth_ = width_;
    rtBufferHeight_ = height_;
    dsBufferWidth_ = width_;
    dsBufferHeight_ = height_;

    for (size_t i = 0; i < kBufferCount; ++i) {
        if (!renderTargets_[i] || !depthStencils_[i] || !shaderResources_[i]) return false;
    }

    previewTarget_ = std::make_unique<RenderTargetResource>(width_, height_, colorFormat_);
    previewTarget_->AddTransitionState(D3D12_RESOURCE_STATE_COPY_DEST);
    previewShaderResource_ = std::make_unique<ShaderResourceResource>(previewTarget_.get());
    previewTarget_->SetCommandList(cmd->GetCommandList());
    previewBufferWidth_ = width_;
    previewBufferHeight_ = height_;
    if (!previewTarget_ || !previewShaderResource_) return false;

    return true;
}

bool ScreenBuffer::Resize(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0) return false;
    if (width == width_ && height == height_) return true;
    if (!dx12Commands_) return false;

    // ここではサイズを記憶するだけで、GPUリソースの再生成は行わない。
    // 実際の再生成は、各バッファがBeginRecordで実際に使用されるタイミングで
    // EnsureRenderTargetSize/EnsureDepthStencilSizeにより1バッファずつ行われる。
    width_ = width;
    height_ = height;

    return true;
}

void ScreenBuffer::EnsureRenderTargetSize(ID3D12GraphicsCommandList *cmd, bool isMainCall) {
    if (width_ == 0 || height_ == 0) return;
    if (rtBufferWidth_ == width_ && rtBufferHeight_ == height_) return;
    // NextPass経由（isMainCall=false）の作り直しは行わない（EnsureRenderTargetSizeのヘッダーコメント参照）。
    // BeginDraw経由（isMainCall=true）のみが作り直しを行える
    if (!isMainCall) return;

    for (size_t i = 0; i < kBufferCount; ++i) {
        renderTargets_[i] = std::make_unique<RenderTargetResource>(width_, height_, colorFormat_);
        // プレビュー安定バッファへのコピー元として使うため（CopyToPreviewTarget参照）
        renderTargets_[i]->AddTransitionState(D3D12_RESOURCE_STATE_COPY_SOURCE);
        shaderResources_[i] = std::make_unique<ShaderResourceResource>(renderTargets_[i].get());
        if (cmd) {
            renderTargets_[i]->SetCommandList(cmd);
        }
    }

    rtBufferWidth_ = width_;
    rtBufferHeight_ = height_;
}

void ScreenBuffer::EnsureDepthStencilSize(ID3D12GraphicsCommandList *cmd) {
    if (width_ == 0 || height_ == 0) return;
    if (dsBufferWidth_ == width_ && dsBufferHeight_ == height_) return;

    for (size_t i = 0; i < kBufferCount; ++i) {
        depthStencils_[i] = std::make_unique<DepthStencilResource>(width_, height_, depthFormat_, 1.0f, static_cast<UINT8>(0), nullptr, true, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
        if (cmd) {
            depthStencils_[i]->SetCommandList(cmd);
        }
    }

    dsBufferWidth_ = width_;
    dsBufferHeight_ = height_;
}

void ScreenBuffer::Destroy() {
    for (size_t i = 0; i < kBufferCount; ++i) {
        shaderResources_[i].reset();
        depthStencils_[i].reset();
        renderTargets_[i].reset();
    }
    rtBufferWidth_ = 0;
    rtBufferHeight_ = 0;
    dsBufferWidth_ = 0;
    dsBufferHeight_ = 0;

    previewShaderResource_.reset();
    previewTarget_.reset();
    previewShaderResourcePendingDestroy_.reset();
    previewTargetPendingDestroy_.reset();
    previewBufferWidth_ = 0;
    previewBufferHeight_ = 0;

    dx12Commands_ = nullptr;

    if (sDirectXCommon_ && commandSlotIndex_ >= 0) {
        sDirectXCommon_->ReleaseCommandObjects(Passkey<ScreenBuffer>{}, commandSlotIndex_);
    }
    commandSlotIndex_ = -1;

    width_ = 0;
    height_ = 0;
}

void ScreenBuffer::BeginDraw() {
    BeginRecord(!isDepthWriteEnabled_, /*isMainCall=*/true);
}

void ScreenBuffer::NextPass() {
    if (!EndRecord()) return;
    BeginRecord(true, /*isMainCall=*/false);
}

void ScreenBuffer::RebindWriteTarget() {
    if (!dx12Commands_) return;
    auto *cmd = dx12Commands_->GetCommandList();
    auto *rt = renderTargets_[GetRtvWriteIndex()].get();
    if (!cmd || !rt) return;

    rt->SetCommandList(cmd);
    if (!rt->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) return;

    const auto rtv = rt->GetCPUDescriptorHandle();
    cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

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
}

void ScreenBuffer::EndDraw() {
    if (!EndRecord()) return;

    // このフレームの最終結果をプレビュー安定バッファへコピーする（コマンドリストがまだ
    // 記録中のこの時点で行う必要がある。詳細はCopyToPreviewTargetのヘッダーコメント参照）
    if (dx12Commands_) {
        CopyToPreviewTarget(dx12Commands_->GetCommandList());
    }

    // コマンドリストを閉じてフレーム終端実行用に登録する
    if (!dx12Commands_ || !dx12Commands_->EndRecord()) return;
    if (sDirectXCommon_) {
        sDirectXCommon_->AddRecordCommandList(Passkey<ScreenBuffer>{}, dx12Commands_->GetCommandList());
    }
}

ID3D12GraphicsCommandList *ScreenBuffer::BeginRecord(bool disableDepthWrite, bool isMainCall) {
    LogScope scope;
    if (!dx12Commands_) return nullptr;

    // 既に記録中なら継続、未記録なら記録開始
    auto *cmd = dx12Commands_->BeginRecord();
    if (!cmd) return nullptr;

    isLastBeginDisableDepthWrite_ = disableDepthWrite;

    // Resize()で要求されたサイズに対して、ダブルバッファの両方を必要に応じて作り直す
    // （作り直し可否判定はEnsureRenderTargetSizeのコメント参照）
    EnsureRenderTargetSize(cmd, isMainCall);
    if (!disableDepthWrite) {
        EnsureDepthStencilSize(cmd);
    }

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

    // EnsureRenderTargetSize/EnsureDepthStencilSizeはisMainCall=trueの時点でダブルバッファの
    // 両方を常にwidth_/height_へ揃えるため、ここでは直接width_/height_を使ってよい
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

void ScreenBuffer::CopyToPreviewTarget(ID3D12GraphicsCommandList *cmd) {
    if (!cmd) return;
    if (width_ == 0 || height_ == 0) return;

    // EndRecord()の直後のためGetRtvReadIndex()の面は既にPIXEL_SHADER_RESOURCE状態
    auto *src = renderTargets_[GetRtvReadIndex()].get();
    if (!src) return;

    // プレビュー用の安定バッファを、現在の要求サイズに合わせて必要なら作り直す。
    // 古い方は即座に破棄せず1フレーム遅延させて破棄する（previewTargetPendingDestroy_のコメント参照）
    if (previewBufferWidth_ != width_ || previewBufferHeight_ != height_) {
        // 前回のリサイズで退避した分は、今回のCopyToPreviewTarget呼び出し時点で
        // 既に1フレーム分のGPU同期を経ているため、ここで安全に破棄できる
        // （このstd::moveへの代入により、それまでpreviewTargetPendingDestroy_に入っていた
        // ものが破棄される）
        previewTargetPendingDestroy_ = std::move(previewTarget_);
        previewShaderResourcePendingDestroy_ = std::move(previewShaderResource_);

        previewTarget_ = std::make_unique<RenderTargetResource>(width_, height_, colorFormat_);
        previewTarget_->AddTransitionState(D3D12_RESOURCE_STATE_COPY_DEST);
        previewShaderResource_ = std::make_unique<ShaderResourceResource>(previewTarget_.get());
        previewTarget_->SetCommandList(cmd);
        previewBufferWidth_ = width_;
        previewBufferHeight_ = height_;
    }
    if (!previewTarget_) return;
    previewTarget_->SetCommandList(cmd);

    if (!src->TransitionTo(D3D12_RESOURCE_STATE_COPY_SOURCE)) return;
    if (!previewTarget_->TransitionTo(D3D12_RESOURCE_STATE_COPY_DEST)) return;

    cmd->CopyResource(previewTarget_->GetResource(), src->GetResource());

    // 両方とも通常の読み取り用状態へ戻しておく（GetSrvHandle/GetPreviewSrvHandleでの参照に備える）
    previewTarget_->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    src->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

} // namespace KashipanEngine
