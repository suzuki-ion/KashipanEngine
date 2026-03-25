#pragma once

#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Assets/AudioManager.h"
#include "Assets/ModelManager.h"
#include "Assets/SamplerManager.h"
#include "Assets/TextureManager.h"
#include "Input/Input.h"
#include "Input/InputCommand.h"

#include "Objects/Object2DBase.h"
#include "Objects/Object3DBase.h"
#include "Objects/Collision/Collider.h"
#include "Scene/Components/ISceneComponent.h"
// #include "Scene/SceneContext.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class SceneContext;

class SceneManager;
class GameEngine;

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
        SamplerManager *samplerManager,
        TextureManager *textureManager,
        Input *input,
        InputCommand *inputCommand);

    /// @brief シーン初期化（SceneManager から呼ばれる）
    virtual void Initialize() {}

    /// @brief シーン終了処理（SceneManager から呼ばれる）
    virtual void Finalize() {}

    friend class SceneContext;

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

    bool AddObject2D(std::unique_ptr<Object2DBase> obj);
    bool AddObject3D(std::unique_ptr<Object3DBase> obj);

    bool RemoveObject2D(Object2DBase *obj);
    bool RemoveObject3D(Object3DBase *obj);

    const std::vector<std::unique_ptr<Object2DBase>> &GetObjects2D() const { return objects2D_; }
    const std::vector<std::unique_ptr<Object3DBase>> &GetObjects3D() const { return objects3D_; }

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

    Object2DBase *GetObject2D(Object2DBase *obj) const {
        if (!obj) return nullptr;
        auto range = objects2DIndexByPointer_.equal_range(obj);
        for (auto it = range.first; it != range.second; ++it) {
            const size_t idx = it->second;
            if (idx < objects2D_.size() && objects2D_[idx] && objects2D_[idx].get() == obj) {
                return objects2D_[idx].get();
            }
        }
        return nullptr;
    }

    Object3DBase *GetObject3D(Object3DBase *obj) const {
        if (!obj) return nullptr;
        auto range = objects3DIndexByPointer_.equal_range(obj);
        for (auto it = range.first; it != range.second; ++it) {
            const size_t idx = it->second;
            if (idx < objects3D_.size() && objects3D_[idx] && objects3D_[idx].get() == obj) {
                return objects3D_[idx].get();
            }
        }
        return nullptr;
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

    size_t HasSceneComponents(const std::string &componentName) const {
        return sceneComponentsIndexByName_.count(componentName);
    }

    void AddSceneVariable(const std::string &key, const std::any &value);
    const MyStd::AnyUnorderedMap &GetSceneVariables() const;

    static AudioManager *GetAudioManager() { return sAudioManager; }
    static ModelManager *GetModelManager() { return sModelManager; }
    static SamplerManager *GetSamplerManager() { return sSamplerManager; }
    static TextureManager *GetTextureManager() { return sTextureManager; }
    static Input *GetInput() { return sInput; }
    static InputCommand *GetInputCommand() { return sInputCommand; }

private:
    static inline AudioManager *sAudioManager = nullptr;
    static inline ModelManager *sModelManager = nullptr;
    static inline SamplerManager *sSamplerManager = nullptr;
    static inline TextureManager *sTextureManager = nullptr;
    static inline Input *sInput = nullptr;
    static inline InputCommand *sInputCommand = nullptr;

    void RebuildObject2DIndices();
    void RebuildObject3DIndices();

    std::string name_;

    std::vector<std::unique_ptr<Object2DBase>> objects2D_;
    std::vector<std::unique_ptr<Object3DBase>> objects3D_;
    std::unordered_multimap<Object2DBase *, size_t> objects2DIndexByPointer_;
    std::unordered_multimap<Object3DBase *, size_t> objects3DIndexByPointer_;
    std::unordered_multimap<std::string, size_t> objects2DIndexByName_;
    std::unordered_multimap<std::string, size_t> objects3DIndexByName_;

    std::vector<std::unique_ptr<ISceneComponent>> sceneComponents_;
    std::unordered_multimap<std::string, size_t> sceneComponentsIndexByName_;
    std::unordered_multimap<std::type_index, size_t> sceneComponentsIndexByType_;
    std::unique_ptr<SceneContext> sceneContext_;

    std::string nextSceneName_;
    SceneManager *sceneManager_ = nullptr;
};

} // namespace KashipanEngine
