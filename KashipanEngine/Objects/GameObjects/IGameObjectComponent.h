#pragma once
#include <string>
#include <memory>

namespace KashipanEngine {

class IGameObjectContext;

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

    /// @brief 初期化処理
    virtual void Initialize(IGameObjectContext &context) { (void)context; }
    /// @brief 終了処理
    virtual void Finalize(IGameObjectContext &context) { (void)context; }

    /// @brief 更新前処理（Updateの前に呼ばれる想定）
    virtual void PreUpdate(IGameObjectContext &context) { (void)context; }
    /// @brief 更新後処理（Updateの後に呼ばれる想定）
    virtual void PostUpdate(IGameObjectContext &context) { (void)context; }

    /// @brief 描画前処理（Renderの前に呼ばれる想定）
    virtual void PreRender(IGameObjectContext &context) { (void)context; }
    /// @brief 描画後処理（Renderの後に呼ばれる想定）
    virtual void PostRender(IGameObjectContext &context) { (void)context; }

    /// @brief フレーム開始時の更新（全体のBeginFrameで呼ばれる想定）
    virtual void BeginFrameUpdate(IGameObjectContext &context) { (void)context; }
    /// @brief フレーム終了時の更新（全体のEndFrameで呼ばれる想定）
    virtual void EndFrameUpdate(IGameObjectContext &context) { (void)context; }
    
    /// @brief コンポーネントのクローンを作成（派生クラスで実装）
    virtual std::unique_ptr<IGameObjectComponent> Clone() const = 0;

protected:
    IGameObjectComponent(const std::string &componentType, size_t maxComponentCountPerObject)
        : kComponentType_(componentType), kMaxComponentCountPerObject_(maxComponentCountPerObject) {}

private:
    /// @brief コンポーネントの種類名
    const std::string kComponentType_ = "IGameObjectComponent";
    /// @brief 1つのオブジェクトに登録可能な同じコンポーネントの最大数
    const size_t kMaxComponentCountPerObject_ = 32;
};

/// @brief 2D向けオブジェクトコンポーネント基底クラス
class IGameObjectComponent2D : public IGameObjectComponent {
public:
    virtual ~IGameObjectComponent2D() = default;
protected:
    using IGameObjectComponent::IGameObjectComponent;
};

/// @brief 3D向けオブジェクトコンポーネント基底クラス
class IGameObjectComponent3D : public IGameObjectComponent {
public:
    virtual ~IGameObjectComponent3D() = default;
protected:
    using IGameObjectComponent::IGameObjectComponent;
};

} // namespace KashipanEngine
