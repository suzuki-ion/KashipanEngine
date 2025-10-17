#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>

namespace KashipanEngine {

// 前方宣言
class DirectXDevice;

enum class ResourceViewType {
    None,   // 未使用
    RTV,    // レンダーターゲットビュー
    DSV,    // 深度ステンシルビュー
    SRV,    // シェーダーリソースビュー
    UAV,    // アンオーダードアクセスビュー
    CBV,    // 定数バッファビュー
    VBV,    // 頂点バッファビュー
    IBV,    // インデックスバッファビュー
    SOV,    // ストリーム出力ビュー
};

/// @brief リソース用の抽象クラス
class IResource {
public:
    // 共通初期化（デバイスとコマンドリストのブリッジを設定）
    static void Initialize(DirectXDevice *dxDevice) {
        dxDevice_ = dxDevice;
        isCommonInitialized_ = (dxDevice_ != nullptr);
    }

    IResource(const std::string &name, ResourceViewType viewType)
        : name_(name), viewType_(viewType), format_(DXGI_FORMAT_UNKNOWN) {
        resourceCount_++;
    }
    virtual ~IResource() {
        resource_ = nullptr;
        resourceCount_--;
    }
    
    const ID3D12Resource *GetResource() const { return resource_.Get(); }
    ID3D12Resource *GetResource() { return resource_.Get(); }
    ResourceViewType GetViewType() const { return viewType_; }
    const std::string &GetName() const { return name_; }
    DXGI_FORMAT GetFormat() const { return format_; }
    D3D12_RESOURCE_STATES GetCurrentState() const { return currentState_; }
    UINT GetWidth() const { return width_; }
    UINT GetHeight() const { return height_; }
    
    // リソースの生成
    virtual void Create() = 0;
    // リソースの解放
    virtual void Release() = 0;
    
    // リソースの状態変更
    virtual void TransitionTo(D3D12_RESOURCE_STATES newState);
    
protected:
    // 共通ユーティリティ
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateCommittedResource(
        const D3D12_HEAP_PROPERTIES &heapProps,
        D3D12_HEAP_FLAGS heapFlags,
        const D3D12_RESOURCE_DESC &resourceDesc,
        D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE *clearValue);

    void SetResourceBarrier(D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

    UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE heapType) const;

protected:
    static DirectXDevice *dxDevice_;
    static inline bool isCommonInitialized_ = false;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    const std::string name_;
    const ResourceViewType viewType_;
    DXGI_FORMAT format_;

    // リソース寸法（必要に応じて設定）
    UINT width_ = 0;
    UINT height_ = 0;

    const D3D12_RESOURCE_STATES kFirstState_ = D3D12_RESOURCE_STATE_COMMON;
    const D3D12_RESOURCE_STATES kLastState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;

    // リソース再生成（派生で実装任意）
    virtual void RecreateResource() = 0;
    
private:
    // デバッグ用に作成されたリソースの数をカウントしておく
    static inline int32_t resourceCount_ = 0;
};

} // namespace KashipanEngine