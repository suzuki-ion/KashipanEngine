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

    /// @brief 同じ種別(RenderTargetKind)の描画先同士での描画順（小さいほど先に描画される）
    /// @details 既定は0。ある描画先が別の描画先の結果をポストエフェクト経由で参照する場合など、
    ///          描画順を明示したい時にのみ変更すること。値が同じ描画先同士の順序は未規定
    ///          （SceneRenderer::CompareSortableEntry参照）
    virtual std::int32_t GetRenderOrderPriority() const noexcept { return 0; }
};

} // namespace KashipanEngine
