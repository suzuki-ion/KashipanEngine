#pragma once
#include <d3d12.h>
#include <wrl.h>

namespace KashipanEngine {

class DirectXCommon;

/// @brief DirectX12のコマンドアロケータクラス
class DX12CommandAllocator {
public:
    ID3D12CommandAllocator* const GetCommandAllocator() { return commandAllocator_.Get(); }
    D3D12_COMMAND_LIST_TYPE GetType() const { return type_; }
    void Reset();
private:
    DX12CommandAllocator(DirectXCommon *dxCommon, D3D12_COMMAND_LIST_TYPE type);
    ~DX12CommandAllocator() = default;
    friend class DirectXCommon;

    // コマンドアロケータ
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    // コマンドリストタイプ
    D3D12_COMMAND_LIST_TYPE type_;
};

} // namespace KashipanEngine
