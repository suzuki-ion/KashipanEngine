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
class ShaderResourceResource;

/// @brief GIFの現在フレームをRGBAテクスチャとして公開するクラス
/// @details 動画の`VideoTexture`と異なり、GIFのフレームは既にRGBA（`GifManager`が
///          WICでデコード済み）のため、YUV→RGB変換用のコンピュートシェーダーパスも
///          ダブルバッファ+フェンスポーリングも不要。フレーム切り替えは低頻度
///          （数十ms〜数百ms間隔）なため、`UploadFrame`は呼び出し元をブロックする
///          一括アップロード（TextureManagerの通常テクスチャ読み込みと同じ方式）で行う。
class GifTexture final : public IShaderTexture {
public:
    GifTexture(DirectXCommon *directXCommon, std::uint32_t width, std::uint32_t height);
    ~GifTexture() override;

    GifTexture(const GifTexture &) = delete;
    GifTexture &operator=(const GifTexture &) = delete;
    GifTexture(GifTexture &&) = delete;
    GifTexture &operator=(GifTexture &&) = delete;

    /// @brief GPUリソースの初期化に成功しているか
    bool IsValid() const noexcept { return valid_; }

    /// @brief 新しいRGBAフレームをアップロードする（呼び出し元スレッドをブロックする）
    /// @param rgba RGBA8フレームデータ（width*height*4バイト、タイトパッキング）
    /// @param dataSize rgbaのバイト数
    /// @return アップロードに成功した場合 true
    bool UploadFrame(const std::uint8_t *rgba, std::size_t dataSize);

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override;
    std::uint32_t GetWidth() const noexcept override { return width_; }
    std::uint32_t GetHeight() const noexcept override { return height_; }

private:
    DirectXCommon *directXCommon_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool valid_ = false;

    std::unique_ptr<ShaderResourceResource> texture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> upload_;
    void *mapped_ = nullptr;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout_{};
    UINT numRows_ = 0;
};

} // namespace KashipanEngine
