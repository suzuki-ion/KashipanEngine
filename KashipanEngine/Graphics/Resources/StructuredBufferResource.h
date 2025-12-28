#pragma once
#include <d3d12.h>
#include <cstddef>
#include "Graphics/Resources/IGraphicsResource.h"

namespace KashipanEngine {

/// @brief StructuredBuffer 用リソース
class StructuredBufferResource final : public IGraphicsResource {
public:
    /// @brief コンストラクタ
    /// @param elementStride 要素のストライドサイズ
    /// @param elementCount 要素数
    /// @param initialData 初期データ（nullptrの場合はゼロ初期化）
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    StructuredBufferResource(size_t elementStride, size_t elementCount, const void* initialData = nullptr, ID3D12Resource* existingResource = nullptr);

    /// @brief リソース再生成
    /// @param elementStride 要素のストライドサイズ
    /// @param elementCount 要素数
    /// @param initialData 初期データ（nullptrの場合はゼロ初期化）
    /// @param existingResource 既存リソース（nullptrの場合は新規作成）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool Recreate(size_t elementStride, size_t elementCount, const void* initialData = nullptr, ID3D12Resource* existingResource = nullptr);

    void* Map();
    void Unmap();

    size_t GetBufferSize() const { return bufferSize_; }
    size_t GetElementStride() const { return elementStride_; }
    size_t GetElementCount() const { return elementCount_; }

private:
    bool Initialize(size_t elementStride, size_t elementCount, const void* initialData, ID3D12Resource* existingResource);

    size_t bufferSize_ = 0;
    size_t elementStride_ = 0;
    size_t elementCount_ = 0;
};

} // namespace KashipanEngine
