#pragma once
#include <d3d12.h>
#include <memory>
#include "Graphics/Resources/IGraphicsResource.h"
#include "Core/DirectX/DescriptorHeaps/HeapSRV.h"

namespace KashipanEngine {

class ConstantBufferResource final : public IGraphicsResource {
public:
    /// @brief コンストラクタ
    /// @param byteSize バッファサイズ（バイト単位）
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    ConstantBufferResource(size_t byteSize, ID3D12Resource *existingResource = nullptr);

    /// @brief リソース再生成
    /// @param byteSize バッファサイズ（バイト単位）
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(size_t byteSize, ID3D12Resource *existingResource = nullptr);

    /// @brief バッファマッピング
    void *Map();
    /// @brief バッファアンマッピング
    void Unmap();

    size_t GetBufferSize() const { return bufferSize_; }

private:
    bool Initialize(size_t byteSize, ID3D12Resource *existingResource);

    size_t bufferSize_ = 0;
};

} // namespace KashipanEngine
