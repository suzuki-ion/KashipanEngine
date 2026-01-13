#pragma once
#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <windows.h>
#include <optional>
#include <cstdint>
#include <array>
#if defined(USE_IMGUI)
struct ImGuiContext;
#endif
#include "Math/Matrix4x4.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Graphics/Resources/ConstantBufferResource.h"

namespace KashipanEngine {

class RendererCpuTimerScope;

class Window;
class DirectXCommon;
class GraphicsEngine;
class PipelineManager;
class Object2DBase;
class Object3DBase;
class ScreenBuffer;
class ShadowMapBuffer;
class IPostEffectComponent;

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
    friend class IPostEffectComponent;
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
    friend class ScreenBuffer;
    friend class ShadowMapBuffer;

    RenderPass(Passkey<Object2DBase>) : dimension(RenderDimension::D2) {}
    RenderPass(Passkey<Object3DBase>) : dimension(RenderDimension::D3) {}
    RenderPass(Passkey<ScreenBuffer>, RenderDimension dim) : dimension(dim) {}

    Window *window = nullptr;          //< 描画先ウィンドウ（Window 描画の場合）
    ScreenBuffer *screenBuffer = nullptr; //< 描画先スクリーンバッファ（ScreenBuffer 描画の場合）
    ShadowMapBuffer *shadowMapBuffer = nullptr; //< 描画先シャドウマップバッファ（ShadowMapBuffer 描画の場合）
    std::string pipelineName;          //< 使用するパイプライン名
    std::string passName;              //< パス名（デバッグ用）

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

/// @brief PostEffect 用描画パス情報構造体
struct PostEffectPass final {
public:
    PostEffectPass() = default;

    std::string pipelineName;
    std::string passName;
    std::uint64_t batchKey = 0;

    std::vector<RenderPass::ConstantBufferRequirement> constantBufferRequirements;
    std::function<bool(void *constantBufferMaps, std::uint32_t instanceCount)> updateConstantBuffersFunction;
    std::vector<RenderPass::InstanceBufferRequirement> instanceBufferRequirements;
    std::function<bool(void *instanceMaps, ShaderVariableBinder &, std::uint32_t instanceIndex)> submitInstanceFunction;
    /// @brief 描画前に必要な SRV/CBV/Sampler 等をバインド
    std::function<bool(ShaderVariableBinder &, std::uint32_t instanceCount)> batchedRenderFunction;
    /// @brief 描画コマンド生成（フルスクリーン三角形等）
    std::function<std::optional<RenderCommand>(PipelineBinder &)> renderCommandFunction;
};

/// @brief ScreenBuffer 用描画パス情報構造体
struct ScreenBufferPass final {
public:
    ScreenBufferPass(const ScreenBufferPass&) = default;
    ScreenBufferPass& operator=(const ScreenBufferPass&) = default;
    ScreenBufferPass(ScreenBufferPass&&) = default;
    ScreenBufferPass& operator=(ScreenBufferPass&&) = default;

private:
    friend class Renderer;
    friend class ScreenBuffer;

    ScreenBufferPass(Passkey<ScreenBuffer>) {}
    ScreenBuffer *screenBuffer = nullptr;
    std::string passName;

    RenderType renderType = RenderType::Standard;
    std::uint64_t batchKey = 0;

    std::vector<RenderPass::ConstantBufferRequirement> constantBufferRequirements;
    std::function<bool(void *constantBufferMaps, std::uint32_t instanceCount)> updateConstantBuffersFunction;
    std::vector<RenderPass::InstanceBufferRequirement> instanceBufferRequirements;
    std::function<bool(void *instanceMaps, ShaderVariableBinder &, std::uint32_t instanceIndex)> submitInstanceFunction;
    std::function<bool(ShaderVariableBinder &, std::uint32_t instanceCount)> batchedRenderFunction;
};

/// @brief 描画用のレンダラークラス
class Renderer final {
public:
    struct CpuTimerStats {
        struct Sample {
            double lastMs = 0.0;
            double avgMs = 0.0;
            std::uint64_t count = 0;
        };

        enum class Scope : std::uint32_t {
            RenderFrame = 0,

            ShadowMap_AllBeginRecord,
            ShadowMap_Passes,
            ShadowMap_AllEndRecord,
            ShadowMap_Execute,

            Offscreen_AllBeginRecord,
            Offscreen_Passes,
            Offscreen_AllEndRecord,
            Offscreen_Execute,

            PostEffect_AllBeginRecord,
            PostEffect_Passes,
            PostEffect_AllEndRecord,
            PostEffect_Execute,

            Persistent_Passes,

            Standard_Total,
            Standard_ConstantBuffer_Update,
            Standard_ConstantBuffer_Bind,
            Standard_InstanceBuffer_Update,
            Standard_RenderCommand,

            Instancing_Total,
            Instancing_ConstantBuffer_Update,
            Instancing_ConstantBuffer_Bind,
            Instancing_InstanceBuffer_MapBind,
            Instancing_SubmitInstances,
            Instancing_RenderCommand,

            Count
        };

        std::array<Sample, static_cast<std::size_t>(Scope::Count)> samples{};
        std::uint32_t avgWindow = 60;
    };

    const CpuTimerStats &GetCpuTimerStats() const noexcept { return cpuTimerStats_; }
    void SetCpuTimerAverageWindow(std::uint32_t frames) noexcept;

#if defined(USE_IMGUI)
    void ShowImGuiCpuTimersWindow();
#endif

    struct PersistentPassHandle {
        std::uint64_t id = 0;
        bool IsValid() const { return id != 0; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const PersistentPassHandle &o) const { return id == o.id; }
        bool operator!=(const PersistentPassHandle &o) const { return id != o.id; }
    };

    struct PersistentScreenPassHandle {
        std::uint64_t id = 0;
        bool IsValid() const { return id != 0; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const PersistentScreenPassHandle &o) const { return id == o.id; }
        bool operator!=(const PersistentScreenPassHandle &o) const { return id != o.id; }
    };

    struct PersistentOffscreenPassHandle {
        std::uint64_t id = 0;
        bool IsValid() const { return id != 0; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const PersistentOffscreenPassHandle &o) const { return id == o.id; }
        bool operator!=(const PersistentOffscreenPassHandle &o) const { return id != o.id; }
    };

    struct PersistentShadowMapPassHandle {
        std::uint64_t id = 0;
        bool IsValid() const { return id != 0; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const PersistentShadowMapPassHandle &o) const { return id == o.id; }
        bool operator!=(const PersistentShadowMapPassHandle &o) const { return id != o.id; }
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

    /// @brief ScreenBuffer 向け永続描画パス登録
    PersistentScreenPassHandle RegisterPersistentScreenPass(ScreenBufferPass&& pass);
    /// @brief ScreenBuffer 向け永続描画パス解除
    bool UnregisterPersistentScreenPass(PersistentScreenPassHandle handle);

    /// @brief ScreenBuffer 宛ての永続 RenderPass 登録
    PersistentOffscreenPassHandle RegisterPersistentOffscreenRenderPass(RenderPass &&pass);
    /// @brief ScreenBuffer 宛ての永続 RenderPass 解除
    bool UnregisterPersistentOffscreenRenderPass(PersistentOffscreenPassHandle handle);

    /// @brief ShadowMapBuffer 宛ての永続 RenderPass 登録
    PersistentShadowMapPassHandle RegisterPersistentShadowMapRenderPass(RenderPass &&pass);
    /// @brief ShadowMapBuffer 宛ての永続 RenderPass 解除
    bool UnregisterPersistentShadowMapRenderPass(PersistentShadowMapPassHandle handle);

    /// @brief フレーム描画処理
    void RenderFrame(Passkey<GraphicsEngine>);

    /// @brief Windowの登録
    void RegisterWindow(Passkey<Window>, HWND hwnd, ID3D12GraphicsCommandList* commandList);

    struct ShadowMapGlobals {
        ShadowMapBuffer* buffer = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE sampler{};
    };

private:
    struct PersistentPassEntry {
        PersistentPassHandle handle;
        RenderPass pass;
    };

    struct PersistentScreenPassEntry {
        PersistentScreenPassHandle handle;
        ScreenBufferPass pass;
    };

    struct PersistentOffscreenPassEntry {
        PersistentOffscreenPassHandle handle;
        RenderPass pass;
    };

    struct PersistentShadowMapPassEntry {
        PersistentShadowMapPassHandle handle;
        RenderPass pass;
    };

    /// @brief インスタンシング時のバッチ識別用キー（Window/ScreenBuffer 共通）
    struct BatchKey {
        const void *targetKey{}; // HWND または ScreenBuffer ポインタ等
        std::string pipelineName;
        std::uint64_t key = 0;

        bool operator==(const BatchKey &o) const {
            return targetKey == o.targetKey && key == o.key && pipelineName == o.pipelineName;
        }
    };

    /// @brief バッチ識別用キーのハッシュ関数
    struct BatchKeyHasher {
        size_t operator()(const BatchKey &k) const noexcept {
            size_t h1 = std::hash<const void *>{}(k.targetKey);
            size_t h2 = std::hash<std::uint64_t>{}(k.key);
            size_t h3 = std::hash<std::string>{}(k.pipelineName);
            size_t h = h1;
            h ^= (h2 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            h ^= (h3 + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
            return h;
        }
    };

    /// @brief インスタンシング用バッファ識別用キー
    struct InstanceBufferKey {
        const void *targetKey{};
        std::string pipelineName;
        std::uint64_t batchKey = 0;
        std::string shaderNameKey;
        size_t elementStride = 0;

        bool operator==(const InstanceBufferKey &o) const {
            return targetKey == o.targetKey && batchKey == o.batchKey && elementStride == o.elementStride
                && pipelineName == o.pipelineName && shaderNameKey == o.shaderNameKey;
        }
    };
    /// @brief インスタンシング用バッファ識別用キーのハッシュ関数
    struct InstanceBufferKeyHasher {
        size_t operator()(const InstanceBufferKey &k) const noexcept {
            size_t h1 = std::hash<const void *>{}(k.targetKey);
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
        const void *targetKey{};
        std::string pipelineName;
        std::uint64_t batchKey = 0;
        std::string shaderNameKey;
        size_t byteSize = 0;

        bool operator==(const ConstantBufferKey &o) const {
            return targetKey == o.targetKey && batchKey == o.batchKey && byteSize == o.byteSize
                && pipelineName == o.pipelineName && shaderNameKey == o.shaderNameKey;
        }
    };
    /// @brief 定数バッファ識別用キーのハッシュ関数
    struct ConstantBufferKeyHasher {
        size_t operator()(const ConstantBufferKey &k) const noexcept {
            size_t h1 = std::hash<const void *>{}(k.targetKey);
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

private:
    void BeginCpuTimerFrame_() noexcept;
    void AddCpuTimerSample_(CpuTimerStats::Scope scope, double ms) noexcept;

    CpuTimerStats cpuTimerStats_{};

    friend class ::KashipanEngine::RendererCpuTimerScope;

    void RenderOffscreenPasses();
    void RenderScreenPasses();
    void RenderPersistentPasses();
    void RenderShadowMapPasses();

    void Render2DStandard(std::vector<const RenderPass *> renderPasses, std::function<void *(const RenderPass *)> getTargetKeyFunc);
    void Render2DInstancing(std::unordered_map<BatchKey, std::vector<const RenderPass *>, BatchKeyHasher> &renderPasses, std::function<void *(const RenderPass *)> getTargetKeyFunc);
    void Render3DStandard(std::vector<const RenderPass *> renderPasses, std::function<void *(const RenderPass *)> getTargetKeyFunc);
    void Render3DInstancing(std::unordered_map<BatchKey, std::vector<const RenderPass *>, BatchKeyHasher> &renderPasses, std::function<void *(const RenderPass *)> getTargetKeyFunc);
    
    /// @brief 描画コマンド発行処理
    void IssueRenderCommand(ID3D12GraphicsCommandList *commandList, const RenderCommand &renderCommand);

    /// @brief DirectX共通クラスへのポインタ
    DirectXCommon *directXCommon_ = nullptr;
    /// @brief パイプラインマネージャーへのポインタ
    PipelineManager *pipelineManager_ = nullptr;

    std::uint64_t nextPersistentPassId_ = 1;
    std::uint64_t nextPersistentScreenPassId_ = 1;
    std::uint64_t nextPersistentOffscreenPassId_ = 1;
    std::uint64_t nextPersistentShadowMapPassId_ = 1;

    std::unordered_map<std::uint64_t, PersistentPassEntry> persistentPassesById_;
    std::unordered_map<std::uint64_t, PersistentScreenPassEntry> persistentScreenPassesById_;
    std::unordered_map<std::uint64_t, PersistentOffscreenPassEntry> persistentOffscreenPassesById_;
    std::unordered_map<std::uint64_t, PersistentShadowMapPassEntry> persistentShadowMapPassesById_;

    std::vector<const ScreenBufferPass*> persistentScreenPasses_;

    std::vector<const RenderPass*> persistent2DStandard_;
    std::vector<const RenderPass*> persistent3DStandard_;
    std::unordered_map<BatchKey, std::vector<const RenderPass*>, BatchKeyHasher> persistent2DInstancing_;
    std::unordered_map<BatchKey, std::vector<const RenderPass*>, BatchKeyHasher> persistent3DInstancing_;

    std::vector<const RenderPass*> offscreen2DStandard_;
    std::vector<const RenderPass*> offscreen3DStandard_;
    std::unordered_map<BatchKey, std::vector<const RenderPass*>, BatchKeyHasher> offscreen2DInstancing_;
    std::unordered_map<BatchKey, std::vector<const RenderPass*>, BatchKeyHasher> offscreen3DInstancing_;

    std::vector<const RenderPass*> shadowMap2DStandard_;
    std::vector<const RenderPass*> shadowMap3DStandard_;
    std::unordered_map<BatchKey, std::vector<const RenderPass*>, BatchKeyHasher> shadowMap2DInstancing_;
    std::unordered_map<BatchKey, std::vector<const RenderPass*>, BatchKeyHasher> shadowMap3DInstancing_;

    /// @brief ウィンドウごとのPipelineBinder
    std::unordered_map<HWND, PipelineBinder> windowBinders_;

    /// @brief 定数バッファマップ
    std::unordered_map<ConstantBufferKey, ConstantBufferEntry, ConstantBufferKeyHasher> constantBuffers_;
    /// @brief インスタンシング用バッファマップ
    std::unordered_map<InstanceBufferKey, InstanceBufferEntry, InstanceBufferKeyHasher> instanceBuffers_;
};

} // namespace KashipanEngine
