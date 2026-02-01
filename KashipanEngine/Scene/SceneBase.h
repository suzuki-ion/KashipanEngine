#pragma once

#include <memory>
#include <string>
#include <any>

#include "AnyUnorderedMap.h"
#include "Assets/AudioManager.h"
#include "Assets/ModelManager.h"
#include "Assets/SamplerManager.h"
#include "Assets/TextureManager.h"
#include "Input/Input.h"
#include "Input/InputCommand.h"
#include "Utilities/Passkeys.h"
#include "Utilities/EntityComponentSystem/WorldECS.h"

namespace KashipanEngine {

class SceneManager;
class GameEngine;

class SceneBase {
public:
    SceneBase() = delete;
    virtual ~SceneBase() = default;

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
    virtual void ShowImGui() {}
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

    std::string name_;
    std::string nextSceneName_;

    WorldECS world_{};
    SceneManager *sceneManager_ = nullptr;
};

} // namespace KashipanEngine
