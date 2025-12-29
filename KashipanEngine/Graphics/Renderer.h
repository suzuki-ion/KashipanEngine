#pragma once
#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <windows.h>
#include <optional>
#include <cstdint>
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Graphics/Resources/ConstantBufferResource.h"

namespace KashipanEngine {

class Window;
class DirectXCommon;
class GraphicsEngine;
class PipelineManager;
class Object2DBase;
class Object3DBase;

/// @brief 描画方式
enum class RenderType {
    Standard,   //< 個別に描画
    Instancing, //< 同一オブジェクトをまとめて描画
};

/// @brief 描画次元
enum class RenderDimension {
    D2,
    D3,
};

/// @brief 描画指示用構造体
struct RenderCommand final {
    RenderCommand(const RenderCommand &) = default;
    RenderCommand &operator=(const RenderCommand &) = default;
    RenderCommand(RenderCommand &&) = default;
    RenderCommand &operator=(RenderCommand &&) = default;
private:
    friend class Renderer;
    friend class Object2DBase;
    friend class Object3DBase;
    RenderCommand() = default;
    UINT vertexCount = 0;           //< 頂点数
    UINT indexCount = 0;            //< インデックス数
    UINT instanceCount = 1;         //< インスタンス数
    UINT startVertexLocation = 0;   //< 開始頂点位置
    UINT startIndexLocation = 0;    //< 開始インデックス位置
    INT baseVertexLocation = 0;     //< ベース頂点位置
    UINT startInstanceLocation = 0; //< 開始インスタンス位置
};

/// @brief 描画パス情報構造体
struct RenderPass final {
public:
    RenderPass(const RenderPass &) = default;
    RenderPass &operator=(const RenderPass &) = default;
    RenderPass(RenderPass &&) = default;
    RenderPass &operator=(RenderPass &&) = default;

    struct ConstantBufferRequirement {
        std::string shaderNameKey;
        size_t byteSize = 0;
    };

    struct InstanceBufferRequirement {
        std::string shaderNameKey;
        size_t elementStride = 0;
    };

private:
    friend class Renderer;
    friend class Object2DBase;
    friend class Object3DBase;

    RenderPass(Passkey<Object2DBase>) : dimension(RenderDimension::D2) {}
    RenderPass(Passkey<Object3DBase>) : dimension(RenderDimension::D3) {}

    Window *window = nullptr;   //< 描画先ウィンドウ
    std::string pipelineName;   //< 使用するパイプライン名
    std::string passName;       //< パス名（デバッグ用）

    const RenderDimension dimension;
    RenderType renderType = RenderType::Standard;

    std::uint64_t batchKey = 0;
    std::vector<ConstantBufferRequirement> constantBufferRequirements;
    std::function<bool(void *constantBufferMaps, std::uint32_t instanceCount)> updateConstantBuffersFunction;
    std::vector<InstanceBufferRequirement> instanceBufferRequirements;
    std::function<bool(void *instanceMaps, ShaderVariableBinder &, std::uint32_t instanceIndex)> submitInstanceFunction;
    std::function<bool(ShaderVariableBinder &, std::uint32_t instanceCount)> batchedRenderFunction;
    std::function<std::optional<RenderCommand>(PipelineBinder &)> renderCommandFunction; //< 描画コマンド取得関数
};

/// @brief 描画用のレンダラークラス
class Renderer final {
public:
    struct PersistentPassHandle {
        std::uint64_t id = 0;
        bool IsValid() const { return id != 0; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const PersistentPassHandle &o) const { return id == o.id; }
        bool operator!=(const PersistentPassHandle &o) const { return id != o.id; }
    };

    /// @brief コンストラクタ
    /// @param maxRenderPasses 最大レンダーパス数
    Renderer(Passkey<GraphicsEngine>, size_t maxRenderPasses, DirectXCommon *directXCommon, PipelineManager *pipelineManager)
        : directXCommon_(directXCommon), pipelineManager_(pipelineManager) {
        persistent2DStandard_.reserve(maxRenderPasses);
        persistent3DStandard_.reserve(maxRenderPasses);
    }
    ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /// @brief 永続レンダーパス登録（返り値のハンドルで解除可能）
    PersistentPassHandle RegisterPersistentRenderPass(RenderPass &&pass);

    /// @brief 永続レンダーパス解除
    bool UnregisterPersistentRenderPass(PersistentPassHandle handle);

    /// @brief レンダーパス登録
    void RegisterRenderPass(const RenderPass &pass);

    /// @brief レンダーパス登録（ムーブ）
    void RegisterRenderPass(RenderPass &&pass);

    /// @brief フレーム描画処理
    void RenderFrame(Passkey<GraphicsEngine>);

    /// @brief Windowの登録
    void RegisterWindow(Passkey<Window>, HWND hwnd, ID3D12GraphicsCommandList* commandList);

private:
    void RenderPasses2DStandardPersistent();
    void RenderPasses2DInstancingPersistent();
    void RenderPasses3DStandardPersistent();
    void RenderPasses3DInstancingPersistent();

    /// @brief 描画コマンド発行処理
    void IssueRenderCommand(ID3D12GraphicsCommandList *commandList, const RenderCommand &renderCommand);

    /// @brief DirectX共通クラスへのポインタ
    DirectXCommon *directXCommon_ = nullptr;
    /// @brief パイプラインマネージャーへのポインタ
    PipelineManager *pipelineManager_ = nullptr;

    std::uint64_t nextPersistentPassId_ = 1;

    struct PersistentPassEntry {
        PersistentPassHandle handle;
        RenderPass pass;
    };

    std::unordered_map<std::uint64_t, PersistentPassEntry> persistentPassesById_;

    /// @brief インスタンシング時のバッチ識別用キー
    struct BatchKey {
        HWND hwnd{};
        std::string pipelineName;
        std::uint64_t key = 0;

        bool operator==(const BatchKey &o) const {
            return hwnd == o.hwnd && key == o.key && pipelineName == o.pipelineName;
        }
    };

    /// @brief バッチ識別用キーのハッシュ関数
    struct BatchKeyHasher {
        size_t operator()(const BatchKey &k) const noexcept {
            size_t h1 = std::hash<void *>{}(k.hwnd);
            size_t h2 = std::hash<std::uint64_t>{}(k.key);
            size_t h3 = std::hash<std::string>{}(k.pipelineName);
            size_t h = h1;
            h ^= (h2 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            h ^= (h3 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            return h;
        }
    };

    // Persistent buckets (split at registration time)
    std::vector<const RenderPass*> persistent2DStandard_;
    std::vector<const RenderPass*> persistent3DStandard_;
    std::unordered_map<BatchKey, std::vector<const RenderPass*>, BatchKeyHasher> persistent2DInstancing_;
    std::unordered_map<BatchKey, std::vector<const RenderPass*>, BatchKeyHasher> persistent3DInstancing_;

    /// @brief ウィンドウごとのPipelineBinder
    std::unordered_map<HWND, PipelineBinder> windowBinders_;

    /// @brief インスタンシング用バッファ識別用キー
    struct InstanceBufferKey {
        HWND hwnd{};
        std::string pipelineName;
        std::uint64_t batchKey = 0;
        std::string shaderNameKey;
        size_t elementStride = 0;

        bool operator==(const InstanceBufferKey &o) const {
            return hwnd == o.hwnd && batchKey == o.batchKey && elementStride == o.elementStride
                && pipelineName == o.pipelineName && shaderNameKey == o.shaderNameKey;
        }
    };
    /// @brief インスタンシング用バッファ識別用キーのハッシュ関数
    struct InstanceBufferKeyHasher {
        size_t operator()(const InstanceBufferKey &k) const noexcept {
            size_t h1 = std::hash<void *>{}(k.hwnd);
            size_t h2 = std::hash<std::uint64_t>{}(k.batchKey);
            size_t h3 = std::hash<std::string>{}(k.pipelineName);
            size_t h4 = std::hash<std::string>{}(k.shaderNameKey);
            size_t h5 = std::hash<size_t>{}(k.elementStride);
            size_t h = h1;
            h ^= (h2 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            h ^= (h3 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            h ^= (h4 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            h ^= (h5 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            return h;
        }
    };
    /// @brief インスタンシング用バッファエントリ
    struct InstanceBufferEntry {
        std::unique_ptr<StructuredBufferResource> buffer;
        size_t capacity = 0;
    };

    /// @brief 定数バッファ識別用キー
    struct ConstantBufferKey {
        HWND hwnd{};
        std::string pipelineName;
        std::uint64_t batchKey = 0;
        std::string shaderNameKey;
        size_t byteSize = 0;

        bool operator==(const ConstantBufferKey &o) const {
            return hwnd == o.hwnd && batchKey == o.batchKey && byteSize == o.byteSize
                && pipelineName == o.pipelineName && shaderNameKey == o.shaderNameKey;
        }
    };
    /// @brief 定数バッファ識別用キーのハッシュ関数
    struct ConstantBufferKeyHasher {
        size_t operator()(const ConstantBufferKey &k) const noexcept {
            size_t h1 = std::hash<void *>{}(k.hwnd);
            size_t h2 = std::hash<std::uint64_t>{}(k.batchKey);
            size_t h3 = std::hash<std::string>{}(k.pipelineName);
            size_t h4 = std::hash<std::string>{}(k.shaderNameKey);
            size_t h5 = std::hash<size_t>{}(k.byteSize);
            size_t h = h1;
            h ^= (h2 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            h ^= (h3 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            h ^= (h4 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            h ^= (h5 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            return h;
        }
    };
    /// @brief 定数バッファエントリ
    struct ConstantBufferEntry {
        std::unique_ptr<ConstantBufferResource> buffer;
        size_t byteSize = 0;
    };

    /// @brief 定数バッファマップ
    std::unordered_map<ConstantBufferKey, ConstantBufferEntry, ConstantBufferKeyHasher> constantBuffers_;
    /// @brief インスタンシング用バッファマップ
    std::unordered_map<InstanceBufferKey, InstanceBufferEntry, InstanceBufferKeyHasher> instanceBuffers_;
};

} // namespace KashipanEngine
