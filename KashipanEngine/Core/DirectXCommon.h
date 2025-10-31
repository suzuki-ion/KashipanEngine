#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include <memory>
#include "Core/DirectX/DX12DXGIs.h"
#include "Core/DirectX/DX12Device.h"
//#include "Core/DirectX/DX12Commands.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;

/// @brief DirectX共通クラス
class DirectXCommon final {
public:
    DirectXCommon(Passkey<GameEngine>, bool enableDebugLayer = true);
    ~DirectXCommon();

    //DX12SwapChain *CreateSwapChain(HWND hwnd, int32_t width, int32_t height, int32_t bufferCount = 2);

private:
    DirectXCommon(const DirectXCommon &) = delete;
    DirectXCommon &operator=(const DirectXCommon &) = delete;
    DirectXCommon(DirectXCommon &&) = delete;
    DirectXCommon &operator=(DirectXCommon &&) = delete;

    std::unique_ptr<DX12DXGIs> dx12DXGIs_;
    std::unique_ptr<DX12Device> dx12Device_;
    //std::unique_ptr<DX12Commands> dx12Commands_;
};

} // namespace KashipanEngine