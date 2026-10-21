#pragma once
#include <dcomp.h>
#include <wrl.h>

namespace KashipanEngine {

class DX12SwapChain;

/// @brief DirectCompositionホストクラス
class DCompHost final {
public:
    DCompHost(Passkey<DX12SwapChain>) {}
    ~DCompHost() = default;

    /// @brief HWND をターゲットとする DirectComposition の初期化
    /// @param topmost TRUE でオーバーレイ（常に手前）として合成されます
    bool InitializeForHwnd(HWND hwnd, BOOL topmost = TRUE);

    /// @brief ルートビジュアルに Composition スワップチェーンを設定
    /// @param swapChainContent スワップチェーンのコンテンツ
    bool SetContentSwapChain(IUnknown *swapChainContent);

    /// @brief 変更のコミット
    bool Commit();

    HWND GetHwnd() const { return hwnd_; }

private:
    DCompHost(const DCompHost &) = delete;
    DCompHost &operator=(const DCompHost &) = delete;
    DCompHost(DCompHost &&) = delete;
    DCompHost &operator=(DCompHost &&) = delete;

    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<IDCompositionDevice> device_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> target_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> root_;
};
} // namespace KashipanEngine