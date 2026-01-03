#include "IGraphicsResource.h"

namespace {
/// @brief リソース管理用コンテナ
std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> sResources_;
/// @brief コンテナの空きインデックス
std::vector<size_t> sFreeResourceIndices_;
/// @brief リソースIDに紐づくデスクリプタハンドル情報
std::vector<std::unique_ptr<KashipanEngine::DescriptorHandleInfo>> sDescriptorInfos_;
} // namespace

namespace KashipanEngine {

void IGraphicsResource::ClearAllResources(Passkey<DirectXCommon>) {
    LogScope scope;
    size_t resourceCount = sResources_.size() - sFreeResourceIndices_.size();
    Log(Translation("engine.graphics.resource.allclear.showcount") + std::to_string(resourceCount), LogSeverity::Debug);
    sResources_.clear();
    sDescriptorInfos_.clear();
    sFreeResourceIndices_.clear();
}

IGraphicsResource::~IGraphicsResource() {
    LogScope scope;
    if (resourceID_ < sResources_.size()) {
        sResources_[resourceID_].Reset();
        if (resourceID_ < sDescriptorInfos_.size()) {
            sDescriptorInfos_[resourceID_].reset();
        }
        sFreeResourceIndices_.push_back(resourceID_);
    }
}

void IGraphicsResource::CreateResource(const wchar_t *resourceName, const D3D12_HEAP_PROPERTIES *heapProperties, D3D12_HEAP_FLAGS heapFlags, const D3D12_RESOURCE_DESC *resourceDesc, const D3D12_CLEAR_VALUE *optimizedClearValue) {
    LogScope scope;
    if (device_ == nullptr) {
        Log(Translation("engine.graphics.resource.create.device.null"), LogSeverity::Warning);
        return;
    }
    if (transitionStates_.empty()) {
        Log(Translation("engine.graphics.resource.create.transition.empty"), LogSeverity::Warning);
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    currentStateIndex_ = 0;
    HRESULT hr = device_->CreateCommittedResource(
        heapProperties,
        heapFlags,
        resourceDesc,
        transitionStates_[currentStateIndex_],
        optimizedClearValue,
        IID_PPV_ARGS(&resource)
    );
    if (FAILED(hr)) {
        Log(Translation("engine.graphics.resource.create.failed"), LogSeverity::Error);
        return;
    }
    if (resourceName != nullptr) {
        resource->SetName(resourceName);
    }

    // リソースIDの割り当て
    if (sFreeResourceIndices_.empty()) {
        resourceID_ = static_cast<uint32_t>(sResources_.size());
        sResources_.push_back(resource);
        // sDescriptorInfos_ のサイズをリソースに合わせて拡張
        if (sDescriptorInfos_.size() < sResources_.size()) {
            sDescriptorInfos_.resize(sResources_.size());
        }
    } else {
        resourceID_ = static_cast<uint32_t>(sFreeResourceIndices_.back());
        sFreeResourceIndices_.pop_back();
        sResources_[resourceID_] = resource;
        // 再利用スロットのデスクリプタ情報は再生成前にクリアされている前提だが安全のためクリア
        if (resourceID_ < sDescriptorInfos_.size()) {
            sDescriptorInfos_[resourceID_].reset();
        }
    }
    resource_ = sResources_[resourceID_].Get();
}

void IGraphicsResource::SetExistingResource(ID3D12Resource *existingResource) {
    LogScope scope;
    // リソースIDの割り当て
    if (sFreeResourceIndices_.empty()) {
        resourceID_ = static_cast<uint32_t>(sResources_.size());
        sResources_.push_back(existingResource);
        if (sDescriptorInfos_.size() < sResources_.size()) {
            sDescriptorInfos_.resize(sResources_.size());
        }
    } else {
        resourceID_ = static_cast<uint32_t>(sFreeResourceIndices_.back());
        sFreeResourceIndices_.pop_back();
        sResources_[resourceID_] = existingResource;
        if (resourceID_ < sDescriptorInfos_.size()) {
            sDescriptorInfos_[resourceID_].reset();
        }
    }
    resource_ = sResources_[resourceID_].Get();
}

bool IGraphicsResource::TransitionTo(D3D12_RESOURCE_STATES desiredState) {
    LogScope scope;
    if (!commandList_) {
        Log(Translation("engine.graphics.resource.transition.commandlist.null"), LogSeverity::Warning);
        return false;
    }
    if (!resource_) {
        Log(Translation("engine.graphics.resource.transition.resource.null"), LogSeverity::Warning);
        return false;
    }

    const D3D12_RESOURCE_STATES before = GetCurrentState();
    if (before == desiredState) {
        return true;
    }

    D3D12_RESOURCE_BARRIER barrierDesc{};
    barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierDesc.Transition.pResource = resource_;
    barrierDesc.Transition.StateBefore = before;
    barrierDesc.Transition.StateAfter = desiredState;
    barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrierDesc);

    // 追跡配列内に desiredState がある場合はインデックスを同期
    for (uint32_t i = 0; i < static_cast<uint32_t>(transitionStates_.size()); ++i) {
        if (transitionStates_[i] == desiredState) {
            currentStateIndex_ = i;
            return true;
        }
    }

    // 配列に無い場合は追跡は更新できないが、バリア自体は発行済み
    return true;
}

bool IGraphicsResource::TransitionToNext() {
    LogScope scope;
    if (!commandList_) {
        Log(Translation("engine.graphics.resource.transition.commandlist.null"), LogSeverity::Warning);
        return false;
    }
    if (transitionStates_.empty()) {
        Log(Translation("engine.graphics.resource.transition.empty"), LogSeverity::Warning);
        return false;
    }

    uint32_t nextIndex = currentStateIndex_ + 1;
    if (nextIndex >= transitionStates_.size()) {
        nextIndex = 0;
    }

    D3D12_RESOURCE_BARRIER barrierDesc = {};
    barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierDesc.Transition.pResource = resource_;
    barrierDesc.Transition.StateBefore = transitionStates_[currentStateIndex_];
    barrierDesc.Transition.StateAfter = transitionStates_[nextIndex];
    barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrierDesc);

    currentStateIndex_ = nextIndex;
    return true;
}

void IGraphicsResource::ResetResourceForRecreate() {
    LogScope scope;
    // デスクリプタハンドルを解放
    if (resourceID_ < sDescriptorInfos_.size()) {
        sDescriptorInfos_[resourceID_].reset();
    }

    // リソースを解放し、空きインデックスに戻す
    if (resourceID_ < sResources_.size()) {
        sResources_[resourceID_].Reset();
        sFreeResourceIndices_.push_back(resourceID_);
    }

    resource_ = nullptr;
    currentStateIndex_ = 0;
}

void IGraphicsResource::SetDescriptorHandleInfo(std::unique_ptr<DescriptorHandleInfo> info) {
    if (resourceID_ >= sDescriptorInfos_.size()) {
        sDescriptorInfos_.resize(static_cast<size_t>(resourceID_) + 1);
    }
    sDescriptorInfos_[resourceID_] = std::move(info);
}

DescriptorHandleInfo *IGraphicsResource::GetDescriptorHandleInfo() const {
    if (resourceID_ < sDescriptorInfos_.size()) {
        return sDescriptorInfos_[resourceID_].get();
    }
    return nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE IGraphicsResource::GetCPUDescriptorHandle() const {
    auto *info = GetDescriptorHandleInfo();
    return info ? info->cpuHandle : D3D12_CPU_DESCRIPTOR_HANDLE{};
}

D3D12_GPU_DESCRIPTOR_HANDLE IGraphicsResource::GetGPUDescriptorHandle() const {
    auto *info = GetDescriptorHandleInfo();
    return info ? info->gpuHandle : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

} // namespace KashipanEngine