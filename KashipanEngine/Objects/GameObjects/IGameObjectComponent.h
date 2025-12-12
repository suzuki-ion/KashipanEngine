#pragma once
#include <string>
#include <memory>
#include <cassert>
#include <optional>
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"

namespace KashipanEngine {

class IGameObjectContext;
class GameObject2DContext;
class GameObject3DContext;

/// @brief オブジェクトコンポーネントインターフェースクラス
class IGameObjectComponent {
public:
    virtual ~IGameObjectComponent() = default;
    IGameObjectComponent(const IGameObjectComponent &) = delete;
    IGameObjectComponent &operator=(const IGameObjectComponent &) = delete;
    IGameObjectComponent(IGameObjectComponent &&) = delete;
    IGameObjectComponent &operator=(IGameObjectComponent &&) = delete;

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

    /// @brief 更新前処理（オブジェクトのUpdateの前に呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> PreUpdate() { return std::nullopt; }
    /// @brief 更新後処理（Updateの後に呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> PostUpdate() { return std::nullopt; }

    /// @brief 描画前処理（Renderの前に呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> PreRender() { return std::nullopt; }
    /// @brief 描画後処理（Renderの後に呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> PostRender() { return std::nullopt; }

    /// @brief フレーム開始時の更新（全体のBeginFrameで呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> BeginFrameUpdate() { return std::nullopt; }
    /// @brief フレーム終了時の更新（全体のEndFrameで呼ばれる想定）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。処理を行わない場合は std::nullopt を返す
    virtual std::optional<bool> EndFrameUpdate() { return std::nullopt; }

    /// @brief シェーダー変数へのバインド処理 
    /// @param binder シェーダー変数バインダー
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す。バインドを行わない場合は std::nullopt を返す
    virtual std::optional<bool> BindShaderVariables(ShaderVariableBinder *binder) {
        (void)binder;
        return std::nullopt;
    }

    /// @brief 所属オブジェクトのコンテキストを設定
    void SetOwnerContext(IGameObjectContext *context) {
        if (!context) assert(false && "Owner context cannot be null.");
        ownerObject_ = context;
    }
    
    /// @brief コンポーネントのクローンを作成（派生クラスで実装）
    virtual std::unique_ptr<IGameObjectComponent> Clone() const = 0;

protected:
    IGameObjectComponent(const std::string &componentType, size_t maxComponentCountPerObject)
        : kComponentType_(componentType), kMaxComponentCountPerObject_(maxComponentCountPerObject) {}
    /// @brief 所属オブジェクトのコンテキストを取得
    IGameObjectContext *GetOwnerContext() const { return ownerObject_; }

private:
    /// @brief コンポーネントの種類名
    const std::string kComponentType_ = "IGameObjectComponent";
    /// @brief 1つのオブジェクトに登録可能な同じコンポーネントの最大数
    const size_t kMaxComponentCountPerObject_ = 0xFF;
    /// @brief オーナーオブジェクト
    IGameObjectContext *ownerObject_ = nullptr;
};

/// @brief 2D向けオブジェクトコンポーネント基底クラス
class IGameObjectComponent2D : public IGameObjectComponent {
public:
    virtual ~IGameObjectComponent2D() = default;
protected:
    using IGameObjectComponent::IGameObjectComponent;

    /// @brief 2Dオブジェクトコンテキストの取得
    GameObject2DContext *GetOwner2DContext() const;
};

/// @brief 3D向けオブジェクトコンポーネント基底クラス
class IGameObjectComponent3D : public IGameObjectComponent {
public:
    virtual ~IGameObjectComponent3D() = default;
protected:
    using IGameObjectComponent::IGameObjectComponent;

    /// @brief 3Dオブジェクトコンテキストの取得
    GameObject3DContext *GetOwner3DContext() const;
};

} // namespace KashipanEngine
