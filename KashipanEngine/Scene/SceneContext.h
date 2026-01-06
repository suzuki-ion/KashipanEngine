#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include "Scene/SceneBase.h"

namespace KashipanEngine {

/// @brief Scene コンテキスト（SceneComponent 用）
class SceneContext final {
public:
    SceneContext(Passkey<SceneBase>, SceneBase *owner) : owner_(owner) {}
    ~SceneContext() = default;

    SceneContext(const SceneContext &) = delete;
    SceneContext &operator=(const SceneContext &) = delete;
    SceneContext(SceneContext &&) = delete;
    SceneContext &operator=(SceneContext &&) = delete;

    SceneBase *GetOwner() const { return owner_; }

    const std::string &GetName() const;

    /// @brief 名前から一致するコンポーネントを取得
    std::vector<ISceneComponent *> GetComponents(const std::string &componentName) const;

    /// @brief 名前から一致する最初のコンポーネントを取得
    ISceneComponent *GetComponent(const std::string &componentName) const;

    /// @brief 名前から一致する最初のコンポーネントを取得（型付き）
    template<typename T>
    T *GetComponent(const std::string &componentName) const {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        auto *base = GetComponent(componentName);
        return base ? static_cast<T *>(base) : nullptr;
    }

    /// @brief 型から一致するコンポーネントを取得
    template<typename T>
    std::vector<T *> GetComponents() const {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        if (!owner_) return {};
        return owner_->template GetSceneComponents<T>();
    }

    /// @brief 名前から一致するコンポーネントを取得（型付き）
    template<typename T>
    std::vector<T *> GetComponents(const std::string &componentName) const {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        auto baseList = GetComponents(componentName);
        std::vector<T *> result;
        result.reserve(baseList.size());
        for (auto *c : baseList) result.push_back(static_cast<T *>(c));
        return result;
    }

    /// @brief 型から一致する最初のコンポーネントを取得
    template<typename T>
    T *GetComponent() const {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        if (!owner_) return nullptr;
        return owner_->template GetSceneComponent<T>();
    }

    /// @brief コンポーネントの存在チェック
    size_t HasComponents(const std::string &componentName) const;

    /// @brief 2D オブジェクトを追加
    bool AddObject2D(std::unique_ptr<Object2DBase> obj);

    /// @brief 3D オブジェクトを追加
    bool AddObject3D(std::unique_ptr<Object3DBase> obj);

    /// @brief 2D オブジェクトを削除
    bool RemoveObject2D(Object2DBase *obj);

    /// @brief 3D オブジェクトを削除
    bool RemoveObject3D(Object3DBase *obj);

private:
    SceneBase *owner_ = nullptr;
};

} // namespace KashipanEngine
