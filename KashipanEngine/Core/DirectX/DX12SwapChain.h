#pragma once
#include <dxgi1_6.h>
#include <dcomp.h>
#include "Graphics/Resources.h"
#include "Core/DirectX/DCompHost.h"

namespace KashipanEngine {

class DirectXCommon;
class Window;

/// @brief スワップチェーンの種類
enum class SwapChainType {
    Unknown = 0,
    ForHwnd,
    ForComposition,
};

/// @brief DirectX12スワップチェーンクラス
class DX12SwapChain final {
    static inline DirectXCommon *sDirectXCommon = nullptr;
    static inline ID3D12Device *sDevice = nullptr;
    static inline IDXGIFactory7 *sDXGIFactory = nullptr;
    static inline ID3D12CommandQueue *sCommandQueue = nullptr;
    static inline RTVHeap *sRTVHeap = nullptr;
    static inline DSVHeap *sDSVHeap = nullptr;

public:
    static void Initialize(Passkey<DirectXCommon>, DirectXCommon *directXCommon,
        ID3D12Device *device, IDXGIFactory7 *dxgiFactory, ID3D12CommandQueue *commandQueue,
        RTVHeap *rtvHeap, DSVHeap *dsvHeap) {
        sDirectXCommon = directXCommon;
        sDevice = device;
        sDXGIFactory = dxgiFactory;
        sCommandQueue = commandQueue;
        sRTVHeap = rtvHeap;
        sDSVHeap = dsvHeap;
    }
    /// @brief コンストラクタ
    /// @param swapChainType スワップチェーンの種類
    /// @param hwnd ウィンドウハンドル
    /// @param width 横幅
    /// @param height 高さ
    /// @param bufferCount バッファ数
    DX12SwapChain(Passkey<DirectXCommon>, SwapChainType swapChainType, HWND hwnd, int32_t width, int32_t height, int32_t bufferCount = 2);
    ~DX12SwapChain();

    /// @brief VSync有効化設定
    void SetVSyncEnabled(bool enabled) { enableVSync_ = enabled; }
    /// @brief ビューポートの設定
    void SetViewport(float topLeftX, float topLeftY, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    /// @brief シザー矩形の設定
    void SetScissor(int32_t left, int32_t top, int32_t right, int32_t bottom);
    /// @brief 目標のアスペクト比でレターボックスを計算してビューポートとシザー矩形を設定
    void SetLetterboxViewportAndScissor(float targetAspectRatio);
    /// @brief ビューポートとシザー矩形をデフォルト値にリセット
    void ResetViewportAndScissor();

    /// @brief 描画前処理
    void BeginDraw(Passkey<Window>);
    /// @brief 描画後処理
    void EndDraw(Passkey<DirectXCommon>);
    /// @brief SwapChainのPresent
    /// @param enableVSync VSync有効フラグ
    void Present(Passkey<DirectXCommon>);

    /// @brief SwapChainのサイズ変更指示
    /// @param width 横幅
    /// @param height 高さ
    void ResizeSignal(Passkey<Window>, int32_t width, int32_t height);
    /// @brief サイズ変更処理
    void Resize(Passkey<DirectXCommon>);

    /// @brief 現在記録中のコマンドリストを取得
    ID3D12GraphicsCommandList* GetRecordedCommandList(Passkey<DirectXCommon>) const noexcept { return commandList_.Get(); }

private:
    DX12SwapChain(const DX12SwapChain &) = delete;
    DX12SwapChain &operator=(const DX12SwapChain &) = delete;
    DX12SwapChain(DX12SwapChain &&) = delete;
    DX12SwapChain &operator=(DX12SwapChain &&) = delete;

    /// @brief スワップチェーンの作成（HWND用）
    void CreateSwapChainForHWND();
    /// @brief スワップチェーンの作成（Composition用）
    void CreateSwapChainForComposition();
    /// @brief ビューポートとシザー矩形の設定
    void SetViewportAndScissorRect();
    /// @brief バックバッファの作成
    void CreateBackBuffers();
    /// @brief 深度ステンシルバッファの作成
    void CreateDepthStencilBuffer();
    /// @brief コマンド関連オブジェクトの初期化
    void InitializeCommandObjects();

    //--------- スワップチェーンの情報 ---------//

    SwapChainType swapChainType_ = SwapChainType::Unknown;

    HWND hwnd_;
    int32_t width_;
    int32_t height_;
    int32_t bufferCount_;
    int32_t currentBufferIndex_ = 0;

    D3D12_VIEWPORT viewport_;
    D3D12_RECT scissorRect_;
    float targetAspectRatio_ = 0.0f;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    std::vector<std::unique_ptr<RenderTargetResource>> backBuffers_;
    std::unique_ptr<DepthStencilResource> depthStencilBuffer_;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;

    bool enableVSync_ = true;

    // Composition 用ホスト（HWND の上に合成したいときに使用）
    std::unique_ptr<DCompHost> dcompHost_;

    //--------- 指示情報 ---------//

    // Resize指示フラグとサイズ
    bool isResizeRequested_ = false;
    int32_t requestedWidth_ = 0;
    int32_t requestedHeight_ = 0;

    //--------- コマンド関連 ---------//

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> commandAllocators_;
};

} // namespace KashipanEngine