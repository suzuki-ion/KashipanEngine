#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Assets/ModelManager.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/IndexBufferResource.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Graphics/Resources/VertexBufferResource.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

/// @brief Renderer が使用するGPUリソースのキャッシュコンテナ
class ResourceContainer final {
public:
    /// @brief メッシュ頂点データ（Object系パイプラインの入力レイアウトと一致させる）
    struct MeshVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    /// @brief メッシュ用GPUバッファ
    struct MeshBuffers {
        std::unique_ptr<VertexBufferResource> vertexBuffer;
        std::unique_ptr<IndexBufferResource> indexBuffer;
        std::uint32_t vertexCount = 0;
        std::uint32_t indexCount = 0;
    };

    ResourceContainer() = default;
    ~ResourceContainer() = default;

    ResourceContainer(const ResourceContainer &) = delete;
    ResourceContainer &operator=(const ResourceContainer &) = delete;
    ResourceContainer(ResourceContainer &&) = delete;
    ResourceContainer &operator=(ResourceContainer &&) = delete;

    /// @brief メッシュハンドルからGPUバッファを取得（未作成の場合はモデルデータから生成する）
    /// @return メッシュバッファ（生成に失敗した場合は nullptr）
    const MeshBuffers *GetOrCreateMeshBuffers(ModelManager::ModelHandle meshHandle) {
        auto it = meshBuffers_.find(meshHandle);
        if (it != meshBuffers_.end()) return it->second.get();

        const auto &modelData = ModelManager::GetModelData(meshHandle);
        if (modelData.GetVertexCount() == 0 || modelData.GetIndexCount() == 0) return nullptr;

        std::vector<MeshVertex> vertices(modelData.GetVertexCount());
        const auto &srcVertices = modelData.GetVertices();
        for (size_t i = 0; i < vertices.size(); ++i) {
            const auto &src = srcVertices[i];
            vertices[i].position = Vector4(src.px, src.py, src.pz, 1.0f);
            vertices[i].texcoord = Vector2(src.u, src.v);
            vertices[i].normal = Vector3(src.nx, src.ny, src.nz);
        }
        const auto &srcIndices = modelData.GetIndices();

        auto buffers = std::make_unique<MeshBuffers>();
        buffers->vertexCount = modelData.GetVertexCount();
        buffers->indexCount = modelData.GetIndexCount();
        buffers->vertexBuffer = std::make_unique<VertexBufferResource>(
            sizeof(MeshVertex) * vertices.size(), vertices.data());
        buffers->indexBuffer = std::make_unique<IndexBufferResource>(
            sizeof(std::uint32_t) * srcIndices.size(), DXGI_FORMAT_R32_UINT, srcIndices.data());

        auto *raw = buffers.get();
        meshBuffers_.emplace(meshHandle, std::move(buffers));
        return raw;
    }

    /// @brief キーに対応する構造化バッファを取得（容量不足の場合は作り直す）
    StructuredBufferResource *GetOrCreateStructuredBuffer(const std::string &key, size_t elementStride, size_t elementCount) {
        if (elementStride == 0 || elementCount == 0) return nullptr;
        auto it = structuredBuffers_.find(key);
        if (it != structuredBuffers_.end() &&
            it->second.elementStride == elementStride &&
            it->second.capacity >= elementCount) {
            return it->second.buffer.get();
        }

        // 再確保時は余裕を持った容量にする
        size_t capacity = elementCount;
        if (it != structuredBuffers_.end() && it->second.elementStride == elementStride) {
            capacity = std::max(elementCount, it->second.capacity * 2);
        }

        StructuredBufferEntry entry;
        entry.elementStride = elementStride;
        entry.capacity = capacity;
        entry.buffer = std::make_unique<StructuredBufferResource>(elementStride, capacity);
        auto *raw = entry.buffer.get();
        structuredBuffers_[key] = std::move(entry);
        return raw;
    }

    /// @brief キーに対応する定数バッファを取得（サイズ不一致の場合は作り直す）
    ConstantBufferResource *GetOrCreateConstantBuffer(const std::string &key, size_t byteSize) {
        if (byteSize == 0) return nullptr;
        auto it = constantBuffers_.find(key);
        if (it != constantBuffers_.end() && it->second.byteSize == byteSize) {
            return it->second.buffer.get();
        }

        ConstantBufferEntry entry;
        entry.byteSize = byteSize;
        entry.buffer = std::make_unique<ConstantBufferResource>(byteSize);
        auto *raw = entry.buffer.get();
        constantBuffers_[key] = std::move(entry);
        return raw;
    }

    /// @brief 保持している全リソースを解放する
    void Clear() {
        meshBuffers_.clear();
        structuredBuffers_.clear();
        constantBuffers_.clear();
    }

private:
    struct StructuredBufferEntry {
        std::unique_ptr<StructuredBufferResource> buffer;
        size_t elementStride = 0;
        size_t capacity = 0;
    };
    struct ConstantBufferEntry {
        std::unique_ptr<ConstantBufferResource> buffer;
        size_t byteSize = 0;
    };

    std::unordered_map<ModelManager::ModelHandle, std::unique_ptr<MeshBuffers>> meshBuffers_;
    std::unordered_map<std::string, StructuredBufferEntry> structuredBuffers_;
    std::unordered_map<std::string, ConstantBufferEntry> constantBuffers_;
};

} // namespace KashipanEngine
