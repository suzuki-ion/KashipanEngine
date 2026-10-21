#pragma once
#include <string>
#include <span>
#include <vector>
#include <memory>
#include <utility>
#include <typeindex>
#include <type_traits>

#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief ゲームオブジェクトコンテキスト
class ObjectContext final {
public:
    ObjectContext(Passkey<EmptyObject>, EmptyObject *owner) : owner_(owner) {}
    ~ObjectContext() = default;

    ObjectContext(const ObjectContext &) = delete;
    ObjectContext &operator=(const ObjectContext &) = delete;
    ObjectContext(ObjectContext &&) = delete;
    ObjectContext &operator=(ObjectContext &&) = delete;

    const EmptyObject *GetOwner() const { return owner_; }
    const std::string &GetName() const { return owner_->GetName(); }

    //==================================================
    // コンポーネント取得系メソッド
    //==================================================

    /// @brief 型から一致するコンポーネント一覧を取得
    /// @tparam T コンポーネントの型
    /// @return 一致するコンポーネントのリスト（存在しない場合は空のリスト）
    template<typename T>
    std::vector<T *> GetComponents() const { return owner_->GetComponents<T>(); }
    /// @brief 型から一致する最初のコンポーネントを取得
    /// @tparam T 取得したいコンポーネント型
    /// @return 一致するコンポーネント（存在しない場合は nullptr）
    template<typename T>
    T *GetComponent() const { return owner_->GetComponent<T>(); }
    /// @brief ポインタからコンポーネントを取得
    /// @param component コンポーネントのポインタ
    /// @return コンポーネント（存在しない場合は nullptr）
    IObjectComponent *GetComponent(const IObjectComponent *component) const { return owner_->GetComponent(component); }
    /// @brief 型からコンポーネントの個数を確認
    /// @tparam T コンポーネントの型
    /// @return 一致するコンポーネントの個数
    template<typename T>
    size_t HasComponents() const { return owner_->HasComponents<T>(); }
    /// @brief ポインタからコンポーネントの個数を確認
    /// @param component コンポーネントのポインタ
    /// @return 一致するコンポーネントの個数
    size_t HasComponent(const IObjectComponent *component) const { return owner_->HasComponent(component); }
    /// @brief 全コンポーネントの取得（コンポーネント本体と追加順のペアのリスト）
    const std::vector<std::pair<IObjectComponent *, size_t>> &GetAllComponents() const { return owner_->GetAllComponents(); }

    //==================================================
    // コンポーネント追加系メソッド
    //==================================================

    /// @brief 既存コンポーネントの追加
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    IObjectComponent *AddComponent(std::unique_ptr<IObjectComponent> comp) { return owner_->AddComponent(std::move(comp)); }
    /// @brief 既存コンポーネントの追加
    /// @tparam T コンポーネントの型
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    template<typename T>
    T *AddComponent(std::unique_ptr<T> comp) { return owner_->AddComponent<T>(std::move(comp)); }
    /// @brief コンポーネントの追加（生成）
    /// @tparam T コンポーネントの型
    /// @tparam Args コンポーネントのコンストラクタ引数の型
    /// @param args コンポーネントのコンストラクタ引数
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    template<typename T, typename... Args>
    T *AddComponent(Args&&... args) { return owner_->AddComponent<T>(std::forward<Args>(args)...); }

    //==================================================
    // コンポーネント削除系メソッド
    //==================================================

    /// @brief ポインタからコンポーネントを削除
    /// @param component 削除したいコンポーネントのポインタ
    /// @return 削除に成功した場合は true
    bool RemoveComponent(const IObjectComponent *component) { return owner_->RemoveComponent(component); }
    /// @brief 型から一致する最初のコンポーネントを削除
    /// @tparam T 削除したいコンポーネントの型
    /// @return 削除に成功した場合は true
    template<typename T>
    bool RemoveComponent() { return owner_->RemoveComponent<T>(); }
    /// @brief 型から一致する全てのコンポーネントを削除
    /// @tparam T 削除したいコンポーネントの型
    /// @return 削除に成功した場合は true
    template <typename T>
    bool RemoveComponents() { return owner_->RemoveComponents<T>(); }

    /// @brief オブジェクトIDの取得
    const UUID128 &GetObjectID() const { return owner_->GetObjectID(); }
    /// @brief オブジェクトの保存の可否取得
    bool IsSaveEnabled() const { return owner_->IsSaveEnabled(); }
    /// @brief オブジェクトのアクティブ状態取得
    bool IsActive() const { return owner_->IsActive(); }

private:
    EmptyObject *owner_ = nullptr;
};

} // namespace KashipanEngine
