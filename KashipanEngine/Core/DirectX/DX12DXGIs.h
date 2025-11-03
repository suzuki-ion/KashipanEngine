#pragma once
#include <dxgi1_6.h>
#include <wrl.h>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;

/// @brief DirectX12 DXGIクラス
class DX12DXGIs final {
public:
    DX12DXGIs(Passkey<DirectXCommon>);
    ~DX12DXGIs();

    /// @brief DXGIファクトリー取得
    IDXGIFactory7 *GetDXGIFactory() const { return dxgiFactory_.Get(); }
    /// @brief DXGIアダプター取得
    IDXGIAdapter4 *GetDXGIAdapter() const { return dxgiAdapter_.Get(); }

private:
    DX12DXGIs(const DX12DXGIs &) = delete;
    DX12DXGIs &operator=(const DX12DXGIs &) = delete;
    DX12DXGIs(DX12DXGIs &&) = delete;
    DX12DXGIs &operator=(DX12DXGIs &&) = delete;

    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> dxgiAdapter_;
};

} // namespace KashipanEngine