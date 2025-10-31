#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;

/// @brief DirectX12デバイスクラス
class DX12Device final {
public:
    DX12Device(Passkey<DirectXCommon>, IDXGIAdapter4 *adapter);
    ~DX12Device() = default;

    /// @brief D3D12デバイス取得
    ID3D12Device *GetD3D12Device() const { return d3d12Device_.Get(); }

private:
    DX12Device(const DX12Device &) = delete;
    DX12Device &operator=(const DX12Device &) = delete;
    DX12Device(DX12Device &&) = delete;
    DX12Device &operator=(DX12Device &&) = delete;

    Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device_;
};

} // namespace KashipanEngine
