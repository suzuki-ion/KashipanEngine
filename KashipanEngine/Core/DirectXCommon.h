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
    DirectXCommon(Passkey<GameEngine>) {}
    ~DirectXCommon() = default;
    DirectXCommon(const DirectXCommon &) = delete;
    DirectXCommon &operator=(const DirectXCommon &) = delete;
    DirectXCommon(DirectXCommon &&) = delete;
    DirectXCommon &operator=(DirectXCommon &&) = delete;

private:
    // 描画開始
    void BeginDraw();
    // 描画終了
    void EndDraw();
};

} // namespace KashipanEngine