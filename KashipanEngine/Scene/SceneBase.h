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
#include "Assets/MeshAssets.h"
#include "Input/Input.h"
#include "Input/InputCommand.h"
#include "AnyUnorderedMap.h"

#include "Scene/Components/ISceneComponent.h"
// #include "Scene/SceneContext.h"
#include "Utilities/EntityComponentSystem/WorldECS.h"
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
    WorldECS &GetWorld() { return world_; }
    const WorldECS &GetWorld() const { return world_; }

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
        MeshAssets *meshAssets,
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
        for (const auto &comp : sceneComponents_) {
            if (dynamic_cast<T *>(comp.get())) {
                components.push_back(dynamic_cast<T *>(comp.get()));
            }
        }
        return components;
    }

    template<typename T>
    T *GetSceneComponent() const {
        for (const auto &comp : sceneComponents_) {
            if (dynamic_cast<T *>(comp.get())) {
                return dynamic_cast<T *>(comp.get());
            }
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
    static MeshAssets *GetMeshAssets() { return sMeshAssets; }
    static Input *GetInput() { return sInput; }
    static InputCommand *GetInputCommand() { return sInputCommand; }

private:
    static inline AudioManager *sAudioManager = nullptr;
    static inline ModelManager *sModelManager = nullptr;
    static inline SamplerManager *sSamplerManager = nullptr;
    static inline TextureManager *sTextureManager = nullptr;
    static inline MeshAssets *sMeshAssets = nullptr;
    static inline Input *sInput = nullptr;
    static inline InputCommand *sInputCommand = nullptr;

    std::string name_;

    WorldECS world_{};

    std::vector<std::unique_ptr<ISceneComponent>> sceneComponents_;
    std::unordered_multimap<std::string, size_t> sceneComponentsIndexByName_;
    std::unordered_multimap<std::type_index, size_t> sceneComponentsIndexByType_;
    std::unique_ptr<SceneContext> sceneContext_;

    std::string nextSceneName_;
    SceneManager *sceneManager_ = nullptr;
};

} // namespace KashipanEngine
