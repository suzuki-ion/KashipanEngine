#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
        Vector3 tangent;
    };

    /// @brief メッシュ用GPUバッファ
    struct MeshBuffers {
        std::unique_ptr<VertexBufferResource> vertexBuffer;
        std::unique_ptr<IndexBufferResource> indexBuffer;
        std::uint32_t vertexCount = 0;
        std::uint32_t indexCount = 0;
        /// @brief ローカル空間の境界球（シャドウマップのスライス単位カリング等に使用）
        Vector3 boundsCenter{ 0.0f, 0.0f, 0.0f };
        float boundsRadius = 0.0f;
        /// @brief 生成元にしたModelDataの版数（ModelManager::GetModelDataVersion）。
        ///        TilemapRenderer等がUpdateProceduralMeshでCPU側データを書き換えた際、
        ///        このキャッシュが古いままにならないよう再構築要否の判定に使う
        std::uint64_t sourceDataVersion = 0;
    };

    ResourceContainer() = default;
    ~ResourceContainer() = default;

    ResourceContainer(const ResourceContainer &) = delete;
    ResourceContainer &operator=(const ResourceContainer &) = delete;
    ResourceContainer(ResourceContainer &&) = delete;
    ResourceContainer &operator=(ResourceContainer &&) = delete;

    /// @brief メッシュハンドルからGPUバッファを取得（未作成、またはModelManager::UpdateProceduralMesh等で
    ///        CPU側データが更新された後の場合はモデルデータから作り直す）
    /// @return メッシュバッファ（生成に失敗した場合は nullptr）
    const MeshBuffers *GetOrCreateMeshBuffers(ModelManager::ModelHandle meshHandle) {
        const auto currentVersion = ModelManager::GetModelDataVersion(meshHandle);
        auto it = meshBuffers_.find(meshHandle);
        if (it != meshBuffers_.end() && it->second->sourceDataVersion == currentVersion) return it->second.get();

        const auto &modelData = ModelManager::GetModelData(meshHandle);
        if (modelData.GetVertexCount() == 0 || modelData.GetIndexCount() == 0) return nullptr;

        std::vector<MeshVertex> vertices(modelData.GetVertexCount());
        const auto &srcVertices = modelData.GetVertices();
        for (size_t i = 0; i < vertices.size(); ++i) {
            const auto &src = srcVertices[i];
            vertices[i].position = Vector4(src.px, src.py, src.pz, 1.0f);
            vertices[i].texcoord = Vector2(src.u, src.v);
            vertices[i].normal = Vector3(src.nx, src.ny, src.nz);
            vertices[i].tangent = Vector3(src.tx, src.ty, src.tz);
        }
        const auto &srcIndices = modelData.GetIndices();

        auto buffers = std::make_unique<MeshBuffers>();
        buffers->sourceDataVersion = currentVersion;
        buffers->vertexCount = modelData.GetVertexCount();
        buffers->indexCount = modelData.GetIndexCount();
        buffers->vertexBuffer = std::make_unique<VertexBufferResource>(
            sizeof(MeshVertex) * vertices.size(), vertices.data());
        buffers->indexBuffer = std::make_unique<IndexBufferResource>(
            sizeof(std::uint32_t) * srcIndices.size(), DXGI_FORMAT_R32_UINT, srcIndices.data());

        // ローカル空間の境界球を計算する（AABBの中心を球の中心とし、頂点までの最大距離を半径とする）
        {
            Vector3 minP{ vertices[0].position.x, vertices[0].position.y, vertices[0].position.z };
            Vector3 maxP = minP;
            for (const auto &v : vertices) {
                minP.x = std::min(minP.x, v.position.x);
                minP.y = std::min(minP.y, v.position.y);
                minP.z = std::min(minP.z, v.position.z);
                maxP.x = std::max(maxP.x, v.position.x);
                maxP.y = std::max(maxP.y, v.position.y);
                maxP.z = std::max(maxP.z, v.position.z);
            }
            const Vector3 center{ (minP.x + maxP.x) * 0.5f, (minP.y + maxP.y) * 0.5f, (minP.z + maxP.z) * 0.5f };
            float radiusSq = 0.0f;
            for (const auto &v : vertices) {
                const float dx = v.position.x - center.x;
                const float dy = v.position.y - center.y;
                const float dz = v.position.z - center.z;
                radiusSq = std::max(radiusSq, dx * dx + dy * dy + dz * dz);
            }
            buffers->boundsCenter = center;
            buffers->boundsRadius = std::sqrt(radiusSq);
        }

        auto *raw = buffers.get();
        // 既存キャッシュが古いデータ（sourceDataVersion不一致）で残っている場合はここで置き換える
        meshBuffers_.insert_or_assign(meshHandle, std::move(buffers));
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

    /// @brief キーに対応する構造化バッファを取得し、内容(data, elementStride*elementCount バイト分)が
    ///        前回このキーでアップロードした内容と完全に一致する場合はMap+memcpyを省略する
    /// @details 動かない静的オブジェクトのみで構成されるバッチ（ワールド行列・マテリアル定数が
    ///          毎フレーム同一）で、GPUへの再アップロードを丸ごと省略できるようにするためのもの。
    ///          内容が変化するバッチでは比較コストが上乗せされるが、対象は元々毎フレームMap+memcpy
    ///          していた小〜中規模の構造化バッファのみなので影響は小さい。
    /// @return 対応するバッファ（内容不一致・新規作成時のみ実際に書き込み、一致すれば書き込まず返す）
    StructuredBufferResource *GetOrUpdateStructuredBuffer(const std::string &key, size_t elementStride, size_t elementCount, const void *data) {
        auto *buffer = GetOrCreateStructuredBuffer(key, elementStride, elementCount);
        if (!buffer || !data || elementCount == 0) return buffer;

        const size_t byteSize = elementStride * elementCount;
        auto &cached = uploadCache_[key];
        if (cached.size() == byteSize && std::memcmp(cached.data(), data, byteSize) == 0) {
            return buffer;
        }

        if (auto *mapped = buffer->Map()) {
            std::memcpy(mapped, data, byteSize);
        }
        cached.resize(byteSize);
        std::memcpy(cached.data(), data, byteSize);
        return buffer;
    }

    /// @brief 保持している全リソースを解放する
    void Clear() {
        meshBuffers_.clear();
        structuredBuffers_.clear();
        rwStructuredBuffers_.clear();
        constantBuffers_.clear();
        vertexBuffers_.clear();
        uavTextures_.clear();
        uploadCache_.clear();
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
    /// @brief GetOrUpdateStructuredBuffer用: キーごとの直近アップロード内容のCPU側コピー
    std::unordered_map<std::string, std::vector<std::byte>> uploadCache_;
};

} // namespace KashipanEngine
