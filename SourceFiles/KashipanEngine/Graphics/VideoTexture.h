#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

#include "Graphics/IShaderTexture.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;
class Renderer;
class ShaderResourceResource;
class UnorderedAccessResource;
class VideoTextureRgbaView;

/// @brief 動画フレーム(NV12)を受け取り、コンピュートシェーダーでYUV→RGBA変換した
///        通常のRGBAテクスチャとして公開するクラス
/// @details 輝度(Y)・色差(UV)はNV12のままダブルバッファの`ShaderResourceResource`へCPUアップロードし
///          （`UploadFrame`）、その後Rendererのコンピュートフェーズ（`Renderer::ProcessVideoConversions`）で
///          専用コンピュートシェーダー("VideoYUVToRGB"パイプライン)がY/UVを読み取りRGBAへ変換して
///          `UnorderedAccessResource`へ書き込む。変換後のRGBAテクスチャは通常のテクスチャと同様に
///          `IShaderTexture`として公開されるため、`TextureManager::RegisterExternalTexture`経由で
///          `Material::textureHandle`へそのまま割り当てることができ、既存のどのパイプライン
///          （2D/3D、ライティング有無を問わず）でも普通のテクスチャとして扱える。
class VideoTexture final {
public:
    VideoTexture(DirectXCommon *directXCommon, std::uint32_t width, std::uint32_t height);
    ~VideoTexture();

    VideoTexture(const VideoTexture &) = delete;
    VideoTexture &operator=(const VideoTexture &) = delete;
    VideoTexture(VideoTexture &&) = delete;
    VideoTexture &operator=(VideoTexture &&) = delete;

    /// @brief GPUリソースの初期化に成功しているか
    bool IsValid() const noexcept { return valid_; }

    /// @brief 新しいNV12フレームをY/UVのGPUテクスチャへアップロードする（呼び出し元スレッドをブロックしない）
    /// @details 直前にこのスロットへ発行したGPUコピーがまだ完了していない場合は何もせず false を返す。
    ///          呼び出し元（VideoPlayer）は次回のUpdateで再試行することを想定している。
    ///          成功した場合、次回のRenderer::ProcessVideoConversionsでYUV→RGB変換が行われるよう予約する
    /// @param nv12Data NV12形式のフレームデータ（Y平面の直後にUV平面が続く）
    /// @param dataSize nv12Dataのバイト数
    /// @param sourceStride Y平面・UV平面それぞれの行ストライド（パディング込みのバイト幅）。
    ///        0を指定した場合はタイトパッキング（ストライド=幅）とみなす。デコーダによっては
    ///        16等の境界に切り上げたストライドで出力するため、決め打ちにすると解像度次第で
    ///        フレームが崩れる/取りこぼす原因になる
    /// @return アップロードを発行できた場合 true
    bool UploadFrame(const std::uint8_t *nv12Data, std::size_t dataSize, std::uint32_t sourceStride = 0);

    /// @brief YUV→RGB変換後のテクスチャのシェーダービューを取得する
    /// @details TextureManager::RegisterExternalTextureで登録し、Material::textureHandleへ
    ///          そのまま割り当てることを想定している
    IShaderTexture *GetRgbaView() const noexcept;

    std::uint32_t GetWidth() const noexcept { return width_; }
    std::uint32_t GetHeight() const noexcept { return height_; }

    /// @brief このフレームにYUV→RGB変換が必要かどうか（Renderer::ProcessVideoConversions専用）
    bool HasPendingConversion(Passkey<Renderer>) const noexcept { return pendingConversion_; }
    /// @brief Y平面の現在の読み取り対象リソースを取得する（Renderer::ProcessVideoConversions専用）
    ShaderResourceResource *GetLumaResource(Passkey<Renderer>) const noexcept;
    /// @brief UV平面の現在の読み取り対象リソースを取得する（Renderer::ProcessVideoConversions専用）
    ShaderResourceResource *GetChromaResource(Passkey<Renderer>) const noexcept;
    /// @brief 変換先RGBAテクスチャのUAVリソースを取得する（Renderer::ProcessVideoConversions専用）
    UnorderedAccessResource *GetRgbaUavResource(Passkey<Renderer>) const noexcept;
    /// @brief 変換予約フラグをクリアする（Renderer::ProcessVideoConversionsが変換実行後に呼ぶ）
    void ClearPendingConversion(Passkey<Renderer>) noexcept { pendingConversion_ = false; }

private:
    friend class VideoTextureRgbaView;

    struct YuvSlot final {
        std::unique_ptr<ShaderResourceResource> lumaTexture;
        std::unique_ptr<ShaderResourceResource> chromaTexture;
        Microsoft::WRL::ComPtr<ID3D12Resource> lumaUpload;
        Microsoft::WRL::ComPtr<ID3D12Resource> chromaUpload;
        void *lumaMapped = nullptr;
        void *chromaMapped = nullptr;
        /// @brief このスロットへ最後に発行したGPUコピーのフェンス値（0=未発行）
        std::uint64_t uploadFenceValue = 0;
    };

    static constexpr std::uint32_t kBufferCount = 2;

    bool InitializeYuvSlot(YuvSlot &slot);
    bool InitializeRgbaOutput();

    D3D12_GPU_DESCRIPTOR_HANDLE GetRgbaSrvHandle() const noexcept;

    DirectXCommon *directXCommon_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t chromaWidth_ = 0;
    std::uint32_t chromaHeight_ = 0;
    bool valid_ = false;
    bool pendingConversion_ = false;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT lumaLayout_{};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT chromaLayout_{};
    UINT lumaNumRows_ = 0;
    UINT chromaNumRows_ = 0;

    YuvSlot yuvSlots_[kBufferCount];
    std::uint32_t writeIndex_ = 0;
    std::uint32_t readIndex_ = 0;

    // YUV→RGB変換後のテクスチャ。単一バッファでよい（共有DIRECTキューのFIFO実行順序により、
    // ある1フレーム内の[コンピュート変換書き込み]→[その後の描画コマンドでのSRV読み取り]が必ずこの順で
    // 実行され、かつ次フレームのコンピュート書き込みは前フレームの描画読み取りより必ず後に実行されるため）
    std::unique_ptr<UnorderedAccessResource> rgbaUav_;
    std::unique_ptr<ShaderResourceResource> rgbaSrv_;
    std::unique_ptr<VideoTextureRgbaView> rgbaView_;
};

} // namespace KashipanEngine
