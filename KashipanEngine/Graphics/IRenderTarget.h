#pragma once
#include <d3d12.h>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class Renderer;

/// @brief レンダーターゲットインターフェイス
class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    /// @brief 描画開始処理
    virtual void BeginRender(Passkey<Renderer>) = 0;
    /// @brief 描画終了処理
    virtual void EndRender(Passkey<Renderer>) = 0;

    /// @brief コマンドリスト取得
    virtual ID3D12GraphicsCommandList *GetCommandList(Passkey<Renderer>) = 0;
};

} // namespace KashipanEngine