#include "Scene/SceneBase.h"

#include <algorithm>

namespace KashipanEngine {

void SceneBase::SetEnginePointers(
    Passkey<GameEngine>,
    AudioManager *audioManager,
    ModelManager *modelManager,
    SamplerManager *samplerManager,
    TextureManager *textureManager,
    Input *input,
    InputCommand *inputCommand) {
    sAudioManager = audioManager;
    sModelManager = modelManager;
    sSamplerManager = samplerManager;
    sTextureManager = textureManager;
    sInput = input;
    sInputCommand = inputCommand;
}

SceneBase::SceneBase(const std::string &sceneName)
    : name_(sceneName) {
}

SceneBase::~SceneBase() {
    ClearSceneComponents();
    ClearObjects2D();
    ClearObjects3D();
}

void SceneBase::Update() {
    for (auto &o : objects2D_) {
        if (o) o->Update();
    }
    for (auto &o : objects3D_) {
        if (o) o->Update();
    }

    for (auto &c : sceneComponents_) {
        if (c) c->Update();
    }

    OnUpdate();
}

bool SceneBase::AddObject2D(std::unique_ptr<Object2DBase> obj) {
    if (!obj) return false;
    objects2D_.push_back(std::move(obj));
    return true;
}

bool SceneBase::AddObject3D(std::unique_ptr<Object3DBase> obj) {
    if (!obj) return false;
    objects3D_.push_back(std::move(obj));
    return true;
}

bool SceneBase::RemoveObject2D(Object2DBase *obj) {
    if (!obj) return false;
    auto it = std::find_if(objects2D_.begin(), objects2D_.end(), [&](const auto &p) { return p.get() == obj; });
    if (it == objects2D_.end()) return false;
    objects2D_.erase(it);
    return true;
}

bool SceneBase::RemoveObject3D(Object3DBase *obj) {
    if (!obj) return false;
    auto it = std::find_if(objects3D_.begin(), objects3D_.end(), [&](const auto &p) { return p.get() == obj; });
    if (it == objects3D_.end()) return false;
    objects3D_.erase(it);
    return true;
}

void SceneBase::ClearObjects2D() {
    objects2D_.clear();
}

void SceneBase::ClearObjects3D() {
    objects3D_.clear();
}

bool SceneBase::AddSceneComponent(std::unique_ptr<ISceneComponent> comp) {
    if (!comp) return false;
    comp->SetOwnerScene(this);
    comp->Initialize();
    sceneComponents_.push_back(std::move(comp));
    return true;
}

bool SceneBase::RemoveSceneComponent(ISceneComponent *comp) {
    if (!comp) return false;
    auto it = std::find_if(sceneComponents_.begin(), sceneComponents_.end(), [&](const auto &p) { return p.get() == comp; });
    if (it == sceneComponents_.end()) return false;

    if (*it) {
        (*it)->Finalize();
        (*it)->SetOwnerScene(nullptr);
    }

    sceneComponents_.erase(it);
    return true;
}

void SceneBase::ClearSceneComponents() {
    for (auto &c : sceneComponents_) {
        if (!c) continue;
        c->Finalize();
        c->SetOwnerScene(nullptr);
    }
    sceneComponents_.clear();
}

} // namespace KashipanEngine
