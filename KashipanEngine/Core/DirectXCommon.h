#pragma once
#include "Core/DirectX/DX12CommandAllocator.h"
#include "Core/DirectX/DX12CommandList.h"
#include "Core/DirectX/DX12Device.h"
#include "Core/DirectX/DX12SwapChain.h"

#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <memory>

namespace KashipanEngine {

class GameEngine;

/// @brief DirectX共通クラス
class DirectXCommon final {
public:
    /// @brief DirectX共通クラスのファクトリークラス
    class Factory {
        friend class GameEngine;
        static std::unique_ptr<DirectXCommon> Create() {
            return std::unique_ptr<DirectXCommon>(new DirectXCommon());
        }
    };
    ~DirectXCommon() = default;

private:
    friend class Factory;
    DirectXCommon() = default;

    // 描画開始
    void BeginDraw();
    // 描画終了
    void EndDraw();

};

} // namespace KashipanEngine