#pragma once
#include <string>
#include <span>
#include <vector>
#include <typeindex>
#include <type_traits>

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

    Object2DBase *GetOwner() const { return owner_; }

    /// @brief オブジェクト名の取得
    const std::string &GetName() const override;

    /// @brief 頂点データの取得
    template<typename T>
    std::span<T> GetVertexData() const { return owner_->template GetVertexData<T>(); }

    /// @brief 頂点データの設定
    template<typename T>
    void SetVertexData(const std::span<T> &data) { owner_->template SetVertexData<T>(data); }

    /// @brief 名前から一致するコンポーネントを取得
    std::vector<IObjectComponent2D *> GetComponents(const std::string &componentName) const;

    /// @brief 名前から一致する最初のコンポーネントを取得
    IObjectComponent2D *GetComponent(const std::string &componentName) const;

    /// @brief 名前から一致する最初のコンポーネントを取得（型付き）
    template<typename T>
    T *GetComponent(const std::string &componentName) const {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        auto *base = GetComponent(componentName);
        return base ? static_cast<T *>(base) : nullptr;
    }

    /// @brief 型から一致するコンポーネントを取得
    template<typename T>
    std::vector<T *> GetComponents() const {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        if (!owner_) return {};
        return owner_->template GetComponents2D<T>();
    }

    /// @brief 名前から一致するコンポーネントを取得（型付き）
    template<typename T>
    std::vector<T *> GetComponents(const std::string &componentName) const {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        auto baseList = GetComponents(componentName);
        std::vector<T *> result;
        result.reserve(baseList.size());
        for (auto *c : baseList) {
            result.push_back(static_cast<T *>(c));
        }
        return result;
    }

    /// @brief 型から一致する最初のコンポーネントを取得
    template<typename T>
    T *GetComponent() const {
        static_assert(std::is_base_of_v<IObjectComponent2D, T>, "T must derive from IObjectComponent2D");
        if (!owner_) return nullptr;
        return owner_->template GetComponent2D<T>();
    }

    /// @brief コンポーネントの存在チェック
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

    Object3DBase *GetOwner() const { return owner_; }

    /// @brief オブジェクト名の取得
    const std::string &GetName() const override;

    /// @brief 頂点データの取得
    template<typename T>
    std::span<T> GetVertexData() const { return owner_->template GetVertexData<T>(); }

    /// @brief 頂点データの設定
    template<typename T>
    void SetVertexDataImpl(const std::span<T> &data) { owner_->template SetVertexData<T>(data); }

    //==================================================
    // Component getters (similar to Object3DBase)
    //==================================================

    /// @brief 他コンポーネントの取得（名前）
    std::vector<IObjectComponent3D *> GetComponents(const std::string &componentName) const;

    /// @brief 名前から一致する最初のコンポーネントを取得
    IObjectComponent3D *GetComponent(const std::string &componentName) const;

    /// @brief 名前から一致する最初のコンポーネントを取得（型付き）
    template<typename T>
    T *GetComponent(const std::string &componentName) const {
        static_assert(std::is_base_of_v<IObjectComponent3D, T>, "T must derive from IObjectComponent3D");
        auto *base = GetComponent(componentName);
        return base ? static_cast<T *>(base) : nullptr;
    }

    /// @brief 型から一致するコンポーネントを取得
    template<typename T>
    std::vector<T *> GetComponents() const {
        static_assert(std::is_base_of_v<IObjectComponent3D, T>, "T must derive from IObjectComponent3D");
        if (!owner_) return {};
        return owner_->template GetComponents3D<T>();
    }

    /// @brief 名前から一致するコンポーネントを取得（型付き）
    template<typename T>
    std::vector<T *> GetComponents(const std::string &componentName) const {
        static_assert(std::is_base_of_v<IObjectComponent3D, T>, "T must derive from IObjectComponent3D");
        auto baseList = GetComponents(componentName);
        std::vector<T *> result;
        result.reserve(baseList.size());
        for (auto *c : baseList) {
            result.push_back(static_cast<T *>(c));
        }
        return result;
    }

    /// @brief 型から一致する最初のコンポーネントを取得
    template<typename T>
    T *GetComponent() const {
        static_assert(std::is_base_of_v<IObjectComponent3D, T>, "T must derive from IObjectComponent3D");
        if (!owner_) return nullptr;
        return owner_->template GetComponent3D<T>();
    }

    /// @brief コンポーネントの存在チェック
    size_t HasComponents(const std::string &componentName) const;

private:
    Object3DBase *owner_ = nullptr;
};

} // namespace KashipanEngine
