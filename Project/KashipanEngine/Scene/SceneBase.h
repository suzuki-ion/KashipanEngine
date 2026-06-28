#pragma once
#pragma once

#include <array>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <vector>

#ifdef USE_IMGUI
#include "Scene/SceneEditor.h"
#endif
#include "Objects/Object2DBase.h"
#include "Objects/Object3DBase.h"
#include "Objects/Collision/Collider.h"
#include "Scene/Components/ISceneComponent.h"
#include "Utilities/Passkeys.h"

#include "Assets/ModelManager.h"

namespace KashipanEngine {

class SceneContext;
#ifdef USE_IMGUI
class SceneEditorContext;
#endif

class SceneManager;
class GameEngine;

class AudioManager;
class SkeletonManager;
class SamplerManager;
class TextureManager;
class AnimationManager;
class Input;
class InputCommand;

class SceneBase {
public:
    SceneBase() = delete;
    virtual ~SceneBase();

    SceneBase(const SceneBase &) = delete;
    SceneBase &operator=(const SceneBase &) = delete;
    SceneBase(SceneBase &&) = delete;
    SceneBase &operator=(SceneBase &&) = delete;

    const std::string &GetName() const { return name_; }
    const std::string &GetNextSceneName() const { return nextSceneName_; }

    void Update();

#if defined(USE_IMGUI)
    void ShowImGui();
#endif

    void SetSceneManager(Passkey<SceneManager>, SceneManager *sceneManager) { sceneManager_ = sceneManager; }

    static void SetEnginePointers(
        Passkey<GameEngine>,
        AudioManager *audioManager,
        ModelManager *modelManager,
        SkeletonManager *skeletonManager,
        SamplerManager *samplerManager,
        TextureManager *textureManager,
        AnimationManager *animationManager,
        Input *input,
        InputCommand *inputCommand);

    /// @brief シーン初期化（SceneManager から呼ばれる）
    virtual void Initialize() {}

    /// @brief シーン終了処理（SceneManager から呼ばれる）
    virtual void Finalize() {}

    friend class SceneContext;
#ifdef USE_IMGUI
    friend class SceneEditorContext;
#endif

protected:
    SceneBase(const std::string &sceneName);

    virtual void OnUpdate() {}

    template<typename T>
    bool TryGetSceneVariable(const std::string &key, T &out) const {
        const auto &vars = GetSceneVariables();
        if (!vars.contains(key)) return false;
        return vars.at(key).TryGetValue(out);
    }

    template<typename T>
    T GetSceneVariableOr(const std::string &key, const T &defaultValue) const {
        T v = defaultValue;
        (void)TryGetSceneVariable<T>(key, v);
        return v;
    }

    /// @brief 2D オブジェクトを追加
    /// @param obj 2D オブジェクトのユニークポインタ
    /// @return 追加に成功した場合は true、失敗した場合は false を返す
    bool AddObject2D(std::unique_ptr<Object2DBase> obj);
    /// @brief 3D オブジェクトを追加
    /// @param obj 3D オブジェクトのユニークポインタ
    /// @return 追加に成功した場合は true、失敗した場合は false を返す
    bool AddObject3D(std::unique_ptr<Object3DBase> obj);

    /// @brief 2D オブジェクトを指定したインデックスに挿入
    /// @param obj 2D オブジェクトのユニークポインタ
    /// @param index 挿入するインデックス
    /// @return 挿入に成功した場合は true、失敗した場合は false を返す
    bool InsertObject2D(std::unique_ptr<Object2DBase> obj, size_t index);
    /// @brief 3D オブジェクトを指定したインデックスに挿入
    /// @param obj 3D オブジェクトのユニークポインタ
    /// @param index 挿入するインデックス
    /// @return 挿入に成功した場合は true、失敗した場合は false を返す
    bool InsertObject3D(std::unique_ptr<Object3DBase> obj, size_t index);

    /// @brief 2D オブジェクトを削除
    /// @param obj 削除する 2D オブジェクトのポインタ
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool RemoveObject2D(Object2DBase *obj);
    /// @brief 3D オブジェクトを削除
    /// @param obj 削除する 3D オブジェクトのポインタ
    /// @return 削除に成功した場合は true、失敗した場合は false を返す
    bool RemoveObject3D(Object3DBase *obj);

    /// @brief 2D オブジェクトを解放（所有権の放棄をし、シーンから削除。インスタンスの解放は行わない）
    /// @param obj 解放する 2D オブジェクトのポインタ
    /// @return 解放に成功した場合は true、失敗した場合は false を返す
    bool ReleaseObject2D(Object2DBase *obj);
    /// @brief 3D オブジェクトを解放（所有権の放棄をし、シーンから削除。インスタンスの解放は行わない）
    /// @param obj 解放する 3D オブジェクトのポインタ
    /// @return 解放に成功した場合は true、失敗した場合は false を返す
    bool ReleaseObject3D(Object3DBase *obj);

    /// @brief 2D オブジェクトを移動
    /// @param obj 移動する 2D オブジェクトのポインタ
    /// @param newIndex 移動先のインデックス
    /// @return 移動に成功した場合は true、失敗した場合は false を返す
    bool MoveObject2D(Object2DBase *obj, size_t newIndex);
    /// @brief 3D オブジェクトを移動
    /// @param obj 移動する 3D オブジェクトのポインタ
    /// @param newIndex 移動先のインデックス
    /// @return 移動に成功した場合は true、失敗した場合は false を返す
    bool MoveObject3D(Object3DBase *obj, size_t newIndex);

    const std::vector<std::unique_ptr<Object2DBase>> &GetObjects2D() const { return objects2D_; }
    const std::vector<std::unique_ptr<Object3DBase>> &GetObjects3D() const { return objects3D_; }

    /// @brief 名前から一致する 2D オブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタのリスト（存在しない場合は空のリスト）
    std::vector<Object2DBase *> GetObjects2D(const std::string &objectName) const {
        std::vector<Object2DBase *> objects;
        auto range = objects2DIndexByName_.equal_range(objectName);
        for (auto it = range.first; it != range.second; ++it) {
            const size_t idx = it->second;
            if (idx < objects2D_.size() && objects2D_[idx]) {
                objects.push_back(objects2D_[idx].get());
            }
        }
        return objects;
    }

    /// @brief 名前から一致する 3D オブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタのリスト（存在しない場合は空のリスト）
    std::vector<Object3DBase *> GetObjects3D(const std::string &objectName) const {
        std::vector<Object3DBase *> objects;
        auto range = objects3DIndexByName_.equal_range(objectName);
        for (auto it = range.first; it != range.second; ++it) {
            const size_t idx = it->second;
            if (idx < objects3D_.size() && objects3D_[idx]) {
                objects.push_back(objects3D_[idx].get());
            }
        }
        return objects;
    }

    /// @brief 名前から一致する最初の 2D オブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタ（存在しない場合は nullptr）
    Object2DBase *GetObject2D(const std::string &objectName) const {
        auto range = objects2DIndexByName_.equal_range(objectName);
        for (auto it = range.first; it != range.second; ++it) {
            const size_t idx = it->second;
            if (idx < objects2D_.size() && objects2D_[idx]) {
                return objects2D_[idx].get();
            }
        }
        return nullptr;
    }

    /// @brief 名前から一致する最初の 3D オブジェクトを取得
    /// @param objectName オブジェクト名
    /// @return 一致するオブジェクトのポインタ（存在しない場合は nullptr）
    Object3DBase *GetObject3D(const std::string &objectName) const {
        auto range = objects3DIndexByName_.equal_range(objectName);
        for (auto it = range.first; it != range.second; ++it) {
            const size_t idx = it->second;
            if (idx < objects3D_.size() && objects3D_[idx]) {
                return objects3D_[idx].get();
            }
        }
        return nullptr;
    }

    /// @brief ポインタから一致する 2D オブジェクトを取得
    /// @param obj オブジェクトのポインタ
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    Object2DBase *GetObject2D(Object2DBase *obj) const {
        if (!obj) return nullptr;
        auto it = objects2DIndexByPointer_.find(obj);
        if (it == objects2DIndexByPointer_.end()) return nullptr;
        return objects2D_[it->second].get();
    }

    /// @brief ポインタから一致する 3D オブジェクトを取得
    /// @param obj オブジェクトのポインタ
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    Object3DBase *GetObject3D(Object3DBase *obj) const {
        if (!obj) return nullptr;
        auto it = objects3DIndexByPointer_.find(obj);
        if (it == objects3DIndexByPointer_.end()) return nullptr;
        return objects3D_[it->second].get();
    }

    /// @brief UUIDから一致する 2D オブジェクトを取得
    /// @param uuid オブジェクトのUUID
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    Object2DBase *GetObject2D(const UUID128 &uuid) const {
        if (!uuid.IsValid()) return nullptr;
        auto it = objects2DIndexByUUID_.find(uuid);
        if (it == objects2DIndexByUUID_.end()) return nullptr;
        return objects2D_[it->second].get();
    }

    /// @brief UUIDから一致する 3D オブジェクトを取得
    /// @param uuid オブジェクトのUUID
    /// @return オブジェクトのポインタ（存在しない場合は nullptr）
    Object3DBase *GetObject3D(const UUID128 &uuid) const {
        if (!uuid.IsValid()) return nullptr;
        auto it = objects3DIndexByUUID_.find(uuid);
        if (it == objects3DIndexByUUID_.end()) return nullptr;
        return objects3D_[it->second].get();
    }

    void ClearObjects2D();
    void ClearObjects3D();

    void SetNextSceneName(const std::string &nextSceneName) { nextSceneName_ = nextSceneName; }
    void ChangeToNextScene();
    void ClearNextSceneName() { nextSceneName_.clear(); }

    bool AddSceneComponent(std::unique_ptr<ISceneComponent> comp);
    bool RemoveSceneComponent(ISceneComponent *comp);
    void ClearSceneComponents();

    std::vector<ISceneComponent *> GetSceneComponents(const std::string &componentName) const {
        std::vector<ISceneComponent *> components;
        auto range = sceneComponentsIndexByName_.equal_range(componentName);
        for (auto it = range.first; it != range.second; ++it) {
            components.push_back(sceneComponents_[it->second].get());
        }
        return components;
    }

    ISceneComponent *GetSceneComponent(const std::string &componentName) const {
        auto range = sceneComponentsIndexByName_.equal_range(componentName);
        if (range.first != range.second) {
            return sceneComponents_[range.first->second].get();
        }
        return nullptr;
    }

    template<typename T>
    std::vector<T *> GetSceneComponents() const {
        std::vector<T *> components;
        auto range = sceneComponentsIndexByType_.equal_range(std::type_index(typeid(T)));
        for (auto it = range.first; it != range.second; ++it) {
            components.push_back(static_cast<T *>(sceneComponents_[it->second].get()));
        }
        return components;
    }

    template<typename T>
    T *GetSceneComponent() const {
        auto range = sceneComponentsIndexByType_.equal_range(std::type_index(typeid(T)));
        if (range.first != range.second) {
            return static_cast<T *>(sceneComponents_[range.first->second].get());
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<ISceneComponent>> &GetSceneComponents() const { return sceneComponents_; }

    size_t HasSceneComponents(const std::string &componentName) const {
        return sceneComponentsIndexByName_.count(componentName);
    }

    void AddSceneVariable(const std::string &key, const std::any &value);
    const MyStd::AnyUnorderedMap &GetSceneVariables() const;

    SceneContext *GetSceneContext() const { return sceneContext_.get(); }
#ifdef USE_IMGUI
    SceneEditorContext *GetSceneEditorContext() const { return sceneEditorContext_.get(); }
#endif

    static AudioManager *GetAudioManager() { return sAudioManager; }
    static ModelManager *GetModelManager() { return sModelManager; }
    static SkeletonManager *GetSkeletonManager() { return sSkeletonManager; }
    static SamplerManager *GetSamplerManager() { return sSamplerManager; }
    static TextureManager *GetTextureManager() { return sTextureManager; }
    static AnimationManager *GetAnimationManager() { return sAnimationManager; }
    static Input *GetInput() { return sInput; }
    static InputCommand *GetInputCommand() { return sInputCommand; }

private:
    static inline AudioManager *sAudioManager = nullptr;
    static inline ModelManager *sModelManager = nullptr;
    static inline SkeletonManager *sSkeletonManager = nullptr;
    static inline SamplerManager *sSamplerManager = nullptr;
    static inline TextureManager *sTextureManager = nullptr;
    static inline AnimationManager *sAnimationManager = nullptr;
    static inline Input *sInput = nullptr;
    static inline InputCommand *sInputCommand = nullptr;

    void RebuildObject2DIndices();
    void RebuildObject3DIndices();

    std::string name_;

    std::vector<std::unique_ptr<Object2DBase>> objects2D_;
    std::vector<std::unique_ptr<Object3DBase>> objects3D_;
    std::unordered_map<UUID128, size_t> objects2DIndexByUUID_;
    std::unordered_map<UUID128, size_t> objects3DIndexByUUID_;
    std::unordered_map<Object2DBase *, size_t> objects2DIndexByPointer_;
    std::unordered_map<Object3DBase *, size_t> objects3DIndexByPointer_;
    std::unordered_multimap<std::string, size_t> objects2DIndexByName_;
    std::unordered_multimap<std::string, size_t> objects3DIndexByName_;

    std::vector<std::unique_ptr<ISceneComponent>> sceneComponents_;
    std::unordered_multimap<std::string, size_t> sceneComponentsIndexByName_;
    std::unordered_multimap<std::type_index, size_t> sceneComponentsIndexByType_;

    std::unique_ptr<SceneContext> sceneContext_;
#ifdef USE_IMGUI
    std::unique_ptr<SceneEditorContext> sceneEditorContext_;
    std::unique_ptr<SceneEditor> sceneEditor_;
#endif

    std::string nextSceneName_;
    SceneManager *sceneManager_ = nullptr;
};

} // namespace KashipanEngine
