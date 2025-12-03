#pragma once
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>
#include "Core/Window.h"
#include "Graphics/Renderer.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/Resources.h"
#include "Objects/GameObjects/IGameObjectComponent.h"

namespace KashipanEngine {

/// @brief 3Dゲームオブジェクト基底クラス
class GameObject3DBase {
public:
    GameObject3DBase() = delete;

    GameObject3DBase(const GameObject3DBase &) = delete;
    GameObject3DBase &operator=(const GameObject3DBase &) = delete;
    GameObject3DBase(GameObject3DBase &&) = delete;
    GameObject3DBase &operator=(GameObject3DBase &&) = delete;
    virtual ~GameObject3DBase() = default;

    /// @brief 頂点の値の設定
    template<typename T>
    void SetVertexData(const std::span<T> &data) {
        if (!vertexData_ || data.size() > vertexCount_) return;
        std::memcpy(vertexData_, data.data(), sizeof(T) * data.size());
    }
    /// @brief 頂点の値の取得
    template<typename T>
    std::span<T> GetVertexData() const {
        return GetVertexSpan<T>();
    }

    /// @brief オブジェクト名の設定
    void SetName(const std::string &name) { name_ = name; }
    /// @brief オブジェクト名の取得
    const std::string &GetName() const { return name_; }

    /// @brief レンダーパスの作成
    RenderPassInfo3D CreateRenderPass(Window *targetWindow,
        const std::string &pipelineName,
        const std::string &passName = "GameObject3D Render Pass");

    /// @brief コンポーネント登録（生成）
    template<typename T, typename... Args>
    bool RegisterComponent(Args&&... args) {
        static_assert(std::is_base_of_v<IGameObjectComponent3D, T>, "T must derive from IGameObjectComponent3D");
        try {
            auto comp = std::make_unique<T>(std::forward<Args>(args)...);
            const std::string key = comp->GetComponentType();
            components_.push_back(std::move(comp));
            const size_t idx = components_.size() - 1;
            componentsIndexByName_.emplace(key, idx);
            return true;
        } catch (...) { return false; }
    }
    /// @brief コンポーネント登録（既存）
    bool RegisterComponent(std::unique_ptr<IGameObjectComponent> comp) {
        if (!comp) return false;
        if (dynamic_cast<IGameObjectComponent3D*>(comp.get()) == nullptr) return false;
        const std::string key = comp->GetComponentType();
        components_.push_back(std::move(comp));
        const size_t idx = components_.size() - 1;
        componentsIndexByName_.emplace(key, idx);
        return true;
    }
    /// @brief 名前でコンポーネント取得（3D）
    std::vector<IGameObjectComponent3D*> GetComponents3D(const std::string &componentName) const {
        std::vector<IGameObjectComponent3D*> result;
        auto range = componentsIndexByName_.equal_range(componentName);
        for (auto it = range.first; it != range.second; ++it) {
            size_t idx = it->second;
            if (idx < components_.size()) {
                if (auto *p = dynamic_cast<IGameObjectComponent3D*>(components_[idx].get())) result.push_back(p);
            }
        }
        return result;
    }
    /// @brief 名前で個数チェック（3D）
    size_t HasComponents3D(const std::string &componentName) const {
        auto range = componentsIndexByName_.equal_range(componentName);
        size_t count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            size_t idx = it->second;
            if (idx < components_.size() && dynamic_cast<IGameObjectComponent3D*>(components_[idx].get())) ++count;
        }
        return count;
    }

protected:
    /// @brief コンストラクタ
    /// @param name オブジェクト名
    /// @param vertexByteSize 1頂点あたりのサイズ（バイト単位）
    /// @param indexByteSize 1インデックスあたりのサイズ（バイト単位）
    /// @param vertexCount 頂点数
    /// @param indexCount インデックス数
    /// @param initialVertexData 初期頂点データ（nullptrの場合は未初期化）
    /// @param initialIndexData 初期インデックスデータ（nullptrの場合は未初期化）
    GameObject3DBase(const std::string &name,
        size_t vertexByteSize, size_t indexByteSize,
        size_t vertexCount, size_t indexCount,
        void *initialVertexData = nullptr,
        void *initialIndexData = nullptr);

    /// @brief シェーダ変数設定処理 (true を返した場合のみ描画コマンド生成へ進む)
    /// @param shaderBinder シェーダ変数バインダー
    /// @return 描画する場合は true
    virtual bool Render(ShaderVariableBinder &shaderBinder) { (void)shaderBinder; return true; }

    /// @brief 描画コマンド生成処理
    /// @param pipelineBinder パイプラインバインダー
    /// @return 描画コマンド (生成しない場合は std::nullopt)
    virtual std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder);

    /// @brief 頂点数取得
    UINT GetVertexCount() const { return vertexCount_; }
    /// @brief インデックス数取得
    UINT GetIndexCount() const { return indexCount_; }
    /// @brief 頂点マップデータ取得
    template<typename T>
    std::span<T> GetVertexSpan() const {
        if (!vertexData_ || vertexCount_ == 0) return {};
        return std::span<T>(static_cast<T *>(vertexData_), vertexCount_);
    }
    /// @brief インデックスマップデータ取得
    template<typename T>
    std::span<T> GetIndexSpan() const {
        if (!indexData_ || indexCount_ == 0) return {};
        return std::span<T>(static_cast<T *>(indexData_), indexCount_);
    }

    /// @brief 頂点バッファの設定
    /// @param pipelineBinder パイプラインバインダー
    void SetVertexBuffer(PipelineBinder &pipelineBinder) {
        if (vertexBuffer_) {
            pipelineBinder.SetVertexBufferView(0, 1, &vertexBufferView_);
        }
    }
    /// @brief インデックスバッファの設定
    /// @param pipelineBinder パイプラインバインダー
    void SetIndexBuffer(PipelineBinder &pipelineBinder) {
        if (indexBuffer_) {
            pipelineBinder.SetIndexBufferView(&indexBufferView_);
        }
    }

    /// @brief デフォルト描画コマンド生成ヘルパー
    RenderCommand CreateDefaultRenderCommand() const;

private:
    friend class GameObject3DContext;

    std::string name_ = "GameObject3D";
    UINT vertexCount_ = 0;
    UINT indexCount_ = 0;
    std::unique_ptr<VertexBufferResource> vertexBuffer_;
    std::unique_ptr<IndexBufferResource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    void *vertexData_ = nullptr;
    void *indexData_ = nullptr;
    std::vector<std::unique_ptr<IGameObjectComponent>> components_;
    std::unordered_multimap<std::string, size_t> componentsIndexByName_;
};

} // namespace KashipanEngine
