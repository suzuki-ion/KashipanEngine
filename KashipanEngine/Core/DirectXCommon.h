#pragma once
#include "Core/DirectX/DX12CommandAllocator.h"
#include "Core/DirectX/DX12CommandList.h"
#include "Core/DirectX/DX12Device.h"
#include "Core/DirectX/DX12SwapChain.h"

#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <memory>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;

/// @brief DirectX共通クラス
class DirectXCommon final {
public:
    DirectXCommon(Passkey<GameEngine>);
    ~DirectXCommon() = default;

    //DX12SwapChain *CreateSwapChain(HWND hwnd, int32_t width, int32_t height, int32_t bufferCount = 2);

private:
    DirectXCommon(const DirectXCommon &) = delete;
    DirectXCommon &operator=(const DirectXCommon &) = delete;
    DirectXCommon(DirectXCommon &&) = delete;
    DirectXCommon &operator=(DirectXCommon &&) = delete;
};

} // namespace KashipanEngine