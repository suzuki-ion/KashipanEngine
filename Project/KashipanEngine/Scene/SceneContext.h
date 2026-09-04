#pragma once

#include <any>
#include <string>
#include <type_traits>
#include <vector>

#include "Scene/Scene.h"
#include "Objects/ComponentRef.h"

namespace KashipanEngine {

/// @brief Scene コンテキスト
class SceneContext final {
public:
    SceneContext() = delete;
    SceneContext(Passkey<Scene>, Scene *owner) : owner_(owner) {}
    ~SceneContext() = default;

    SceneContext(const SceneContext &) = delete;
    SceneContext &operator=(const SceneContext &) = delete;
    SceneContext(SceneContext &&) = delete;
    SceneContext &operator=(SceneContext &&) = delete;

    const std::string &GetName() const { return owner_->GetName(); }
    /// @brief シーンが再生中かどうか（デバッグ用の一時オブジェクト管理等に使う）
    bool IsPlaying() const { return owner_->IsPlaying(); }

    //==================================================
    // シーン変数
    //==================================================

    /// @brief シーン変数を追加する
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param value 変数の値
    /// @return 追加されたシーン変数のポインタ
    template <typename T>
    MyAny *AddSceneVariable(const std::string &key, const T &value = T()) { return owner_->AddSceneVariable(key, value); }
    /// @brief シーン変数を削除する
    /// @param key 変数のキー
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool RemoveSceneVariable(const std::string &key) { return owner_->RemoveSceneVariable(key); }
    /// @brief シーン変数の情報を取得する
    /// @param key 変数のキー
    /// @return シーン変数のポインタ（存在しない場合は nullptr）
    MyAny *GetSceneVariable(const std::string &key) { return owner_->GetSceneVariable(key); }
    /// @brief シーン変数の型を取得する
    /// @param key 変数のキー
    /// @return シーン変数の型情報
    const TypeInfo &GetSceneVariableTypeInfo(const std::string &key) { return owner_->GetSceneVariableTypeInfo(key); }
    /// @brief シーン変数を取得する
    /// @return シーン変数のマップ
    const std::unordered_map<std::string, MyAny> &GetSceneVariables() const { return owner_->GetSceneVariables(); }

    //==================================================
    // グローバルシーン変数
    //==================================================

    /// @brief グローバルシーン変数を追加する
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param value 変数の値
    /// @return 追加されたシーン変数のポインタ
    template <typename T>
    MyAny *AddGlobalSceneVariable(const std::string &key, const T &value = T()) { return owner_->AddGlobalSceneVariable(key, value); }
    /// @brief グローバルシーン変数を削除する
    /// @param key 変数のキー
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool RemoveGlobalSceneVariable(const std::string &key) { return owner_->RemoveGlobalSceneVariable(key); }
    /// @brief グローバルシーン変数の情報を取得する
    /// @param key 変数のキー
    /// @return シーン変数のポインタ（存在しない場合は nullptr）
    MyAny *GetGlobalSceneVariable(const std::string &key) { return owner_->GetGlobalSceneVariable(key); }
    /// @brief グローバルシーン変数の型を取得する
    /// @param key 変数のキー
    /// @return シーン変数の型情報
    const TypeInfo &GetGlobalSceneVariableTypeInfo(const std::string &key) { return owner_->GetGlobalSceneVariableTypeInfo(key); }
    /// @brief グローバルシーン変数を取得する
    /// @return シーン変数のマップ
    const std::unordered_map<std::string, MyAny> &GetGlobalSceneVariables() const { return owner_->GetGlobalSceneVariables(); }

    //==================================================
    // シーン内オブジェクト管理
    //==================================================

    /// @brief 空のオブジェクトを生成
    /// @param name 空のオブジェクト名
    /// @param index 生成位置のインデックス（省略時は末尾に追加）
    /// @return 生成された空のオブジェクトのポインタ
    EmptyObject *CreateEmptyObject(const std::string &name = "", const UUID128 &objectID = UUID128(), size_t index = MAXSIZE_T) { return owner_->CreateEmptyObject(name, objectID, index); }
    /// @brief 既存オブジェクトを複製してシーンへ追加する
    /// @param source 複製元オブジェクトのポインタ（このシーンに属している必要がある）
    /// @param name 複製後のオブジェクト名（空の場合は複製元と同じ名前になる）
    /// @param includeChildren 子孫オブジェクトもまとめて複製するか
    /// @return 複製されたオブジェクトのポインタ（失敗した場合は nullptr）
    EmptyObject *CloneObject(EmptyObject *source, const std::string &name = "", bool includeChildren = false) { return owner_->CloneObject(source, name, includeChildren); }
    /// @brief オブジェクトを削除
    /// @param obj 削除するオブジェクトのポインタ
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool DeleteObject(EmptyObject *obj) { return owner_->DeleteObject(obj); }
    /// @brief 3D オブジェクトを解放（所有権の放棄をし、シーンから削除。インスタンスの解放は行わない）
    /// @param obj 解放する 3D オブジェクトのポインタ
    /// @return 解放に成功した場合は true、失敗した場合は false を返す
    bool ReleaseObject(EmptyObject *obj) { return owner_->ReleaseObject(obj); }
    /// @brief オブジェクトを移動
    /// @param 移動するオブジェクトのポインタ
    /// @param newIndex 移動先のインデックス
    /// @return 移動に成功した場合は true、失敗した場合は false を返す
    bool MoveObject(EmptyObject *obj, size_t newIndex) { return owner_->MoveObject(obj, newIndex); }

    /// @brief シーン内のオブジェクト一覧を取得
    /// @return オブジェクトのポインタのリスト
    const std::vector<EmptyObject *> &GetSceneObjects() const { return owner_->GetSceneObjects(); }
    /// @brief 名前から一致するオブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタのリスト（存在しない場合は空のリスト）
    std::vector<EmptyObject *> GetSceneObjects(const std::string &objectName) const { return owner_->GetSceneObjects(objectName); }
    /// @brief 名前から一致する最初のオブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(const std::string &objectName) const { return owner_->GetSceneObject(objectName); }
    /// @brief ポインタから一致するオブジェクトを取得
    /// @param obj オブジェクトのポインタ
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(EmptyObject *obj) const { return owner_->GetSceneObject(obj); }
    /// @brief UUIDから一致するオブジェクトを取得
    /// @param uuid オブジェクトのUUID
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(const UUID128 &uuid) const { return owner_->GetSceneObject(uuid); }

    /// @brief シーン内のオブジェクトをすべて削除
    void ClearSceneObjects() { owner_->ClearSceneObjects(); }

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
    ISceneComponent *GetComponent(const ISceneComponent *component) const { return owner_->GetComponent(component); }
    /// @brief 型からコンポーネントの個数を確認
    /// @tparam T コンポーネントの型
    /// @return 一致するコンポーネントの個数
    template<typename T>
    size_t HasComponents() const { return owner_->HasComponents<T>(); }
    /// @brief ポインタからコンポーネントの個数を確認
    /// @param component コンポーネントのポインタ
    /// @return 一致するコンポーネントの個数
    size_t HasComponent(const ISceneComponent *component) const { return owner_->HasComponent(component); }
    /// @brief 全コンポーネントの取得（コンポーネント本体と追加順のペアのリスト）
    const std::vector<std::pair<std::unique_ptr<ISceneComponent>, size_t>> &GetAllComponents() const { return owner_->GetAllComponents(); }

    //==================================================
    // オブジェクトコンポーネント（IObjectComponent）用の型別プール
    //==================================================

    /// @brief 型IDからオブジェクトコンポーネント用プールを取得する（未作成の場合は生成）
    IComponentPoolBase *GetOrCreateComponentPool(size_t typeID) { return owner_->GetOrCreateComponentPool(typeID); }
    /// @brief 型Tのオブジェクトコンポーネント用プールを取得する（未作成の場合は生成）
    template <typename T>
    ComponentPool<T> &GetOrCreateComponentPool() { return owner_->GetOrCreateComponentPool<T>(); }
    /// @brief ComponentRef からコンポーネントの生ポインタへ解決する（使う直前に毎回呼ぶこと。結果をフレームをまたいで保持しない）
    /// @return 解決に成功した場合はコンポーネントへのポインタ、対象オブジェクト・コンポーネントが既に存在しない場合は nullptr
    IObjectComponent *ResolveComponent(const ComponentRef &ref) const {
        if (!ref.IsValid()) return nullptr;
        EmptyObject *obj = GetSceneObject(ref.ownerObjectID);
        return obj ? obj->GetComponentByAddedID(ref.addedID) : nullptr;
    }

    //==================================================
    // コンポーネント追加系メソッド
    //==================================================

    /// @brief 既存コンポーネントの追加
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    ISceneComponent *AddComponent(std::unique_ptr<ISceneComponent> comp) { return owner_->AddComponent(std::move(comp)); }
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
    bool RemoveComponent(const ISceneComponent *component) { return owner_->RemoveComponent(component); }
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
    /// @brief 全コンポーネントを削除
    void ClearSceneComponents() { owner_->ClearSceneComponents(); }

    //==================================================
    // シーン切り替え系メソッド
    //==================================================

    /// @brief 次のシーン名を設定
    /// @param nextSceneName 次のシーン名
    void SetNextSceneName(const std::string &nextSceneName) { owner_->SetNextSceneName(nextSceneName); }
    /// @brief 次のシーンに切り替え
    /// @return 切り替えに成功した場合は true、失敗した場合は false
    bool ChangeToNextScene() { return owner_->ChangeToNextScene(); }
    /// @brief 次のシーン名をクリア
    void ClearNextSceneName() { owner_->ClearNextSceneName(); }
    /// @brief 次のシーン名が設定されているかを確認
    /// @return 設定されている場合は true、設定されていない場合は false
    bool HasNextSceneName() const { return owner_->HasNextSceneName(); }

    //==================================================
    // ゲームループ制御
    //==================================================

    /// @brief ゲームループの終了を要求する
    /// @details 非エディタービルドではゲームループ（アプリケーション）が終了する。
    ///          エディタービルドではエディター自体は閉じず、再生停止の要求として扱われる
    void RequestExitGameLoop() { Scene::RequestExitGameLoop(); }

    //==================================================
    // 各種マネージャーへのアクセス
    //==================================================

    AudioManager *GetAudioManager() { return Scene::GetAudioManager(); }
    ModelManager *GetModelManager() { return Scene::GetModelManager(); }
    SkeletonManager *GetSkeletonManager() { return Scene::GetSkeletonManager(); }
    SamplerManager *GetSamplerManager() { return Scene::GetSamplerManager(); }
    TextureManager *GetTextureManager() { return Scene::GetTextureManager(); }
    AnimationManager *GetAnimationManager() { return Scene::GetAnimationManager(); }
    MaterialManager *GetMaterialManager() { return Scene::GetMaterialManager(); }
    Input *GetInput() { return Scene::GetInput(); }
    InputCommand *GetInputCommand() { return Scene::GetInputCommand(); }

private:
    Scene *owner_ = nullptr;
};

} // namespace KashipanEngine
