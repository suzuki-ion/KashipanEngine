#pragma once
#include <string>
#include <memory>

namespace KashipanEngine {

class GameObject2DBase;
class GameObject3DBase;

/// @brief オブジェクトコンポーネントインターフェースクラス
class IObjectComponent {
public:
    virtual ~IObjectComponent() = default;
    IObjectComponent(const IObjectComponent &) = delete;
    IObjectComponent &operator=(const IObjectComponent &) = delete;
    IObjectComponent(IObjectComponent &&) = delete;
    IObjectComponent &operator=(IObjectComponent &&) = delete;

    /// @brief コンポーネントの種類を取得
    const std::string &GetComponentType() const { return componentType_; }

    /// @brief 初期化処理
    virtual void Initialize() = 0;
    /// @brief 更新処理
    virtual void Update() = 0;
    /// @brief 描画前処理
    virtual void PreRender() = 0;
    /// @brief 終了処理
    virtual void Shutdown() = 0;
    /// @brief コンポーネントのクローンを作成（派生クラスで実装）
    virtual std::unique_ptr<IObjectComponent> Clone() const = 0;

protected:
    explicit IObjectComponent(const std::string &componentType) : componentType_(componentType) {}

private:
    const std::string componentType_ = "IObjectComponent";
};

/// @brief 2D向けオブジェクトコンポーネント基底クラス
class IObjectComponent2D : public IObjectComponent {
public:
    virtual ~IObjectComponent2D() = default;
protected:
    using IObjectComponent::IObjectComponent;
};

/// @brief 3D向けオブジェクトコンポーネント基底クラス
class IObjectComponent3D : public IObjectComponent {
public:
    virtual ~IObjectComponent3D() = default;
protected:
    using IObjectComponent::IObjectComponent;
};

} // namespace KashipanEngine
