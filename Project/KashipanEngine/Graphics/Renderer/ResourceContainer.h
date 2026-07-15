#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Assets/ModelManager.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/IndexBufferResource.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Graphics/Resources/UnorderedAccessResource.h"
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

    /// @brief キーに対応する頂点バッファを取得（容量不足の場合は作り直す）
    VertexBufferResource *GetOrCreateVertexBuffer(const std::string &key, size_t byteSize) {
        if (byteSize == 0) return nullptr;
        auto it = vertexBuffers_.find(key);
        if (it != vertexBuffers_.end() && it->second.byteSize >= byteSize) {
            return it->second.buffer.get();
        }

        // 再確保時は余裕を持った容量にする
        size_t capacity = byteSize;
        if (it != vertexBuffers_.end()) {
            capacity = std::max(byteSize, it->second.byteSize * 2);
        }

        VertexBufferEntry entry;
        entry.byteSize = capacity;
        entry.buffer = std::make_unique<VertexBufferResource>(capacity);
        auto *raw = entry.buffer.get();
        vertexBuffers_[key] = std::move(entry);
        return raw;
    }

    /// @brief キーに対応するUAVテクスチャを取得（サイズ/フォーマット不一致の場合は作り直す）
    /// @details 作り直された場合、前フレームまでの内容は失われる（Computeシェーダー側での
    ///          再初期化が必要な場合は formatKind やサイズを変更しない運用にすること）
    UnorderedAccessResource *GetOrCreateUAVTexture(const std::string &key, UINT width, UINT height, DXGI_FORMAT format) {
        if (width == 0 || height == 0) return nullptr;
        auto it = uavTextures_.find(key);
        if (it != uavTextures_.end() && it->second.width == width && it->second.height == height && it->second.format == format) {
            return it->second.texture.get();
        }

        UAVTextureEntry entry;
        entry.width = width;
        entry.height = height;
        entry.format = format;
        entry.texture = std::make_unique<UnorderedAccessResource>(width, height, format);
        auto *raw = entry.texture.get();
        uavTextures_[key] = std::move(entry);
        return raw;
    }

    /// @brief キーに対応するSRV付きRWStructuredBufferを取得（容量不足の場合は作り直す）
    /// @details Computeシェーダーで書き込んだ後、別パスでStructuredBuffer(SRV)として読む用途向け
    ///          （例: Forward+のタイルライトインデックスバッファ）。作り直された場合は
    ///          前フレームまでの内容は失われる
    RWStructuredBufferResource *GetOrCreateRWStructuredBufferSrv(const std::string &key, size_t elementStride, size_t elementCount) {
        if (elementStride == 0 || elementCount == 0) return nullptr;
        auto it = rwStructuredBuffers_.find(key);
        if (it != rwStructuredBuffers_.end() &&
            it->second.elementStride == elementStride &&
            it->second.capacity >= elementCount) {
            return it->second.buffer.get();
        }

        // 再確保時は余裕を持った容量にする
        size_t capacity = elementCount;
        if (it != rwStructuredBuffers_.end() && it->second.elementStride == elementStride) {
            capacity = std::max(elementCount, it->second.capacity * 2);
        }

        RWStructuredBufferEntry entry;
        entry.elementStride = elementStride;
        entry.capacity = capacity;
        entry.buffer = std::make_unique<RWStructuredBufferResource>(elementStride, capacity, true);
        auto *raw = entry.buffer.get();
        rwStructuredBuffers_[key] = std::move(entry);
        return raw;
    }

    /// @brief 保持している全リソースを解放する
    void Clear() {
        meshBuffers_.clear();
        structuredBuffers_.clear();
        rwStructuredBuffers_.clear();
        constantBuffers_.clear();
        vertexBuffers_.clear();
        uavTextures_.clear();
    }

private:
    struct StructuredBufferEntry {
        std::unique_ptr<StructuredBufferResource> buffer;
        size_t elementStride = 0;
        size_t capacity = 0;
    };
    struct RWStructuredBufferEntry {
        std::unique_ptr<RWStructuredBufferResource> buffer;
        size_t elementStride = 0;
        size_t capacity = 0;
    };
    struct ConstantBufferEntry {
        std::unique_ptr<ConstantBufferResource> buffer;
        size_t byteSize = 0;
    };
    struct VertexBufferEntry {
        std::unique_ptr<VertexBufferResource> buffer;
        size_t byteSize = 0;
    };
    struct UAVTextureEntry {
        std::unique_ptr<UnorderedAccessResource> texture;
        UINT width = 0;
        UINT height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    };

    std::unordered_map<ModelManager::ModelHandle, std::unique_ptr<MeshBuffers>> meshBuffers_;
    std::unordered_map<std::string, StructuredBufferEntry> structuredBuffers_;
    std::unordered_map<std::string, RWStructuredBufferEntry> rwStructuredBuffers_;
    std::unordered_map<std::string, ConstantBufferEntry> constantBuffers_;
    std::unordered_map<std::string, VertexBufferEntry> vertexBuffers_;
    std::unordered_map<std::string, UAVTextureEntry> uavTextures_;
};

} // namespace KashipanEngine
