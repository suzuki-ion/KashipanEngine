#pragma once
#include "Core/DirectX/DescriptorHeaps/DescriptorHeapBase.h"
#include "Core/DirectX/DescriptorHeaps/HeapRTV.h"
#include "Core/DirectX/DescriptorHeaps/HeapDSV.h"
#include "Core/DirectX/DescriptorHeaps/HeapSRV.h"
#include "Core/DirectX/DescriptorHeaps/HeapSampler.h"

namespace KashipanEngine {

class DirectXCommon;
class ShaderVariableBinder;
class ScreenBuffer;
class ShadowMapBuffer;

enum class ResourceViewType {
    None,
    RTV,
    DSV,
    SRV,
    UAV,
    CBV,
    VBV,
    IBV,
};

/// @brief グラフィックのリソースインターフェース
class IGraphicsResource {
    static inline ID3D12Device *device_ = nullptr;
    static inline RTVHeap *rtvHeap_ = nullptr;
    static inline DSVHeap *dsvHeap_ = nullptr;
    static inline SRVHeap *srvHeap_ = nullptr;
    static inline SamplerHeap *samplerHeap_ = nullptr;
public:
    static void SetDevice(Passkey<DirectXCommon>, ID3D12Device *device) { device_ = device; }
    static void SetDescriptorHeaps(Passkey<DirectXCommon>, RTVHeap *rtv, DSVHeap *dsv, SRVHeap *srv, SamplerHeap *sampler) {
        rtvHeap_ = rtv; dsvHeap_ = dsv; srvHeap_ = srv; samplerHeap_ = sampler;
    }
    static ID3D12Device *GetDevice(Passkey<ShaderVariableBinder>) { return device_; }

    static SRVHeap *GetSRVHeap(Passkey<ScreenBuffer>) { return srvHeap_; }
    static SamplerHeap *GetSamplerHeap(Passkey<ScreenBuffer>) { return samplerHeap_; }

    static SRVHeap *GetSRVHeap(Passkey<ShadowMapBuffer>) { return srvHeap_; }
    static SamplerHeap *GetSamplerHeap(Passkey<ShadowMapBuffer>) { return samplerHeap_; }

    static void ClearAllResources(Passkey<DirectXCommon>);
    virtual ~IGraphicsResource();

    IGraphicsResource(const IGraphicsResource &) = delete;
    IGraphicsResource &operator=(const IGraphicsResource &) = delete;
    IGraphicsResource(IGraphicsResource &&) = delete;
    IGraphicsResource &operator=(IGraphicsResource &&) = delete;

    // コマンドリスト設定（インスタンス単位）
    void SetCommandList(ID3D12GraphicsCommandList *commandList) { commandList_ = commandList; }
    ID3D12GraphicsCommandList *GetCommandList() const { return commandList_; }

    /// @brief バリアの状態を追加
    void AddTransitionState(D3D12_RESOURCE_STATES state) { transitionStates_.push_back(state); }
    /// @brief バリアの状態リストをクリア
    void ClearTransitionStates() { transitionStates_.clear(); }

    /// @brief バリアを指定状態に遷移
    /// @details 現在状態（エンジン側追跡値）から desiredState へ遷移する
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool TransitionTo(D3D12_RESOURCE_STATES desiredState);

    /// @brief バリアを次の状態に遷移
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool TransitionToNext();

    /// @brief リソース取得
    ID3D12Resource *GetResource() const { return resource_; }
    /// @brief リソースビューの種類を取得
    ResourceViewType GetResourceViewType() const { return resourceViewType_; }

    /// @brief 現在のバリア状態取得
    D3D12_RESOURCE_STATES GetCurrentState() const { return transitionStates_.empty() ? D3D12_RESOURCE_STATE_COMMON : transitionStates_[currentStateIndex_]; }

    /// @brief デスクリプタCPUハンドル取得（未割り当ての場合は空）
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle() const;
    /// @brief デスクリプタグラフィックスハンドル取得（未割り当ての場合は空）
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle() const;

protected:
    /// @brief リソース作成
    void CreateResource(
        const wchar_t *resourceName,
        const D3D12_HEAP_PROPERTIES *heapProperties,
        D3D12_HEAP_FLAGS heapFlags,
        const D3D12_RESOURCE_DESC *resourceDesc,
        const D3D12_CLEAR_VALUE *optimizedClearValue = nullptr
    );
    /// @brief 既存のリソースを設定
    void SetExistingResource(ID3D12Resource *existingResource);

    /// @brief 派生クラス限定のコンストラクタ
    /// @param viewType リソースビューの種類
    IGraphicsResource(ResourceViewType viewType) : resourceViewType_(viewType) {}

    /// @brief デバイス取得（派生クラス用）
    ID3D12Device *GetDevice() const { return device_; }
    /// @brief RTVヒープ取得（派生クラス用）
    RTVHeap *GetRTVHeap() const { return rtvHeap_; }
    /// @brief DSVヒープ取得（派生クラス用）
    DSVHeap *GetDSVHeap() const { return dsvHeap_; }
    /// @brief SRVヒープ取得（派生クラス用）
    SRVHeap *GetSRVHeap() const { return srvHeap_; }
    /// @brief Samplerヒープ取得（派生クラス用）
    SamplerHeap *GetSamplerHeap() const { return samplerHeap_; }
    /// @brief デスクリプタハンドル情報設定（派生クラス用）
    void SetDescriptorHandleInfo(std::unique_ptr<DescriptorHandleInfo> info);
    /// @brief デスクリプタハンドル情報取得（派生クラス用）
    DescriptorHandleInfo *GetDescriptorHandleInfo() const;
    /// @brief 再生成前に現在のリソースを解放（派生クラス用）
    void ResetResourceForRecreate();

    const ResourceViewType resourceViewType_ = ResourceViewType::None;

private:
    uint32_t resourceID_ = 0;
    ID3D12Resource *resource_ = nullptr;
    ID3D12GraphicsCommandList *commandList_ = nullptr;
    uint32_t currentStateIndex_ = 0;
    std::vector<D3D12_RESOURCE_STATES> transitionStates_;
};

} // namespace KashipanEngine