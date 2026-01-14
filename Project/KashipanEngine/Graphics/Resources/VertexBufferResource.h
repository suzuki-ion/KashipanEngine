#pragma once
#include <d3d12.h>
#include <memory>
#include "Graphics/Resources/IGraphicsResource.h"

namespace KashipanEngine {

/// @brief 頂点バッファリソースクラス
class VertexBufferResource final : public IGraphicsResource {
public:
    /// @brief コンストラクタ
    /// @param byteSize バッファサイズ（バイト単位）
    /// @param initialData 初期データ（nullptrの場合は未初期化）
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    VertexBufferResource(size_t byteSize, const void *initialData = nullptr, ID3D12Resource *existingResource = nullptr);

    /// @brief リソース再生成
    /// @param byteSize バッファサイズ（バイト単位）
    /// @param initialData 初期データ（nullptrの場合は未初期化）
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(size_t byteSize, const void *initialData = nullptr, ID3D12Resource *existingResource = nullptr);

    /// @brief 頂点バッファビュー取得
    /// @param stride 頂点のストライド（バイト単位）
    /// @return 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW GetView(UINT stride) const;

    /// @brief バッファマッピング
    void *Map();
    /// @brief バッファアンマッピング
    void Unmap();

private:
    bool Initialize(size_t byteSize, const void *initialData, ID3D12Resource *existingResource);

    size_t bufferSize_ = 0;
};

} // namespace KashipanEngine
