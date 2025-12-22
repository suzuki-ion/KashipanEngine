#pragma once
#include <dxgi1_6.h>
#include <dcomp.h>
#include "Graphics/Resources.h"
#include "Core/DirectX/DCompHost.h"

namespace KashipanEngine {

class GameEnginel;
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
    static inline SRVHeap *sSRVHeap = nullptr;

public:
    static void Initialize(Passkey<DirectXCommon>, DirectXCommon *directXCommon,
        ID3D12Device *device, IDXGIFactory7 *dxgiFactory, ID3D12CommandQueue *commandQueue,
        RTVHeap *rtvHeap, DSVHeap *dsvHeap, SRVHeap *srvHeap) {
        sDirectXCommon = directXCommon;
        sDevice = device;
        sDXGIFactory = dxgiFactory;
        sCommandQueue = commandQueue;
        sRTVHeap = rtvHeap;
        sDSVHeap = dsvHeap;
        sSRVHeap = srvHeap;
    }

    /// @brief 遅延初期化用コンストラクタ (HWND 未決定)
    DX12SwapChain(Passkey<DirectXCommon>, int32_t bufferCount = 2) : bufferCount_(bufferCount) {
        InitializeCommandObjects();
    }
    ~DX12SwapChain() { DestroyInternal(); }

    DX12SwapChain(const DX12SwapChain &) = delete;
    DX12SwapChain &operator=(const DX12SwapChain &) = delete;
    DX12SwapChain(DX12SwapChain &&) = delete;
    DX12SwapChain &operator=(DX12SwapChain &&) = delete;

    /// @brief 実体生成 (HWND 取得後に呼ぶ)
    void AttachWindowAndCreate(Passkey<DirectXCommon>, SwapChainType swapChainType, HWND hwnd, int32_t width, int32_t height);
    /// @brief 利用終了 (内部リソース解放、スロット再利用)
    void Destroy(Passkey<DirectXCommon>);
    /// @brief 生成済みか
    bool IsCreated() const noexcept { return isCreated_; }
    /// @brief 描画中か
    bool IsDrawing() const noexcept { return isDrawing_; }

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
    /// @brief Present
    void Present(Passkey<DirectXCommon>);

    /// @brief サイズ変更指示
    void ResizeSignal(Passkey<Window>, int32_t width, int32_t height);
    /// @brief サイズ変更反映
    void Resize(Passkey<DirectXCommon>);

    /// @brief 記録済みコマンドリスト取得 (DirectXCommon 用)
    ID3D12GraphicsCommandList* GetRecordedCommandList(Passkey<DirectXCommon>) const noexcept { return commandList_.Get(); }
    /// @brief 記録済みコマンドリスト取得 (Window 用)
    ID3D12GraphicsCommandList *GetRecordedCommandList(Passkey<Window>) const noexcept { return commandList_.Get(); }

    int32_t GetBufferCount() const noexcept { return bufferCount_; }
    DXGI_FORMAT GetBackBufferFormat() const noexcept { return backBufferFormat_; }

private:
    /// @brief スワップチェーン作成 (HWND 用)
    void CreateSwapChainForHWND();
    /// @brief スワップチェーン作成 (Composition 用)
    void CreateSwapChainForComposition();
    /// @brief ビューポート / シザー設定
    void SetViewportAndScissorRect();
    /// @brief バックバッファ生成
    void CreateBackBuffers();
    /// @brief 深度ステンシル生成
    void CreateDepthStencilBuffer();
    /// @brief コマンド関連オブジェクト生成
    void InitializeCommandObjects();
    /// @brief 内部リソース破棄
    void DestroyInternal();

    //--------- スワップチェーン状態 ---------//
    SwapChainType swapChainType_ = SwapChainType::Unknown;
    HWND hwnd_ = nullptr;
    int32_t width_ = 0;
    int32_t height_ = 0;
    int32_t bufferCount_ = 2;
    int32_t currentBufferIndex_ = 0;

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};
    float targetAspectRatio_ = 0.0f;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    std::vector<std::unique_ptr<RenderTargetResource>> backBuffers_;
    std::unique_ptr<DepthStencilResource> depthStencilBuffer_;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};

    bool enableVSync_ = true;
    std::unique_ptr<DCompHost> dcompHost_;

    // Resize 指示
    bool isResizeRequested_ = false;
    int32_t requestedWidth_ = 0;
    int32_t requestedHeight_ = 0;

    // コマンド関連
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> commandAllocators_;

    bool isCreated_ = false;
    bool isDrawing_ = false;

    DXGI_FORMAT backBufferFormat_ = DXGI_FORMAT_UNKNOWN;
};

} // namespace KashipanEngine