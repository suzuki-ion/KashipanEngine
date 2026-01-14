#pragma once
#include <d3d12.h>
#include <memory>
#include "Graphics/Resources/IGraphicsResource.h"

namespace KashipanEngine {

class IndexBufferResource final : public IGraphicsResource {
public:
    /// @brief コンストラクタ
    /// @param byteSize バッファサイズ（バイト単位）
    /// @param indexFormat インデックスフォーマット
    /// @param initialData 初期データ（nullptrの場合は未初期化）
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    IndexBufferResource(size_t byteSize, DXGI_FORMAT indexFormat, const void *initialData = nullptr, ID3D12Resource *existingResource = nullptr);

    /// @brief リソース再生成
    /// @param byteSize バッファサイズ（バイト単位）
    /// @param indexFormat インデックスフォーマット
    /// @param initialData 初期データ（nullptrの場合は未初期化）
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(size_t byteSize, DXGI_FORMAT indexFormat, const void *initialData = nullptr, ID3D12Resource *existingResource = nullptr);

    /// @brief インデックスバッファビュー取得
    D3D12_INDEX_BUFFER_VIEW GetView() const;

    /// @brief バッファマッピング 
    void *Map();
    /// @brief バッファアンマッピング
    void Unmap();

private:
    bool Initialize(size_t byteSize, DXGI_FORMAT indexFormat, const void *initialData, ID3D12Resource *existingResource);

    size_t bufferSize_ = 0;
    DXGI_FORMAT indexFormat_ = DXGI_FORMAT_R16_UINT;
};

} // namespace KashipanEngine
