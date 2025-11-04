#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace KashipanEngine {

class DirectXCommon;

/// @brief DirectX12デバイスクラス
class DX12Device final {
public:
    DX12Device(Passkey<DirectXCommon>, IDXGIAdapter4 *adapter);
    ~DX12Device();

    /// @brief デバイス取得
    ID3D12Device *GetDevice() const { return device_.Get(); }

private:
    DX12Device(const DX12Device &) = delete;
    DX12Device &operator=(const DX12Device &) = delete;
    DX12Device(DX12Device &&) = delete;
    DX12Device &operator=(DX12Device &&) = delete;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
};

} // namespace KashipanEngine
