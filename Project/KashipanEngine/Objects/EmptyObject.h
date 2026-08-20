#pragma once
#include <algorithm>
#include "Objects/IObjectComponent.h"
#include "Objects/ComponentPool.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector4.h"
#include "Utilities/Tag.h"
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

    /// @brief 別オブジェクトの状態（タグ・全コンポーネント・アクティブ状態等）をこのオブジェクトへ複製する
    /// @details 複製されるのは source 自身のコンポーネントのみで、親子関係や子オブジェクトは複製されない。
    ///          objectID・prefabNodeIDは複製されない（このオブジェクト自身のものを維持する）
    void CopyStateFrom(Passkey<Scene>, const EmptyObject &source);

    void InitializeInterface(Passkey<Scene>) { Initialize(); }
    void FinalizeInterface(Passkey<Scene>) { Finalize(); }
    void UpdateInterface(Passkey<Scene>) { Update(); }

    void SetName(const std::string &name) { name_ = name; }
    const std::string &GetName() const { return name_; }

    /// @brief タグを設定する（文字列からハッシュ化される。空文字で未設定に戻る）
    void SetTag(const std::string &tagName) {
        tagName_ = tagName;
        tag_ = Tag(tagName);
    }
    /// @brief タグを取得する（比較用）
    const Tag &GetTag() const noexcept { return tag_; }
    /// @brief タグの文字列を取得する（表示・保存用）
    const std::string &GetTagName() const noexcept { return tagName_; }

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
        std::vector<const std::pair<IObjectComponent *, size_t> *> sortedComponents;
        for (size_t idx : indices) {
            if (idx < components_.size()) {
                sortedComponents.push_back(&components_[idx]);
            }
        }
        std::sort(sortedComponents.begin(), sortedComponents.end(), [](const auto *a, const auto *b) {
            return a->second < b->second;
        });
        for (const auto *pair : sortedComponents) {
            result.push_back(static_cast<T *>(pair->first));
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
                return static_cast<T *>(components_[idx].first);
            }
        }
        return nullptr;
    }
    /// @brief 追加順ID（addedID）から一致するコンポーネントを取得する
    /// @details ComponentRefの解決に使う。addedIDは削除されても再利用されないため、
    ///          プールのスロット再利用によるエイリアシングに左右されず安全に対象を一意に特定できる
    /// @param addedID 対象コンポーネントの追加順ID
    /// @return 一致するコンポーネント（存在しない場合は nullptr）
    IObjectComponent *GetComponentByAddedID(size_t addedID) const;
    /// @brief コンポーネントの追加順ID（addedID）を取得する（ComponentRefの作成に使う）
    /// @param component 対象コンポーネントのポインタ
    /// @return 追加順ID（存在しない場合は MAXSIZE_T）
    size_t GetComponentAddedID(const IObjectComponent *component) const {
        auto it = componentsIndexByPointer_.find(component);
        if (it == componentsIndexByPointer_.end() || it->second >= components_.size()) return MAXSIZE_T;
        return components_[it->second].second;
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

    /// @brief 全コンポーネントの取得（コンポーネント本体と追加順のペアのリスト。本体はSceneのプールが所有する非所有ポインタ）
    const std::vector<std::pair<IObjectComponent *, size_t>> &GetAllComponents() const { return components_; }

    /// @brief Transform・ScriptComponent以外の全コンポーネントをまとめて有効/無効化する
    /// @details 「見た目や当たり判定は消すが、スクリプト自身のロジック（タイマー等）は
    ///          動かし続けたい」場合に使う（例: 敵撃破時、パーティクル再生を待つ間だけ
    ///          スクリプトを生かしたまま他の全コンポーネントを止めたい、等）
    void SetComponentsActiveExceptTransformAndScript(bool active) {
        for (auto &[component, addedID] : components_) {
            if (!component) continue;
            const std::string &type = component->GetComponentType();
            if (type == "Transform" || type == "ScriptComponent") continue;
            component->SetActive(active);
        }
    }

    //==================================================
    // コンポーネント追加系メソッド
    //==================================================

    /// @brief 既存コンポーネントの追加
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    IObjectComponent *AddComponent(std::unique_ptr<IObjectComponent> comp);
    /// @brief 型IDからプールへ直接デフォルト構築で追加する（JSON経由の状態転送を伴わない高速パス）
    /// @details AddComponent<T>()（引数無し）専用。定義はSceneContextの完全な型定義が必要なため
    ///          EmptyObject.cppにある
    IObjectComponent *AddComponentByTypeID(size_t typeIndex);
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
        if constexpr (sizeof...(Args) == 0) {
            // 引数無し（実質全ての呼び出し箇所がこちら）の場合は、プールへ直接デフォルト構築する
            // 非テンプレートの高速パスを使う（一時インスタンスの構築やJSON経由の状態転送を避ける）。
            // SceneContextはこのヘッダ内では前方宣言のみのため、テンプレートから直接
            // GetOrCreateComponentPool<T>() を呼ぶことはできない（.cpp側でのみ可能）
            return static_cast<T *>(AddComponentByTypeID(IObjectComponent::GetComponentTypeID<T>()));
        } else {
            try {
                auto comp = std::make_unique<T>(std::forward<Args>(args)...);
                return static_cast<T *>(AddComponent(std::unique_ptr<IObjectComponent>(std::move(comp))));
            } catch (...) { return nullptr; }
        }
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

    /// @brief Prefab内の対応ノードを識別する安定IDの設定
    /// @details Prefabに関連しないオブジェクトでは無効なまま（Clone()でも複製されない）。
    ///          objectID_と異なり、配置・複製の際に新規発行されず、Prefabファイル内の対応ノードとの
    ///          突き合わせにのみ使われる
    void SetPrefabNodeID(const UUID128 &id) { prefabNodeID_ = id; }
    /// @brief Prefab内の対応ノードを識別する安定IDの取得
    const UUID128 &GetPrefabNodeID() const { return prefabNodeID_; }
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

    /// @brief EditorOnly（エディター専用）フラグを設定する
    /// @details EditorOnlyのオブジェクトはエディターのシーンビューにのみ描画され、
    ///          再生開始時・エディター無しビルドでのシーン読み込み時に（子孫ごと）削除される
    void SetEditorOnly(bool editorOnly) { isEditorOnly_ = editorOnly; }
    /// @brief EditorOnlyフラグを取得する（自身のフラグのみ。祖先は考慮しない）
    bool IsEditorOnly() const noexcept { return isEditorOnly_; }
    /// @brief 自身または祖先のいずれかがEditorOnlyかどうか（描画の除外判定用）
    bool IsEditorOnlyInHierarchy() const;

    /// @brief エディターのScene View等が使う「編集用描画先」への無条件描画から除外するかを設定する
    /// @details 通常、MeshRendererはSetTargetObjectでカスタム描画先を指定していても、
    ///          エディター用描画先（Scene View）には除外設定に関わらず常に描画される
    ///          （SceneRenderer::CollectSortableEntries参照）。デバッグ用の孤立プレビュー等、
    ///          カスタム描画先にのみ描画したく、Scene Viewには一切映り込ませたくないオブジェクトに使う。
    ///          JSONへは保存しない（実行時のみ使う一時的なフラグ）。既定はfalse＝従来通りの挙動
    void SetHiddenFromEditorTarget(bool hidden) noexcept { isHiddenFromEditorTarget_ = hidden; }
    /// @brief 編集用描画先への無条件描画から除外されているかを取得する
    bool IsHiddenFromEditorTarget() const noexcept { return isHiddenFromEditorTarget_; }

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
    /// @brief プールへ配置済みのコンポーネントを、このオブジェクトのローカルな管理台帳（空きスロット再利用含む）へ登録する
    IObjectComponent *RegisterPlacedComponent(IObjectComponent *placed, size_t typeIndex);

    std::string name_ = "EmptyObject";
    /// @brief タグ（比較用ハッシュ）と表示・保存用のタグ文字列
    Tag tag_;
    std::string tagName_;

    /// @brief コンポーネントのリスト（ペア: コンポーネント本体への非所有ポインタ（実体はSceneのプールが所有）, 追加された順番）
    std::vector<std::pair<IObjectComponent *, size_t>> components_;
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
    /// @brief Prefab内の対応ノードを識別する安定ID（Prefabに関連しないオブジェクトでは無効なまま）
    UUID128 prefabNodeID_;
    bool isSaveEnabled_ = true;

    bool isActive_ = true;
    /// @brief EditorOnly（エディター専用）フラグ
    bool isEditorOnly_ = false;
    /// @brief エディター用描画先（Scene View等）への無条件描画から除外するか（非シリアライズ）
    bool isHiddenFromEditorTarget_ = false;
};

} // namespace KashipanEngine
