#pragma once
#include <string>
#include <memory>
#include <cassert>
#include <optional>
#include <cstdint>
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

class IObjectContext;
class Object2DContext;
class Object3DContext;

/// @brief オブジェクトコンポーネントインターフェースクラス
class IObjectComponent {
public:
    virtual ~IObjectComponent() = default;
    IObjectComponent(const IObjectComponent &) = delete;
    IObjectComponent &operator=(const IObjectComponent &) = delete;
    IObjectComponent(IObjectComponent &&) = delete;
    IObjectComponent &operator=(IObjectComponent &&) = delete;

    /// @brief コンポーネントの種類を取得
    const std::string &GetComponentType() const { return kComponentType_; }
    /// @brief 1つのオブジェクトに登録可能な同じコンポーネントの最大数を取得
    size_t GetMaxComponentCountPerObject() const { return kMaxComponentCountPerObject_; }

    /// @brief 初期化処理（オブジェクトに登録された直後に呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> Initialize() { return std::nullopt; }
    /// @brief 終了処理（オブジェクトから削除される直前に呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> Finalize() { return std::nullopt; }

    /// @brief 更新処理（オブジェクトのUpdateで呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> Update() { return std::nullopt; }

    /// @brief 描画処理（描画パス生成前に呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> Render() { return std::nullopt; }

    /// @brief フレーム終了時の更新（全体のEndFrameで呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> EndFrameUpdate() { return std::nullopt; }

#if defined(USE_IMGUI)
    /// @brief ImGui 表示（ウィンドウの Begin/End は呼ばない）
    virtual void ShowImGui() = 0;
#endif

    /// @brief Updateの処理優先順位（小さいほど先に処理される）
    int GetUpdatePriority() const { return updatePriority_; }
    void SetUpdatePriority(int priority) { updatePriority_ = priority; }

    /// @brief Renderの処理優先順位（小さいほど先に処理される）
    int GetRenderPriority() const { return renderPriority_; }
    void SetRenderPriority(int priority) { renderPriority_ = priority; }

    /// @brief EndFrameUpdateの処理優先順位（小さいほど先に処理される）
    int GetEndFrameUpdatePriority() const { return endFrameUpdatePriority_; }
    void SetEndFrameUpdatePriority(int priority) { endFrameUpdatePriority_ = priority; }

    /// @brief シェーダー変数へのバインド処理 
    /// @param binder シェーダー変数バインダー
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。バインドを行わない場合は std::nullopt を返す
    virtual std::optional<bool> BindShaderVariables(ShaderVariableBinder *binder) {
        (void)binder;
        return std::nullopt;
    }

    /// @brief インスタンシング描画時のリソースバインド（バッチ単位で 1 回）
    /// @param binder シェーダー変数バインダー
    /// @param instanceCount インスタンス数
    /// @return true を返した場合のみインスタンスデータ送信が呼ばれる。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> BindInstancingResources(ShaderVariableBinder *binder, std::uint32_t instanceCount) {
        (void)binder;
        (void)instanceCount;
        return std::nullopt;
    }

    /// @brief インスタンシング描画用インスタンスマップ取得（バッチ単位で 1 回）
    /// @details 実装したコンポーネントは、内部で保持しているインスタンシング用バッファを Map し、
    ///          `SubmitInstance` に渡される書き込み先ポインタを返す。
    /// @return 取得したマップポインタ（未対応の場合は nullptr）
    virtual void *AcquireInstanceMap() { return nullptr; }

    /// @brief インスタンシング描画用インスタンスマップ解放（バッチ単位で 1 回）
    /// @param instanceMap AcquireInstanceMap が返したポインタ
    virtual void ReleaseInstanceMap(void *instanceMap) { (void)instanceMap; }

    /// @brief インスタンシング描画時のインスタンスデータ送信（インスタンス単位）
    /// @param instanceMap 送信先のインスタンスマップ
    /// @param instanceIndex インスタンスのインデックス
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> SubmitInstance(void *instanceMap, std::uint32_t instanceIndex) {
        (void)instanceMap;
        (void)instanceIndex;
        return std::nullopt;
    }

    /// @brief 所属オブジェクトのコンテキストを設定
    void SetOwnerContext(IObjectContext *context) {
        if (!context) assert(false && "Owner context cannot be null.");
        ownerObject_ = context;
    }
    
    /// @brief コンポーネントのクローンを作成（派生クラスで実装）
    virtual std::unique_ptr<IObjectComponent> Clone() const = 0;

protected:
    IObjectComponent(const std::string &componentType, size_t maxComponentCountPerObject)
        : kComponentType_(componentType), kMaxComponentCountPerObject_(maxComponentCountPerObject) {}
    /// @brief 所属オブジェクトのコンテキストを取得
    IObjectContext *GetOwnerContext() const { return ownerObject_; }

private:
    /// @brief コンポーネントの種類名
    const std::string kComponentType_ = "IObjectComponent";
    /// @brief 1つのオブジェクトに登録可能な同じコンポーネントの最大数
    const size_t kMaxComponentCountPerObject_ = 0xFF;

    int updatePriority_ = 1;
    int renderPriority_ = 1;
    int endFrameUpdatePriority_ = 1;

    /// @brief オーナーオブジェクト
    IObjectContext *ownerObject_ = nullptr;
};

/// @brief 2D向けオブジェクトコンポーネント基底クラス
class IObjectComponent2D : public IObjectComponent {
public:
    virtual ~IObjectComponent2D() = default;
protected:
    using IObjectComponent::IObjectComponent;

    /// @brief 2Dオブジェクトコンテキストの取得
    Object2DContext *GetOwner2DContext() const;
};

/// @brief 3D向けオブジェクトコンポーネント基底クラス
class IObjectComponent3D : public IObjectComponent {
public:
    virtual ~IObjectComponent3D() = default;
protected:
    using IObjectComponent::IObjectComponent;

    /// @brief 3Dオブジェクトコンテキストの取得
    Object3DContext *GetOwner3DContext() const;
};

} // namespace KashipanEngine
