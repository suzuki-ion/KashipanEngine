#pragma once
#include <d3d12.h>
#include <cstddef>
#include "Graphics/Resources/IGraphicsResource.h"

namespace KashipanEngine {

/// @brief Computeシェーダーから書き込み可能なRWStructuredBuffer用リソース
/// @details UAV(コンピュートシェーダーの書き込み先)としてもVBV(後続の描画パスでの頂点バッファ)としても
///          使えるよう、DEFAULTヒープ上にUAVフラグ付きで確保する。デスクリプタヒープは介さず、
///          ルートディスクリプタ（SetComputeRootUnorderedAccessView）で直接バインドする想定。
///          createSrv=true の場合はSRVも作成し、後続のピクセル/コンピュートシェーダーから
///          StructuredBuffer<T>として読み取れるようにする（自動ルートシグネチャ生成のデスクリプタ
///          テーブル経由のバインドは resourceViewType_ を見ず GetGPUDescriptorHandle() のみを見るため、
///          UAV用途との併用に支障はない）。
class RWStructuredBufferResource final : public IGraphicsResource {
public:
    /// @brief コンストラクタ
    /// @param elementStride 要素のストライドサイズ
    /// @param elementCount 要素数
    /// @param createSrv SRV を作成するか（コンピュートシェーダーで書き込んだ結果を別パスでSRVとして読む場合）
    RWStructuredBufferResource(size_t elementStride, size_t elementCount, bool createSrv = false);

    /// @brief リソース再生成（要素数・ストライドが変わった場合に呼ぶ）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(size_t elementStride, size_t elementCount, bool createSrv = false);

    /// @brief 頂点バッファビュー取得（スキニング後の描画時に使用）
    /// @param stride 頂点のストライド（バイト単位）
    D3D12_VERTEX_BUFFER_VIEW GetView(UINT stride) const;

    /// @brief SRV を持つか
    bool HasSrv() const noexcept { return GetDescriptorHandleInfo() != nullptr; }

    size_t GetBufferSize() const noexcept { return bufferSize_; }
    size_t GetElementStride() const noexcept { return elementStride_; }
    size_t GetElementCount() const noexcept { return elementCount_; }

private:
    bool Initialize(size_t elementStride, size_t elementCount, bool createSrv);

    size_t bufferSize_ = 0;
    size_t elementStride_ = 0;
    size_t elementCount_ = 0;
};

} // namespace KashipanEngine
