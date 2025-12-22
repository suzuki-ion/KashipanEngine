#pragma once
#include <string>
#include <span>
#include "Objects/Object2DBase.h"
#include "Objects/Object3DBase.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief ゲームオブジェクトコンテキストインターフェースクラス
class IObjectContext {
public:
    virtual ~IObjectContext() = default;
    virtual const std::string &GetName() const = 0;
};

/// @brief 2Dゲームオブジェクトコンテキストクラス
class Object2DContext : public IObjectContext {
public:
    Object2DContext(Passkey<Object2DBase>, Object2DBase *owner) : owner_(owner) {}
    ~Object2DContext() = default;

    Object2DContext(const Object2DContext &) = delete;
    Object2DContext &operator=(const Object2DContext &) = delete;
    Object2DContext(Object2DContext &&) = delete;
    Object2DContext &operator=(Object2DContext &&) = delete;

    /// @brief オブジェクト名の取得
    const std::string &GetName() const override;
    /// @brief 頂点データの取得
    template<typename T>
    std::span<T> GetVertexData() const { return owner_->template GetVertexData<T>(); }
    /// @brief 頂点データの設定
    template<typename T>
    void SetVertexData(const std::span<T> &data) { owner_->template SetVertexData<T>(data); }
    /// @brief 他コンポーネントの取得
    std::vector<IObjectComponent2D *> GetComponents(const std::string &componentName) const;
    /// @brief コンポーネントの存在チェック
    /// @return コンポーネントの個数
    size_t HasComponents(const std::string &componentName) const;

private:
    Object2DBase *owner_ = nullptr;
};

/// @brief 3Dゲームオブジェクトコンテキストクラス
class Object3DContext : public IObjectContext {
public:
    Object3DContext(Passkey<Object3DBase>, Object3DBase *owner) : owner_(owner) {}
    ~Object3DContext() = default;

    Object3DContext(const Object3DContext &) = delete;
    Object3DContext &operator=(const Object3DContext &) = delete;
    Object3DContext(Object3DContext &&) = delete;
    Object3DContext &operator=(Object3DContext &&) = delete;

    /// @brief オブジェクト名の取得
    const std::string &GetName() const override;
    /// @brief 頂点データの取得
    template<typename T>
    std::span<T> GetVertexData() const { return owner_->template GetVertexData<T>(); }
    /// @brief 頂点データの設定
    template<typename T>
    void SetVertexDataImpl(const std::span<T> &data) { owner_->template SetVertexData<T>(data); }
    /// @brief 他コンポーネントの取得
    std::vector<IObjectComponent3D *> GetComponents(const std::string &componentName) const;
    /// @brief コンポーネントの存在チェック
    /// @return コンポーネントの個数
    size_t HasComponents(const std::string &componentName) const;

private:
    Object3DBase *owner_ = nullptr;
};

} // namespace KashipanEngine
