#pragma once
#include "Objects/IObjectComponent.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector4.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {

class ObjectContext;
class SceneContext;
class SceneBase;

/// @brief 3Dオブジェクト基底クラス
class EmptyObject final {
    EmptyObject(SceneContext *ownerSceneContext, const std::string &name);
public:
    EmptyObject(Passkey<SceneBase>, SceneContext *ownerSceneContext, const std::string &name = "EmptyObject")
        : EmptyObject(ownerSceneContext, name) {}
    ~EmptyObject() = default;

    EmptyObject(const EmptyObject &) = delete;
    EmptyObject &operator=(const EmptyObject &) = delete;
    EmptyObject(EmptyObject &&) = delete;
    EmptyObject &operator=(EmptyObject &&) = delete;

    std::unique_ptr<EmptyObject> Clone() const;

    void Update(Passkey<SceneBase>);

    void SetName(const std::string &name) { name_ = name; }
    const std::string &GetName() const { return name_; }

    //==================================================
    // コンポーネント追加系メソッド
    //==================================================

    /// @brief コンポーネントの追加（生成）
    /// @tparam T コンポーネントの型（IObjectComponentを継承している必要あり）
    /// @tparam Args コンポーネントのコンストラクタ引数の型
    /// @param args コンポーネントのコンストラクタ引数
    /// @return 追加に成功した場合は true
    template<typename T, typename... Args>
    bool AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        try {
            auto comp = std::make_unique<T>(std::forward<Args>(args)...);
            return AddComponent(std::move(comp));
        } catch (...) { return false; }
    }
    /// @brief 既存コンポーネントの追加
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 追加に成功した場合は true
    bool AddComponent(std::unique_ptr<IObjectComponent> comp);

    //==================================================
    // コンポーネント削除系メソッド
    //==================================================

    /// @brief 型から一致する指定インデックスのコンポーネントを削除
    /// @tparam T コンポーネントの型
    /// @param index 同型コンポーネントの何番目を削除するか (0: 最初)
    /// @return 削除に成功した場合は true
    template<typename T>
    bool RemoveComponent(size_t index = 0) {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        size_t typeIndex = IObjectComponent::GetComponentTypeID<T>();
        if (typeIndex >= componentsIndexByType_.size()) return false;
        const auto &indices = componentsIndexByType_[typeIndex];
        if (index >= indices.size()) return false;
        size_t idx = indices[index];
        if (idx < components_.size()) {
            return RemoveComponent(components_[idx].get());
        }
        return false;
    }
    /// @brief ポインタからコンポーネントを削除
    /// @param component 削除したいコンポーネントのポインタ
    /// @return 削除に成功した場合は true
    bool RemoveComponent(IObjectComponent *component);

    //==================================================
    // コンポーネント取得系メソッド
    //==================================================

    /// @brief 型から一致するコンポーネント一覧を取得
    /// @tparam T コンポーネントの型（例: Transform3D）
    /// @return 一致するコンポーネントのリスト（存在しない場合は空のリスト）
    template<typename T>
    std::vector<T *> GetComponents() const {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        std::vector<T *> result;
        size_t typeIndex = IObjectComponent::GetComponentTypeID<T>();
        if (typeIndex >= componentsIndexByType_.size()) return result;
        const auto &indices = componentsIndexByType_[typeIndex];
        for (size_t idx : indices) {
            if (idx < components_.size()) {
                result.push_back(static_cast<T *>(components_[idx].get()));
            }
        }
        return result;
    }
    /// @brief 型から一致する最初のコンポーネントを取得
    /// @tparam T 取得したいコンポーネント型（例: Transform3D）
    /// @return 一致するコンポーネント（存在しない場合は nullptr）
    template<typename T>
    T *GetComponent() const {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        size_t typeIndex = IObjectComponent::GetComponentTypeID<T>();
        if (typeIndex >= componentsIndexByType_.size()) return nullptr;
        const auto &indices = componentsIndexByType_[typeIndex];
        for (size_t idx : indices) {
            if (idx < components_.size()) {
                return static_cast<T *>(components_[idx].get());
            }
        }
        return nullptr;
    }
    /// @brief 型からコンポーネントの個数を確認
    /// @tparam T コンポーネントの型
    /// @return 一致するコンポーネントの個数
    template<typename T>
    size_t HasComponents() const {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        size_t typeIndex = IObjectComponent::GetComponentTypeID<T>();
        if (typeIndex >= componentsIndexByType_.size()) return 0;
        return static_cast<size_t>(componentsIndexByType_[typeIndex].size());
    }
    /// @brief ポインタからコンポーネントの個数を確認
    /// @param component コンポーネントのポインタ
    /// @return 一致するコンポーネントの個数
    size_t HasComponent(IObjectComponent *component) const {
        if (component == nullptr) return 0;
        auto it = componentsIndexByPointer_.find(component);
        if (it == componentsIndexByPointer_.end()) return 0;
        return 1;
    }
    /// @brief 全コンポーネントの取得
    const std::vector<std::pair<std::unique_ptr<IObjectComponent>, size_t>> &GetAllComponents() const { return components_; }

    /// @brief オブジェクトIDの設定
    void SetObjectID(const UUID128 &id) { objectID_ = id; }
    /// @brief オブジェクトIDの取得
    const UUID128 &GetObjectID() const { return objectID_; }
    /// @brief オブジェクトの保存の可否設定
    void SetSaveEnabled(bool enabled) { isSaveEnabled_ = enabled; }
    /// @brief オブジェクトの保存の可否取得
    bool IsSaveEnabled() const { return isSaveEnabled_; }

    /// @brief オブジェクトのアクティブ状態設定
    void SetActive(bool active) { isActive_ = active; }
    /// @brief オブジェクトのアクティブ状態取得
    bool IsActive() const { return isActive_; }

private:
    friend class ObjectContext;

    /// @brief コンポーネントのインデックステーブルの再作成
    void RebuildComponentIndexTables();

    std::string name_ = "EmptyObject";

    /// @brief コンポーネントのリスト（ペア: コンポーネント本体, コンポーネントの世代）
    std::vector<std::pair<std::unique_ptr<IObjectComponent>, size_t>> components_;
    std::vector<std::vector<size_t>> componentsIndexByType_;
    std::unordered_map<IObjectComponent *, size_t> componentsIndexByPointer_;
    std::vector<size_t> componentsFreeIndices_;

    SceneContext *ownerSceneContext_ = nullptr;
    std::unique_ptr<ObjectContext> objectContext_;
    std::unordered_map<std::string, std::any> objectProperties_;
    UUID128 objectID_ = UUID128(true);
    bool isSaveEnabled_ = true;

    bool isActive_ = true;
};

} // namespace KashipanEngine
