#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;

/// @brief DirectX12のコマンドクラス
class DX12Commands final {
public:
    DX12Commands(Passkey<DirectXCommon>, ID3D12Device *device, D3D12_COMMAND_LIST_TYPE type);
    ~DX12Commands();
    /// @brief コマンドリスト取得
    ID3D12GraphicsCommandList *GetCommandList() const { return commandList_.Get(); }
    /// @brief コマンドアロケーターリセット
    void ResetCommandAllocator();
    /// @brief コマンドリストリセット
    void ResetCommandList();
    /// @brief コマンドリストクローズ
    void CloseCommandList();

private:
    DX12Commands(const DX12Commands &) = delete;
    DX12Commands &operator=(const DX12Commands &) = delete;
    DX12Commands(DX12Commands &&) = delete;
    DX12Commands &operator=(DX12Commands &&) = delete;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
};

} // namespace KashipanEngine