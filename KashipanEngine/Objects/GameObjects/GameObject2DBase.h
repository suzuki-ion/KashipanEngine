#pragma once
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include <unordered_map>
#include "Core/Window.h"
#include "Graphics/Renderer.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/Resources.h"
#include "Objects/GameObjects/IGameObjectComponent.h"

namespace KashipanEngine {

/// @brief 2Dゲームオブジェクト基底クラス
class GameObject2DBase {
public:
    GameObject2DBase() = delete;
    GameObject2DBase(const GameObject2DBase &) = delete;
    GameObject2DBase &operator=(const GameObject2DBase &) = delete;
    GameObject2DBase(GameObject2DBase &&) = delete;
    GameObject2DBase &operator=(GameObject2DBase &&) = delete;
    virtual ~GameObject2DBase() = default;

    /// @brief 更新処理（登録済みコンポーネントを更新）
    void Update();
    /// @brief 描画前処理（登録済みコンポーネントの前処理）
    void PreRender();

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
    RenderPassInfo2D CreateRenderPass(Window *targetWindow,
        const std::string &pipelineName,
        const std::string &passName = "GameObject2D Render Pass");

    /// @brief コンポーネントの登録（生成）
    /// @tparam T コンポーネントの型（IGameObjectComponent2Dを継承している必要あり）
    /// @tparam Args コンポーネントのコンストラクタ引数の型
    /// @param args コンポーネントのコンストラクタ引数
    /// @return 登録に成功した場合は true
    template<typename T, typename... Args>
    bool RegisterComponent(Args&&... args) {
        static_assert(std::is_base_of_v<IGameObjectComponent2D, T>, "T must derive from IGameObjectComponent2D");
        try {
            auto comp = std::make_unique<T>(std::forward<Args>(args)...);
            // 登録上限を超えていないか確認
            size_t maxCount = comp->GetMaxComponentCountPerObject();
            size_t existingCount = HasComponents2D(comp->GetComponentType());
            if (existingCount >= maxCount) return false;
            // 登録処理
            const std::string key = comp->GetComponentType();
            components_.push_back(std::move(comp));
            const size_t idx = components_.size() - 1;
            componentsIndexByName_.emplace(key, idx);
            return true;
        } catch (...) { return false; }
    }
    /// @brief 既存コンポーネントの登録
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 登録に成功した場合は true
    bool RegisterComponent(std::unique_ptr<IGameObjectComponent> comp) {
        if (!comp) return false;
        if (dynamic_cast<IGameObjectComponent2D*>(comp.get()) == nullptr) return false;
        // 登録上限を超えていないか確認
        size_t maxCount = comp->GetMaxComponentCountPerObject();
        size_t existingCount = HasComponents2D(comp->GetComponentType());
        if (existingCount >= maxCount) return false;
        // 登録処理
        const std::string key = comp->GetComponentType();
        components_.push_back(std::move(comp));
        const size_t idx = components_.size() - 1;
        componentsIndexByName_.emplace(key, idx);
        return true;
    }
    /// @brief 名前から一致するコンポーネントを取得
    /// @param componentName コンポーネント名
    /// @return 一致するコンポーネントのリスト
    std::vector<IGameObjectComponent2D*> GetComponents2D(const std::string &componentName) const {
        std::vector<IGameObjectComponent2D*> result;
        auto range = componentsIndexByName_.equal_range(componentName);
        for (auto it = range.first; it != range.second; ++it) {
            size_t idx = it->second;
            if (idx < components_.size()) {
                if (auto *p = dynamic_cast<IGameObjectComponent2D*>(components_[idx].get())) result.push_back(p);
            }
        }
        return result;
    }
    /// @brief 名前からコンポーネントの存在を確認
    /// @param componentName コンポーネント名
    /// @return 一致するコンポーネントの個数
    size_t HasComponents2D(const std::string &componentName) const {
        auto range = componentsIndexByName_.equal_range(componentName);
        size_t count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            size_t idx = it->second;
            if (idx < components_.size() && dynamic_cast<IGameObjectComponent2D*>(components_[idx].get())) ++count;
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
    GameObject2DBase(const std::string &name,
        size_t vertexByteSize, size_t indexByteSize,
        size_t vertexCount, size_t indexCount,
        void *initialVertexData = nullptr,
        void *initialIndexData = nullptr);

    /// @brief 更新処理（派生クラスでの独自処理用）
    virtual void OnUpdate() {}

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
    friend class GameObject2DContext;

    std::string name_ = "GameObject2D";
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
