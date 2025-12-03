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
    const std::string &GetComponentType() const { return componentType_; }

    /// @brief 初期化処理
    virtual void Initialize(IGameObjectContext &context) = 0;
    /// @brief 更新処理
    virtual void Update(IGameObjectContext &context) = 0;
    /// @brief 描画前処理
    virtual void PreRender(IGameObjectContext &context) = 0;
    /// @brief 終了処理
    virtual void Shutdown(IGameObjectContext &context) = 0;
    /// @brief コンポーネントのクローンを作成（派生クラスで実装）
    virtual std::unique_ptr<IGameObjectComponent> Clone() const = 0;

protected:
    explicit IGameObjectComponent(const std::string &componentType) : componentType_(componentType) {}

private:
    const std::string componentType_ = "IGameObjectComponent";
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
