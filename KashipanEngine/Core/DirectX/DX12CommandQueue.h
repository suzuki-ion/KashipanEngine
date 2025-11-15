#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;

/// @brief DirectX12コマンドキュークラス
class DX12CommandQueue final {
public:
    DX12CommandQueue(Passkey<DirectXCommon>, ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
    ~DX12CommandQueue();

    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }

    /// @brief コマンドリスト群を実行
    void ExecuteCommandLists(Passkey<DirectXCommon>, const std::vector<ID3D12CommandList*>& lists);

private:
    DX12CommandQueue(const DX12CommandQueue&) = delete;
    DX12CommandQueue& operator=(const DX12CommandQueue&) = delete;
    DX12CommandQueue(DX12CommandQueue&&) = delete;
    DX12CommandQueue& operator=(DX12CommandQueue&&) = delete;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
};

} // namespace KashipanEngine
