#pragma once

#include <memory>
#include <string>
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
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

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

    void SetSceneManager(Passkey<SceneManager>, SceneManager *sceneManager) { sceneManager_ = sceneManager; }

    static void SetEnginePointers(
        Passkey<GameEngine>,
        AudioManager *audioManager,
        ModelManager *modelManager,
        SamplerManager *samplerManager,
        TextureManager *textureManager,
        Input *input,
        InputCommand *inputCommand);

protected:
    SceneBase(const std::string &sceneName);

    virtual void OnUpdate() {}

    bool AddObject2D(std::unique_ptr<Object2DBase> obj);
    bool AddObject3D(std::unique_ptr<Object3DBase> obj);

    bool RemoveObject2D(Object2DBase *obj);
    bool RemoveObject3D(Object3DBase *obj);

    const std::vector<std::unique_ptr<Object2DBase>> &GetObjects2D() const { return objects2D_; }
    const std::vector<std::unique_ptr<Object3DBase>> &GetObjects3D() const { return objects3D_; }

    void ClearObjects2D();
    void ClearObjects3D();

    void SetNextSceneName(const std::string &nextSceneName) { nextSceneName_ = nextSceneName; }
    void ClearNextSceneName() { nextSceneName_.clear(); }

    bool AddSceneComponent(std::unique_ptr<ISceneComponent> comp);
    bool RemoveSceneComponent(ISceneComponent *comp);
    void ClearSceneComponents();

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

    std::vector<std::unique_ptr<Object2DBase>> objects2D_;
    std::vector<std::unique_ptr<Object3DBase>> objects3D_;

    std::vector<std::unique_ptr<ISceneComponent>> sceneComponents_;

    std::string nextSceneName_;
    SceneManager *sceneManager_ = nullptr;
};

} // namespace KashipanEngine
