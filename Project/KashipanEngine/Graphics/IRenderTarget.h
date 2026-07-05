#pragma once
#include <cstdint>
#include <string>

struct ID3D12GraphicsCommandList;
namespace KashipanEngine {

/// @brief 描画先の種類
enum class RenderTargetKind {
    Window,
    ScreenBuffer,
    ShadowMapBuffer,
    RenderTargetKindCount
};

/// @brief 描画先インターフェースクラス
class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    virtual RenderTargetKind GetRenderTargetKind() const noexcept = 0;
    virtual std::string GetRenderTargetName() const = 0;
    virtual std::uint32_t GetRenderTargetWidth() const noexcept = 0;
    virtual std::uint32_t GetRenderTargetHeight() const noexcept = 0;
    virtual bool IsRenderTargetAvailable() const noexcept = 0;

    /// @brief 描画開始処理（バリアの設定やレンダーターゲットのクリアなどを行う）
    virtual void BeginDraw() = 0;
    /// @brief 描画終了処理（バリアの設定やコマンドリストのクローズなどを行う）
    virtual void EndDraw() = 0;
    /// @brief 描画に使用するコマンドリストを取得する
    virtual ID3D12GraphicsCommandList *GetCommandList() const = 0;
};

} // namespace KashipanEngine
