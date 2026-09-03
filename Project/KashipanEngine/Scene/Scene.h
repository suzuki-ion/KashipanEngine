#pragma once
#pragma once

#include <array>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Objects/EmptyObject.h"
#include "Objects/Collision/Collider.h"
#include "Objects/ChunkedPool.h"
#include "Objects/ComponentPool.h"
#include "ComponentSerialize/ComponentRegistry.h"
#include "Scene/Components/ISceneComponent.h"
#include "Utilities/Passkeys.h"
#include "Utilities/UUID128.h"
#include "Utilities/MyAny.h"

namespace KashipanEngine {

class SceneContext;
#ifdef USE_IMGUI
class SceneEditor;
class SceneEditorContext;
#endif

class SceneManager;
class GameEngine;

class AudioManager;
class ModelManager;
class SkeletonManager;
class SamplerManager;
class TextureManager;
class AnimationManager;
class MaterialManager;
class Input;
class InputCommand;
class DirectXCommon;
class GraphicsEngine;

/// @brief シーンクラス
class Scene final {
    friend class SceneContext;
#ifdef USE_IMGUI
    friend class SceneEditorContext;
#endif
public:
    /// @brief GameEngine から DirectXCommon を設定（Play開始/終了時のGPU同期に使う）
    static void SetDirectXCommon(Passkey<GameEngine>, DirectXCommon *dx) { sDirectXCommon_ = dx; }
    /// @brief GameEngine から GraphicsEngine を設定
    /// @details PlayStop()がシーン内の全オブジェクト・コンポーネントを作り直す際、
    ///          Rendererの構造化バッファ等のキャッシュ（古いインスタンス由来のキーが
    ///          溜まり続けディスクリプタヒープを枯渇させる）を破棄するために使う
    static void SetGraphicsEngine(Passkey<GameEngine>, GraphicsEngine *graphicsEngine) { sGraphicsEngine_ = graphicsEngine; }

    explicit Scene(const std::string &sceneName);
    explicit Scene(const JSON &sceneData);

    Scene() = delete;
    virtual ~Scene();

    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;
    Scene(Scene &&) = delete;
    Scene &operator=(Scene &&) = delete;

    const std::string &GetName() const { return name_; }
    const std::string &GetNextSceneName() const { return nextSceneName_; }
    SceneContext *GetSceneContext() const { return sceneContext_.get(); }
    /// @brief シーンの識別ID（シーンJSONの"sceneID"）を取得する
    /// @details シーン名は変更されうるため、シーンごとのエディター設定（EditorSettings）の
    ///          キーにはこちらを使う（SceneEditorView::EnsureSceneViewObject参照）
    const UUID128 &GetSceneID() const noexcept { return sceneID_; }

    void InitializeInterface(Passkey<SceneManager>) { OnInitialize(); }
    void FinalizeInterface(Passkey<SceneManager>) { OnFinalize(); }
    void UpdateInterface(Passkey<SceneManager>) {
#if defined(USE_IMGUI)
        // エディター実行時はPlay中（かつ一時停止していないか、1フレーム進める要求がある）場合のみ更新する
        if (!isPlaying_ || (isPaused_ && !isStepFrameRequested_)) return;
        isStepFrameRequested_ = false;
#endif
        // このスコープ中のDeleteObjectは実体の破棄を遅延させる（後述のisProcessingObjectLifecycle_を参照）
        isProcessingObjectLifecycle_ = true;
        UpdateSceneObjects();
        UpdateComponents();
        OnUpdate();
        isProcessingObjectLifecycle_ = false;
        // Update中に溜まった削除待ちオブジェクトを、更新が完全に終わった安全なタイミングで実際に破棄する
        FlushPendingDestroys();
    }

#if defined(USE_IMGUI)
    void ShowImGuiInterface(Passkey<SceneManager>);

    /// @brief 所有シーンコンポーネントの ImGui 表示（ウィンドウの Begin/End は呼ばない）
    void ShowComponentImGui(ISceneComponent *component) {
        if (GetComponent(component)) {
            component->ShowImGuiInterface(Passkey<Scene>{});
        }
    }
#endif

    //==================================================
    // 再生状態（エディターではUnity風のPlay/Pause/Stopで制御する）
    //==================================================

#if defined(USE_IMGUI)
    /// @brief 再生中かどうか
    bool IsPlaying() const { return isPlaying_; }
    /// @brief 一時停止中かどうか
    bool IsPaused() const { return isPaused_; }
    /// @brief 再生を開始する（開始前のシーン状態を保存する）
    void PlayStart();
    /// @brief 再生を終了する（開始前のシーン状態へ復元する）
    void PlayStop();
    /// @brief 一時停止する
    void PlayPause() { isPaused_ = true; }
    /// @brief 一時停止を解除する
    void PlayResume() { isPaused_ = false; }
    /// @brief 1フレームだけ進める（一時停止中のみ有効）
    void RequestStepFrame() { isStepFrameRequested_ = true; }
#else
    /// @brief 再生中かどうか（非エディタービルドではシーンが動いている間は常にtrue）
    bool IsPlaying() const { return true; }
    /// @brief 一時停止中かどうか（非エディタービルドには一時停止の概念が無いため常にfalse）
    bool IsPaused() const { return false; }
#endif

    /// @brief ゲームループの終了を要求する
    /// @details 非エディタービルドではゲームループ（アプリケーション）が終了する。
    ///          エディタービルドではエディター自体は閉じず、再生停止（PlayStop）の要求として扱われる。
    ///          定義はGameEngineの完全な型定義が必要なためScene.cppにある
    static void RequestExitGameLoop();

    /// @brief シーンの保存
    /// @return シーンのJSONデータ
    JSON SaveToJSON() const;
    /// @brief シーンの読み込み
    /// @param json シーンのJSONデータ
    /// @return 読み込みに成功した場合は true、失敗した場合は false を返す
    bool LoadFromJSON(const JSON &json);

    void SetSceneManager(Passkey<SceneManager>, SceneManager *sceneManager) { sceneManager_ = sceneManager; }

    static void SetEnginePointers(
        Passkey<GameEngine>,
        AudioManager *audioManager,
        ModelManager *modelManager,
        SkeletonManager *skeletonManager,
        SamplerManager *samplerManager,
        TextureManager *textureManager,
        AnimationManager *animationManager,
        MaterialManager *materialManager,
        Input *input,
        InputCommand *inputCommand);

protected:
    /// @brief シーン初期化（任意の初期化処理を行う場合はこの関数をオーバーライドする）
    virtual void OnInitialize() {}
    /// @brief シーン終了処理（任意の終了処理を行う場合はこの関数をオーバーライドする）
    virtual void OnFinalize() {}
    /// @brief シーン更新処理（任意の更新処理を行う場合はこの関数をオーバーライドする）
    virtual void OnUpdate() {}

    //==================================================
    // シーン変数
    //==================================================

    /// @brief シーン変数を追加する
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param value 変数の値
    /// @return 追加されたシーン変数のポインタ
    template <typename T>
    MyAny *AddSceneVariable(const std::string &key, const T &value = T()) {
        sceneVariables_[key] = value;
        return &sceneVariables_[key];
    }
    /// @brief シーン変数を削除する
    /// @param key 変数のキー
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool RemoveSceneVariable(const std::string &key);
    /// @brief シーン変数の情報を取得する
    /// @param key 変数のキー
    /// @return シーン変数のポインタ（存在しない場合は nullptr）
    MyAny *GetSceneVariable(const std::string &key);
    /// @brief シーン変数の型を取得する
    /// @param key 変数のキー
    /// @return シーン変数の型情報
    const TypeInfo &GetSceneVariableTypeInfo(const std::string &key);
    /// @brief シーン変数を取得する
    /// @return シーン変数のマップ
    const std::unordered_map<std::string, MyAny> &GetSceneVariables() const { return sceneVariables_; }

    //==================================================
    // グローバルシーン変数
    //==================================================

    /// @brief グローバルシーン変数を追加する
    /// @tparam T 変数の型
    /// @param key 変数のキー
    /// @param value 変数の値
    /// @return 追加されたシーン変数のポインタ
    template <typename T>
    MyAny *AddGlobalSceneVariable(const std::string &key, const T &value = T()) { return AddGlobalSceneVariableInternal(key, value, GetValueType<T>()); }
    /// @brief グローバルシーン変数を削除する
    /// @param key 変数のキー
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool RemoveGlobalSceneVariable(const std::string &key) { return RemoveGlobalSceneVariableInternal(key); }
    /// @brief グローバルシーン変数の情報を取得する
    /// @param key 変数のキー
    /// @return シーン変数のポインタ（存在しない場合は nullptr）
    MyAny *GetGlobalSceneVariable(const std::string &key) { return GetGlobalSceneVariableInternal(key); }
    /// @brief グローバルシーン変数の型を取得する
    /// @param key 変数のキー
    /// @return シーン変数の型情報
    const TypeInfo &GetGlobalSceneVariableTypeInfo(const std::string &key);
    /// @brief グローバルシーン変数を取得する
    /// @return シーン変数のマップ
    const std::unordered_map<std::string, MyAny> &GetGlobalSceneVariables() const { return GetGlobalSceneVariablesInternal(); }

    //==================================================
    // シーン内オブジェクト管理
    //==================================================

    /// @brief 空のオブジェクトを生成
    /// @param name 空のオブジェクト名
    /// @param index 生成位置のインデックス（省略時は末尾に追加）
    /// @return 生成された空のオブジェクトのポインタ
    EmptyObject *CreateEmptyObject(const std::string &name = "", const UUID128 &objectID = UUID128(), size_t index = MAXSIZE_T);
    /// @brief 既存オブジェクトを複製してシーンへ追加する
    /// @details 複製されるのは対象オブジェクト自身のコンポーネントのみで、親子関係や子オブジェクトは複製されない
    ///          （EmptyObject::Clone() の仕様に準じる）
    /// @param source 複製元オブジェクトのポインタ（このシーンに属している必要がある）
    /// @param name 複製後のオブジェクト名（空の場合は複製元と同じ名前になる）
    /// @return 複製されたオブジェクトのポインタ（失敗した場合は nullptr）
    EmptyObject *CloneObject(EmptyObject *source, const std::string &name = "");
    /// @brief オブジェクトを削除
    /// @param obj 削除するオブジェクトのポインタ
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool DeleteObject(EmptyObject *obj);
    /// @brief 3D オブジェクトを解放（所有権の放棄をし、シーンから削除。インスタンスの解放は行わない）
    /// @param obj 解放する 3D オブジェクトのポインタ
    /// @return 解放に成功した場合は true、失敗した場合は false を返す
    bool ReleaseObject(EmptyObject *obj);
    /// @brief オブジェクトを移動
    /// @param 移動するオブジェクトのポインタ
    /// @param newIndex 移動先のインデックス
    /// @return 移動に成功した場合は true、失敗した場合は false を返す
    bool MoveObject(EmptyObject *obj, size_t newIndex);

    /// @brief シーン内のオブジェクト一覧を取得（表示順。実体はチャンク方式のプールが所有する）
    /// @return オブジェクトのポインタのリスト
    const std::vector<EmptyObject *> &GetSceneObjects() const { return objects_; }
    /// @brief 名前から一致するオブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタのリスト（存在しない場合は空のリスト）
    std::vector<EmptyObject *> GetSceneObjects(const std::string &objectName) const;
    /// @brief 名前から一致する最初のオブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(const std::string &objectName) const;
    /// @brief ポインタから一致するオブジェクトを取得
    /// @param obj オブジェクトのポインタ
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(EmptyObject *obj) const;
    /// @brief UUIDから一致するオブジェクトを取得
    /// @param uuid オブジェクトのUUID
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    EmptyObject *GetSceneObject(const UUID128 &uuid) const;

    /// @brief シーン内のオブジェクトをすべて削除
    void ClearSceneObjects();

    /// @brief EditorOnlyオブジェクトを（子孫ごと）すべて削除する
    /// @details 再生開始時（PlayStart）と、エディター無しビルドでのシーン読み込み時に呼ばれる
    void DeleteEditorOnlyObjects();

    //==================================================
    // オブジェクト単体のJSONスナップショット（エディターのUndo/Redo用）
    //==================================================

    /// @brief オブジェクト単体をjsonへ保存する
    JSON SaveObjectToJson(EmptyObject *obj) {
        if (!obj || !GetSceneObject(obj)) return JSON::object();
        return obj->SaveToJson(Passkey<Scene>{});
    }
    /// @brief jsonからオブジェクトを生成する
    EmptyObject *CreateObjectFromJson(const JSON &json, size_t index = MAXSIZE_T) {
        const std::string name = json.value("name", "EmptyObject");
        const UUID128 objectID(json.value("objectID", std::string{}));
        auto *obj = CreateEmptyObject(name, objectID, index);
        if (obj) {
            obj->LoadFromJson(Passkey<Scene>{}, json);
        }
        return obj;
    }
    /// @brief オブジェクトのシーン内インデックスを取得する（存在しない場合は MAXSIZE_T）
    size_t GetObjectIndex(const EmptyObject *obj) const {
        for (size_t i = 0; i < objects_.size(); ++i) {
            if (objects_[i] == obj) return i;
        }
        return MAXSIZE_T;
    }

    //==================================================
    // コンポーネント取得系メソッド
    //==================================================

    /// @brief 型から一致するコンポーネント一覧を取得
    /// @tparam T コンポーネントの型
    /// @return 一致するコンポーネントのリスト（存在しない場合は空のリスト）
    template<typename T>
    std::vector<T *> GetComponents() const {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        std::vector<T *> result;
        size_t typeIndex = ISceneComponent::GetComponentTypeID<T>();
        if (typeIndex >= componentsIndexByType_.size()) return result;
        const auto &indices = componentsIndexByType_[typeIndex];
        for (size_t idx : indices) {
            if (idx < components_.size()) {
                result.push_back(static_cast<T *>(components_[idx].first.get()));
            }
        }
        return result;
    }
    /// @brief 型から一致する最初のコンポーネントを取得
    /// @tparam T 取得したいコンポーネント型
    /// @return 一致するコンポーネント（存在しない場合は nullptr）
    template<typename T>
    T *GetComponent() const {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        size_t typeIndex = ISceneComponent::GetComponentTypeID<T>();
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
    ISceneComponent *GetComponent(const ISceneComponent *component) const;

    /// @brief 型からコンポーネントの個数を確認
    /// @tparam T コンポーネントの型
    /// @return 一致するコンポーネントの個数
    template<typename T>
    size_t HasComponents() const {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        size_t typeIndex = ISceneComponent::GetComponentTypeID<T>();
        if (typeIndex >= componentsIndexByType_.size()) return 0;
        return static_cast<size_t>(componentsIndexByType_[typeIndex].size());
    }
    /// @brief ポインタからコンポーネントの個数を確認
    /// @param component コンポーネントのポインタ
    /// @return 一致するコンポーネントの個数
    size_t HasComponent(const ISceneComponent *component) const;

    /// @brief 全コンポーネントの取得（コンポーネント本体と追加順のペアのリスト）
    const std::vector<std::pair<std::unique_ptr<ISceneComponent>, size_t>> &GetAllComponents() const { return components_; }

    //==================================================
    // コンポーネント追加系メソッド
    //==================================================

    /// @brief 既存コンポーネントの追加
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    ISceneComponent *AddComponent(std::unique_ptr<ISceneComponent> comp);
    /// @brief 既存コンポーネントの追加
    /// @tparam T コンポーネントの型
    /// @param comp 既存コンポーネント（ムーブされる）
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    template<typename T>
    T *AddComponent(std::unique_ptr<T> comp) {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        return static_cast<T *>(AddComponent(std::unique_ptr<ISceneComponent>(std::move(comp))));
    }
    /// @brief コンポーネントの追加（生成）
    /// @tparam T コンポーネントの型
    /// @tparam Args コンポーネントのコンストラクタ引数の型
    /// @param args コンポーネントのコンストラクタ引数
    /// @return 追加に成功した場合はコンポーネントのポインタ、失敗した場合は nullptr
    template<typename T, typename... Args>
    T *AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        try {
            auto comp = std::make_unique<T>(std::forward<Args>(args)...);
            return static_cast<T *>(AddComponent(std::unique_ptr<ISceneComponent>(std::move(comp))));
        } catch (...) { return nullptr; }
    }

    //==================================================
    // コンポーネント削除系メソッド
    //==================================================

    /// @brief ポインタからコンポーネントを削除
    /// @param component 削除したいコンポーネントのポインタ
    /// @return 削除に成功した場合は true
    bool RemoveComponent(const ISceneComponent *component);
    /// @brief 型から一致する最初のコンポーネントを削除
    /// @tparam T 削除したいコンポーネントの型
    /// @return 削除に成功した場合は true
    template<typename T>
    bool RemoveComponent() {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
        T *comp = GetComponent<T>();
        if (!comp) return false;
        return RemoveComponent(comp);
    }
    /// @brief 型から一致する全てのコンポーネントを削除
    /// @tparam T 削除したいコンポーネントの型
    /// @return 削除に成功した場合は true
    template <typename T>
    bool RemoveComponents() {
        static_assert(std::is_base_of_v<ISceneComponent, T>, "T must derive from ISceneComponent");
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
    void ClearSceneComponents();

    //==================================================
    // オブジェクトコンポーネント（IObjectComponent）用の型別プール
    //==================================================

    /// @brief 型IDからオブジェクトコンポーネント用プールを取得する（未作成の場合はComponentRegistry経由で生成）
    /// @param typeID IObjectComponent::GetComponentTypeID() が返す型ID
    /// @return プールへのポインタ（型が未登録の場合は nullptr）
    IComponentPoolBase *GetOrCreateComponentPool(size_t typeID) {
        if (typeID >= objectComponentPoolsByType_.size()) {
            objectComponentPoolsByType_.resize(typeID + 1);
        }
        if (!objectComponentPoolsByType_[typeID]) {
            objectComponentPoolsByType_[typeID] = CreateObjectComponentPoolByTypeID(typeID);
        }
        return objectComponentPoolsByType_[typeID].get();
    }
    /// @brief 型Tのオブジェクトコンポーネント用プールを取得する（未作成の場合は直接生成）
    /// @tparam T コンポーネントの型（コンパイル時に既知の場合はComponentRegistryを経由しない）
    template <typename T>
    ComponentPool<T> &GetOrCreateComponentPool() {
        size_t typeID = IObjectComponent::GetComponentTypeID<T>();
        if (typeID >= objectComponentPoolsByType_.size()) {
            objectComponentPoolsByType_.resize(typeID + 1);
        }
        if (!objectComponentPoolsByType_[typeID]) {
            objectComponentPoolsByType_[typeID] = std::make_unique<ComponentPool<T>>();
        }
        return static_cast<ComponentPool<T> &>(*objectComponentPoolsByType_[typeID]);
    }

    //==================================================
    // シーン切り替え系メソッド
    //==================================================

    /// @brief 次のシーン名を設定
    /// @param nextSceneName 次のシーン名
    void SetNextSceneName(const std::string &nextSceneName) { nextSceneName_ = nextSceneName; }
    /// @brief 次のシーンに切り替え
    /// @return 切り替えに成功した場合は true、失敗した場合は false
    bool ChangeToNextScene();
    /// @brief 次のシーン名をクリア
    void ClearNextSceneName() { nextSceneName_.clear(); }
    /// @brief 次のシーン名が設定されているかを確認
    /// @return 設定されている場合は true、設定されていない場合は false
    bool HasNextSceneName() const { return !nextSceneName_.empty(); }

    //==================================================
    // 各種マネージャーへのアクセス
    //==================================================

    static AudioManager *GetAudioManager() { return sAudioManager; }
    static ModelManager *GetModelManager() { return sModelManager; }
    static SkeletonManager *GetSkeletonManager() { return sSkeletonManager; }
    static SamplerManager *GetSamplerManager() { return sSamplerManager; }
    static TextureManager *GetTextureManager() { return sTextureManager; }
    static AnimationManager *GetAnimationManager() { return sAnimationManager; }
    static MaterialManager *GetMaterialManager() { return sMaterialManager; }
    static Input *GetInput() { return sInput; }
    static InputCommand *GetInputCommand() { return sInputCommand; }

private:
    static inline AudioManager *sAudioManager = nullptr;
    static inline ModelManager *sModelManager = nullptr;
    static inline SkeletonManager *sSkeletonManager = nullptr;
    static inline SamplerManager *sSamplerManager = nullptr;
    static inline TextureManager *sTextureManager = nullptr;
    static inline AnimationManager *sAnimationManager = nullptr;
    static inline MaterialManager *sMaterialManager = nullptr;
    static inline Input *sInput = nullptr;
    static inline InputCommand *sInputCommand = nullptr;
    static inline DirectXCommon *sDirectXCommon_ = nullptr;
    static inline GraphicsEngine *sGraphicsEngine_ = nullptr;

    void UpdateSceneObjects();
    void UpdateComponents();
    void RegenerateUpdateComponentsList();
    void RemoveObjectFromMaps(EmptyObject *obj);
    void FlushPendingDestroys();

    std::string name_;

    //==================================================
    // オブジェクト
    //==================================================

    /// @brief オブジェクトの実体を所有するチャンク方式プール（要素は絶対に再配置されない）
    ChunkedPool<EmptyObject> objectPool_;
    /// @brief シーン内での表示・保存順を保持する非所有ポインタのリスト（実体は objectPool_ が所有）
    std::vector<EmptyObject *> objects_;
    /// @brief オブジェクト/コンポーネントの更新処理中（UpdateInterface実行中）かどうか。
    ///        trueの間にDeleteObjectが呼ばれた場合、実体の破棄をFlushPendingDestroysまで遅延する。
    ///        （スクリプトが自分自身の所有オブジェクトを削除すると、実行中のScriptComponent自体を
    ///          即座に破棄してしまい use-after-free になるため）
    bool isProcessingObjectLifecycle_ = false;
    /// @brief isProcessingObjectLifecycle_中にDeleteObjectされ、実体の破棄を待っているオブジェクト
    std::vector<EmptyObject *> pendingDestroyObjects_;
    std::unordered_map<UUID128, EmptyObject *> objectsByUUID_;
    std::unordered_set<EmptyObject *> objectsExistingSet_;
    std::unordered_map<std::string, std::unordered_set<EmptyObject *>> objectsByName_;

    //==================================================
    // オブジェクトコンポーネント（IObjectComponent）のプール
    //==================================================

    /// @brief 型IDでインデックスされたコンポーネントプール（実体はここが所有する）
    std::vector<std::unique_ptr<IComponentPoolBase>> objectComponentPoolsByType_;

    //==================================================
    // シーンコンポーネント
    //==================================================

    /// @brief コンポーネントのリスト（ペア: コンポーネント本体, コンポーネントが追加された順番）
    std::vector<std::pair<std::unique_ptr<ISceneComponent>, size_t>> components_;
    std::vector<std::vector<size_t>> componentsIndexByType_;
    std::unordered_map<const ISceneComponent *, size_t> componentsIndexByPointer_;
    std::vector<size_t> componentsFreeIndices_;
    size_t nextAddedComponentID_ = 0;

    struct UpdateComponentInfo {
        size_t addedID;
        int priority;
        ISceneComponent *component;
    };
    std::vector<UpdateComponentInfo> updateComponents_;

    //==================================================
    // シーンのコンテキストとエディタ
    //==================================================

    std::unique_ptr<SceneContext> sceneContext_;
#ifdef USE_IMGUI
    std::unique_ptr<SceneEditorContext> sceneEditorContext_;
    std::unique_ptr<SceneEditor> sceneEditor_;

    /// @brief 再生中かどうか（エディター上でのUnity風Play/Stop用）
    bool isPlaying_ = false;
    /// @brief 一時停止中かどうか
    bool isPaused_ = false;
    /// @brief 1フレームだけ進める要求フラグ（一時停止中のみ有効）
    bool isStepFrameRequested_ = false;
    /// @brief 再生開始前のシーン状態のスナップショット（Stop時に復元する）
    JSON editModeSnapshot_;
#endif

    std::string nextSceneName_;
    SceneManager *sceneManager_ = nullptr;

    //==================================================
    // シーン変数
    //==================================================

    std::unordered_map<std::string, MyAny> sceneVariables_;

    //==================================================
    // グローバルシーン変数
    //==================================================

    MyAny *AddGlobalSceneVariableInternal(const std::string &key, const MyAny &value, const TypeInfo &typeInfo);
    bool RemoveGlobalSceneVariableInternal(const std::string &key);
    MyAny *GetGlobalSceneVariableInternal(const std::string &key);
    const std::unordered_map<std::string, MyAny> &GetGlobalSceneVariablesInternal() const;

    //==================================================
    // シーンID
    //==================================================

    UUID128 sceneID_ = UUID128(true);
};

}// namespace KashipanEngine
