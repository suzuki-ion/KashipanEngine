#pragma once
#include <algorithm>
#include "Objects/IObjectComponent.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector4.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {

class ObjectContext;
class SceneContext;
class Scene;

/// @brief 空オブジェクトクラス
class EmptyObject final {
    EmptyObject(SceneContext *ownerSceneContext, const std::string &name);
public:
    EmptyObject() = delete;
    EmptyObject(Passkey<Scene>, SceneContext *ownerSceneContext, const std::string &name = "EmptyObject")
        : EmptyObject(ownerSceneContext, name) {}
    ~EmptyObject();

    EmptyObject(const EmptyObject &) = delete;
    EmptyObject &operator=(const EmptyObject &) = delete;
    EmptyObject(EmptyObject &&) = delete;
    EmptyObject &operator=(EmptyObject &&) = delete;

    std::unique_ptr<EmptyObject> Clone() const;

    void InitializeInterface(Passkey<Scene>) { Initialize(); }
    void FinalizeInterface(Passkey<Scene>) { Finalize(); }
    void UpdateInterface(Passkey<Scene>) { Update(); }

    void SetName(const std::string &name) { name_ = name; }
    const std::string &GetName() const { return name_; }

    //==================================================
    // コンポーネント取得系メソッド
    //==================================================

    /// @brief 型から一致するコンポーネント一覧を取得
    /// @tparam T コンポーネントの型
    /// @return 一致するコンポーネントのリスト（存在しない場合は空のリスト）
    template<typename T>
    std::vector<T *> GetComponents() const {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        std::vector<T *> result;
        size_t typeIndex = IObjectComponent::GetComponentTypeID<T>();
        if (typeIndex >= componentsIndexByType_.size()) return result;
        const auto &indices = componentsIndexByType_[typeIndex];
        std::vector<const std::pair<std::unique_ptr<IObjectComponent>, size_t> *> sortedComponents;
        for (size_t idx : indices) {
            if (idx < components_.size()) {
                sortedComponents.push_back(&components_[idx]);
            }
        }
        std::sort(sortedComponents.begin(), sortedComponents.end(), [](const auto *a, const auto *b) {
            return a->second < b->second;
        });
        for (const auto *pair : sortedComponents) {
            result.push_back(static_cast<T *>(pair->first.get()));
        }
        return result;
    }
    /// @brief 型から一致する最初のコンポーネントを取得
    /// @tparam T 取得したいコンポーネント型
    /// @return 一致するコンポーネント（存在しない場合は nullptr）
    template<typename T>
    T *GetComponent() const {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        size_t typeIndex = IObjectComponent::GetComponentTypeID<T>();
        if (typeIndex >= componentsIndexByType_.size()) return nullptr;
        const auto &indices = componentsIndexByType_[typeIndex];
        for (size_t idx : indices) {
            if (idx < components_.size()) {
                return static_cast<T *>(components_[idx].first.get());
            }
        }
        return nullptr;
    }
    /// @brief ポインタからコンポーネントを取得
    /// @param component コンポーネントのポインタ
    /// @return コンポーネント（存在しない場合は nullptr）
    IObjectComponent *GetComponent(const IObjectComponent *component) const;

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
    size_t HasComponent(const IObjectComponent *component) const;

    /// @brief 全コンポーネントの取得（コンポーネント本体と追加順のペアのリスト）
    const std::vector<std::pair<std::unique_ptr<IObjectComponent>, size_t>> &GetAllComponents() const { return components_; }

    //==================================================
    // コンポーネント追加系メソッド
    //==================================================

    /// @brief 既存コンポーネントの追加
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    IObjectComponent *AddComponent(std::unique_ptr<IObjectComponent> comp);
    /// @brief 既存コンポーネントの追加
    /// @tparam T コンポーネントの型
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    template<typename T>
    T *AddComponent(std::unique_ptr<T> comp) {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        return static_cast<T *>(AddComponent(std::unique_ptr<IObjectComponent>(std::move(comp))));
    }
    /// @brief コンポーネントの追加（生成）
    /// @tparam T コンポーネントの型
    /// @tparam Args コンポーネントのコンストラクタ引数の型
    /// @param args コンポーネントのコンストラクタ引数
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    template<typename T, typename... Args>
    T *AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        try {
            auto comp = std::make_unique<T>(std::forward<Args>(args)...);
            return static_cast<T *>(AddComponent(std::unique_ptr<IObjectComponent>(std::move(comp))));
        } catch (...) { return nullptr; }
    }

    //==================================================
    // コンポーネント削除系メソッド
    //==================================================

    /// @brief ポインタからコンポーネントを削除
    /// @param component 削除したいコンポーネントのポインタ
    /// @return 削除に成功した場合は true
    bool RemoveComponent(const IObjectComponent *component);
    /// @brief 型から一致する最初のコンポーネントを削除
    /// @tparam T 削除したいコンポーネントの型
    /// @return 削除に成功した場合は true
    template<typename T>
    bool RemoveComponent() {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        T *comp = GetComponent<T>();
        if (!comp) return false;
        return RemoveComponent(comp);
    }
    /// @brief 型から一致する全てのコンポーネントを削除
    /// @tparam T 削除したいコンポーネントの型
    /// @return 削除に成功した場合は true
    template <typename T>
    bool RemoveComponents() {
        static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
        bool allRemoved = true;
        while (true) {
            T *comp = GetComponent<T>();
            if (!comp) break;
            if (!RemoveComponent(comp)) {
                allRemoved = false;
                break;
            }
        }
        return allRemoved;
    }
    /// @brief 全コンポーネントを削除
    void ClearComponents();

    /// @brief オブジェクトIDの設定
    void SetObjectID(const UUID128 &id) { objectID_ = id; }
    /// @brief オブジェクトIDの取得
    const UUID128 &GetObjectID() const { return objectID_; }
    /// @brief オブジェクトの保存の可否設定
    void SetSaveEnabled(bool enabled) { isSaveEnabled_ = enabled; }
    /// @brief オブジェクトの保存の可否取得
    bool IsSaveEnabled() const { return isSaveEnabled_; }

    /// @brief オブジェクトのアクティブ状態設定
    /// @details 有効化時に全コンポーネントのInitialize、無効化時に全コンポーネントのFinalizeが走る。
    ///          子孫オブジェクト自身の isActive フラグは変更しないが、実効アクティブ状態
    ///          （祖先が無効なら自身も無効として扱われる）が変化した子孫についても
    ///          同様にInitialize/Finalizeが走る。
    void SetActive(bool active);
    /// @brief オブジェクトのアクティブ状態取得
    bool IsActive() const;

    //==================================================
    // JSON保存/読み込み系メソッド
    //==================================================

    /// @brief オブジェクト情報をjsonへ保存
    JSON SaveToJson(Passkey<Scene>);
    /// @brief オブジェクト情報をjsonから読み込み
    bool LoadFromJson(Passkey<Scene>, const JSON &json);

    //==================================================
    // コンポーネント単体のJSONスナップショット（エディターのUndo/Redo用）
    //==================================================

    /// @brief 所有コンポーネント単体をjsonへ保存（type と data を含む）
    JSON SaveComponentToJson(const IObjectComponent *component) const {
        JSON json = JSON::object();
        auto *comp = GetComponent(component);
        if (!comp) return json;
        json["type"] = comp->GetComponentType();
        json["data"] = comp->SaveToJsonInterface(Passkey<EmptyObject>{});
        return json;
    }
    /// @brief jsonからコンポーネントを生成して追加する
    IObjectComponent *AddComponentFromJson(const JSON &json) {
        const std::string type = json.value("type", "");
        if (type.empty()) return nullptr;
        auto comp = CreateObjectComponentByType(type);
        auto *ptr = AddComponent(std::move(comp));
        if (ptr && json.contains("data")) {
            ptr->LoadFromJsonInterface(Passkey<EmptyObject>{}, json["data"]);
        }
        return ptr;
    }
    /// @brief 所有コンポーネントの状態をjsonから復元する
    bool LoadComponentFromJson(IObjectComponent *component, const JSON &json) {
        auto *comp = GetComponent(component);
        if (!comp) return false;
        const JSON &data = json.contains("data") ? json["data"] : json;
        return comp->LoadFromJsonInterface(Passkey<EmptyObject>{}, data);
    }

#if defined(USE_IMGUI)
    /// @brief 所有コンポーネントの ImGui 表示（ウィンドウの Begin/End は呼ばない）
    void ShowComponentImGui(IObjectComponent *component) {
        if (GetComponent(component)) {
            component->ShowImGuiInterface(Passkey<EmptyObject>{});
        }
    }

    /// @brief 全コンポーネントの常時ImGui表示（ゲームループがポーズ中でも毎フレーム呼ばれる）
    void ShowPersistentImGui(Passkey<Scene>) {
        if (!IsActive()) return;
        for (auto &pair : components_) {
            if (pair.first) {
                pair.first->ShowPersistentImGuiInterface(Passkey<EmptyObject>{});
            }
        }
    }
#endif

private:
    friend class ObjectContext;

    void Initialize();
    void Finalize();
    void Update();
    void RegenerateUpdateComponentsList();
    /// @brief シーン内から自身の子孫オブジェクトを探し、変更前の実効アクティブ状態を記録する（SetActive用）
    void CollectDescendantsActiveState(std::vector<std::pair<EmptyObject *, bool>> &out) const;

    std::string name_ = "EmptyObject";

    /// @brief コンポーネントのリスト（ペア: コンポーネント本体, コンポーネントが追加された順番）
    std::vector<std::pair<std::unique_ptr<IObjectComponent>, size_t>> components_;
    std::vector<std::vector<size_t>> componentsIndexByType_;
    std::unordered_map<const IObjectComponent *, size_t> componentsIndexByPointer_;
    std::vector<size_t> componentsFreeIndices_;
    size_t nextAddedID_ = 0;

    struct UpdateComponentInfo {
        size_t addedID;
        int priority;
        IObjectComponent *component;
    };
    std::vector<UpdateComponentInfo> updateComponents_;

    SceneContext *ownerSceneContext_ = nullptr;
    std::unique_ptr<ObjectContext> objectContext_;
    UUID128 objectID_ = UUID128(true);
    bool isSaveEnabled_ = true;

    bool isActive_ = true;
};

} // namespace KashipanEngine
