#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <imgui.h>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "Core/Window.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/Renderer.h"
#include "Graphics/Resources.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector4.h"
#include "Objects/IObjectComponent.h"
#include "../../MyStd/AnyUnorderedMap.h"

namespace KashipanEngine {

class Object2DContext;
class ScreenBuffer;

/// @brief 2Dオブジェクト基底クラス
class Object2DBase {
public:
    struct RenderPassRegistrationHandle {
        std::uint64_t value = 0;
        bool IsValid() const noexcept { return value != 0; }
        explicit operator bool() const noexcept { return IsValid(); }
        bool operator==(const RenderPassRegistrationHandle &o) const noexcept { return value == o.value; }
        bool operator!=(const RenderPassRegistrationHandle &o) const noexcept { return value != o.value; }
    };

    enum class RenderTargetKind {
        Window,
        ScreenBuffer,
    };

    struct RenderPassRegistrationInfo {
        RenderPassRegistrationHandle handle{};
        RenderTargetKind targetKind = RenderTargetKind::Window;
        Window *window = nullptr;
        ScreenBuffer *screenBuffer = nullptr;
        std::string pipelineName;
    };

    Object2DBase() = delete;
    Object2DBase(const Object2DBase &) = delete;
    Object2DBase &operator=(const Object2DBase &) = delete;
    Object2DBase(Object2DBase &&) = delete;
    Object2DBase &operator=(Object2DBase &&) = delete;
    virtual ~Object2DBase();

    /// @brief 更新処理（登録済みコンポーネントを更新）
    void Update();
    /// @brief 描画処理（登録済みコンポーネントの描画処理）
    void Render();

#if defined(USE_IMGUI)
    /// @brief ImGui 表示（ウィンドウの Begin/End は呼ばない）
    void ShowImGui();
#endif

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

    /// @brief 任意データ領域（アプリ側の動的変数管理用）
    MyStd::AnyUnorderedMap &UserData() { return userData_; }
    const MyStd::AnyUnorderedMap &UserData() const { return userData_; }

    /// @brief 頂点数取得
    UINT GetVertexCount() const { return vertexCount_; }
    /// @brief インデックス数取得
    UINT GetIndexCount() const { return indexCount_; }

    /// @brief 描画先/パイプラインが確定したタイミングで永続レンダーパスを登録
    /// @return 登録したパスのハンドル（解除や情報取得に使用）
    RenderPassRegistrationHandle AttachToRenderer(Window *targetWindow, const std::string &pipelineName);

    /// @brief 描画先/パイプラインが確定したタイミングで永続オフスクリーンレンダーパスを登録
    /// @return 登録したパスのハンドル（解除や情報取得に使用）
    RenderPassRegistrationHandle AttachToRenderer(ScreenBuffer *targetBuffer, const std::string &pipelineName);

    /// @brief 永続レンダーパス登録を解除（全て）
    void DetachFromRenderer();

    /// @brief 指定ハンドルの永続レンダーパス登録を解除
    bool DetachFromRenderer(RenderPassRegistrationHandle handle);

    /// @brief 現在保持している描画パス登録情報一覧を取得
    std::vector<RenderPassRegistrationInfo> GetRenderPassRegistrations() const;

    /// @brief 指定ハンドルの描画パス登録情報を取得
    std::optional<RenderPassRegistrationInfo> GetRenderPassRegistration(RenderPassRegistrationHandle handle) const;

    /// @brief バッチキーを個別のものに設定（同じパイプラインを使う他オブジェクトとバッチングされなくなる）
    void SetUniqueBatchKey();

    /// @brief コンポーネントの登録（生成）
    /// @tparam T コンポーネントの型（IGameObjectComponent2Dを継承している必要あり）
    /// @tparam Args コンポーネントのコンストラクタ引数の型
    /// @param args コンポーネントのコンストラクタ引数
    /// @return 登録に成功した場合は true
    template<typename T, typename... Args>
    bool RegisterComponent(Args&&... args) {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        try {
            auto comp = std::make_unique<T>(std::forward<Args>(args)...);
            return RegisterComponent(std::move(comp));
        } catch (...) { return false; }
    }
    /// @brief 既存コンポーネントの登録
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 登録に成功した場合は true
    bool RegisterComponent(std::unique_ptr<IObjectComponent> comp);

    //==================================================
    // コンポーネント取得系メソッド
    //==================================================

    /// @brief 名前から一致するコンポーネントを取得
    /// @param componentName コンポーネント名
    /// @return 一致するコンポーネントのリスト
    std::vector<IObjectComponent2D*> GetComponents2D(const std::string &componentName) const {
        std::vector<IObjectComponent2D*> result;
        auto range = components2DIndexByName_.equal_range(componentName);
        for (auto it = range.first; it != range.second; ++it) {
            size_t idx = it->second;
            if (idx < components2D_.size()) {
                result.push_back(components2D_[idx]);
            }
        }
        return result;
    }

    /// @brief 名前から一致するコンポーネントを取得（型付き）
    /// @tparam T 取得したいコンポーネント型（例: Transform2D）
    /// @param componentName コンポーネント名
    /// @return 一致するコンポーネントのリスト
    template<typename T>
    std::vector<T*> GetComponents2D(const std::string &componentName) const {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        auto baseList = GetComponents2D(componentName);
        std::vector<T*> result;
        result.reserve(baseList.size());
        for (auto *c : baseList) {
            result.push_back(static_cast<T*>(c));
        }
        return result;
    }

    /// @brief 型から一致するコンポーネントを取得
    /// @tparam T コンポーネントの型（例: Transform2D）
    /// @return 一致するコンポーネントのリスト
    template<typename T>
    std::vector<T*> GetComponents2D() const {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        std::vector<T*> result;
        const auto range = components2DIndexByType_.equal_range(std::type_index(typeid(T)));
        for (auto it = range.first; it != range.second; ++it) {
            const size_t idx = it->second;
            if (idx < components2D_.size()) {
                result.push_back(static_cast<T*>(components2D_[idx]));
            }
        }
        return result;
    }

    /// @brief 名前から一致する最初のコンポーネントを取得
    /// @param componentName コンポーネント名
    /// @return 一致するコンポーネント（存在しない場合は nullptr）
    IObjectComponent2D *GetComponent2D(const std::string &componentName) const {
        auto range = components2DIndexByName_.equal_range(componentName);
        for (auto it = range.first; it != range.second; ++it) {
            size_t idx = it->second;
            if (idx < components2D_.size()) {
                return components2D_[idx];
            }
        }
        return nullptr;
    }

    /// @brief 名前から一致する最初のコンポーネントを取得（型付き）
    /// @tparam T 取得したいコンポーネント型（例: Transform2D）
    /// @param componentName コンポーネント名
    /// @return 一致するコンポーネント（存在しない場合は nullptr）
    template<typename T>
    T* GetComponent2D(const std::string &componentName) const {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        auto *base = GetComponent2D(componentName);
        return base ? static_cast<T*>(base) : nullptr;
    }

    /// @brief 型から一致する最初のコンポーネントを取得
    /// @tparam T 取得したいコンポーネント型（例: Transform2D）
    /// @return 一致するコンポーネント（存在しない場合は nullptr）
    template<typename T>
    T* GetComponent2D() const {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        const auto range = components2DIndexByType_.equal_range(std::type_index(typeid(T)));
        for (auto it = range.first; it != range.second; ++it) {
            const size_t idx = it->second;
            if (idx < components2D_.size()) {
                return static_cast<T*>(components2D_[idx]);
            }
        }
        return nullptr;
    }

    /// @brief 名前からコンポーネントの存在を確認
    /// @param componentName コンポーネント名
    /// @return 一致するコンポーネントの個数
    size_t HasComponents2D(const std::string &componentName) const {
        auto range = components2DIndexByName_.equal_range(componentName);
        size_t count = 0;
        for (auto it = range.first; it != range.second; ++it) {
            size_t idx = it->second;
            if (idx < components2D_.size()) ++count;
        }
        return count;
    }

    /// @brief 型からコンポーネントの存在を確認
    /// @tparam T コンポーネントの型
    /// @return 一致するコンポーネントの個数
    template<typename T>
    size_t HasComponents2D() const {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        return static_cast<size_t>(components2DIndexByType_.count(std::type_index(typeid(T))));
    }

    /// @brief 名前から一致する指定インデックスのコンポーネントを削除
    /// @param componentName コンポーネント名
    /// @param index 同名コンポーネントの何番目を削除するか (0: 最初)
    /// @return 削除に成功した場合は true
    bool RemoveComponent2D(const std::string &componentName, size_t index = 0);

    /// @brief インスタンシング用バッチキー（同一キー同士がまとめて描画される）
    std::uint64_t GetInstanceBatchKey() const { return instanceBatchKey_; }

protected:
    /// @brief コンストラクタ
    /// @param name オブジェクト名
    Object2DBase(const std::string &name);

    /// @brief コンストラクタ
    /// @param name オブジェクト名
    /// @param vertexByteSize 1頂点あたりのサイズ（バイト単位）
    /// @param indexByteSize 1インデックスあたりのサイズ（バイト単位）
    /// @param vertexCount 頂点数
    /// @param indexCount インデックス数
    /// @param initialVertexData 初期頂点データ（nullptrの場合は未初期化）
    /// @param initialIndexData 初期インデックスデータ（nullptrの場合は未初期化）
    Object2DBase(const std::string &name,
        size_t vertexByteSize, size_t indexByteSize,
        size_t vertexCount, size_t indexCount,
        void *initialVertexData = nullptr,
        void *initialIndexData = nullptr);

    /// @brief 更新処理（派生クラスでの独自処理用）
    virtual void OnUpdate() {}

#if defined(USE_IMGUI)
    /// @brief ImGui 拡張表示（派生クラスで任意実装。Begin/End は呼ばない）
    virtual void ShowImGuiDerived() {}
#endif

    /// @brief シェーダ変数設定処理 (true を返した場合のみ描画コマンド生成へ進む)
    /// @param shaderBinder シェーダー変数バインダー
    /// @return 描画する場合は true
    virtual bool Render(ShaderVariableBinder &shaderBinder) { (void)shaderBinder; return false; }

    /// @brief 描画コマンド生成処理
    /// @param pipelineBinder パイプラインバインダー
    /// @return 描画コマンド (生成しない場合は std::nullopt)
    virtual std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder);

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

    /// @brief まとめ描画時に呼ばれる（共通リソースのバインド + instanceCount 分のバッファ確保）
    virtual bool RenderBatched(ShaderVariableBinder &shaderBinder, std::uint32_t instanceCount);

    /// @brief インスタンスデータを書き込む
    virtual bool SubmitInstance(void *instanceMaps, ShaderVariableBinder &shaderBinder, std::uint32_t instanceIndex);

    void SetRenderType(RenderType type) { renderType_ = type; }

    void SetConstantBufferRequirements(std::vector<RenderPass::ConstantBufferRequirement> reqs) {
        constantBufferRequirements_ = std::move(reqs);
    }

    void SetUpdateConstantBuffersFunction(std::function<bool(void *constantBufferMaps, std::uint32_t instanceCount)> fn) {
        updateConstantBuffersFunction_ = std::move(fn);
    }

private:
    friend class Object2DContext;

    /// @brief レンダーパスの作成（Window）
    RenderPass CreateRenderPass(Window *targetWindow, const std::string &pipelineName);

    /// @brief レンダーパスの作成（ScreenBuffer）
    RenderPass CreateRenderPass(ScreenBuffer *targetBuffer, const std::string &pipelineName);

    struct RenderPassRegistrationEntry {
        RenderPassRegistrationInfo info;
        Renderer::PersistentPassHandle windowHandle{};
        Renderer::PersistentOffscreenPassHandle offscreenHandle{};
    };

    RenderPassRegistrationHandle GenerateRegistrationHandle() const;

    std::unordered_map<std::uint64_t, RenderPassRegistrationEntry> renderPassRegistrations_;

    struct ShaderBindingFailureInfo {
        size_t componentIndex;
        std::string componentType;
    };

    /// @brief コンポーネントのシェーダー変数バインド処理
    /// @param shaderBinder シェーダ変数バインダー
    /// @return バインドに失敗したコンポーネントの情報リスト
    std::vector<ShaderBindingFailureInfo> BindShaderVariablesToComponents(ShaderVariableBinder &shaderBinder);

    std::string name_ = "GameObject2D";
    std::string passName_ = "GameObject2D";

    mutable std::optional<RenderPass> cachedRenderPass_;

    UINT vertexCount_ = 0;
    UINT indexCount_ = 0;
    std::unique_ptr<VertexBufferResource> vertexBuffer_;
    std::unique_ptr<IndexBufferResource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    void *vertexData_ = nullptr;
    void *indexData_ = nullptr;
    std::vector<std::unique_ptr<IObjectComponent>> components_;
    std::unordered_multimap<std::string, size_t> componentsIndexByName_;

    std::vector<IObjectComponent2D*> components2D_;
    std::unordered_multimap<std::string, size_t> components2DIndexByName_;
    std::unordered_multimap<std::type_index, size_t> components2DIndexByType_;

    std::unique_ptr<Object2DContext> context_;
    std::vector<size_t> shaderBindingComponentIndices_;

    MyStd::AnyUnorderedMap userData_;

    std::uint64_t instanceBatchKey_ = 0;
    RenderType renderType_ = RenderType::Standard;
    std::vector<RenderPass::ConstantBufferRequirement> constantBufferRequirements_;
    std::function<bool(void *constantBufferMaps, std::uint32_t instanceCount)> updateConstantBuffersFunction_;
};

} // namespace KashipanEngine
