#include "Scene/Scene.h"

#include <algorithm>

namespace KashipanEngine {

Scene::Scene(const std::string &sceneName)
    : name_(sceneName) {
}

Scene::~Scene() {
    ClearSceneComponents();
    ClearObjects2D();
    ClearObjects3D();
}

void Scene::Update() {
    for (auto &c : sceneComponents_) {
        if (c) c->Update();
    }

    for (auto &o : objects2D_) {
        if (o) o->Update();
    }
    for (auto &o : objects3D_) {
        if (o) o->Update();
    }

    OnUpdate();
}

bool Scene::AddObject2D(std::unique_ptr<Object2DBase> obj) {
    if (!obj) return false;
    objects2D_.push_back(std::move(obj));
    return true;
}

bool Scene::AddObject3D(std::unique_ptr<Object3DBase> obj) {
    if (!obj) return false;
    objects3D_.push_back(std::move(obj));
    return true;
}

bool Scene::RemoveObject2D(Object2DBase *obj) {
    if (!obj) return false;
    auto it = std::find_if(objects2D_.begin(), objects2D_.end(), [&](const auto &p) { return p.get() == obj; });
    if (it == objects2D_.end()) return false;
    objects2D_.erase(it);
    return true;
}

bool Scene::RemoveObject3D(Object3DBase *obj) {
    if (!obj) return false;
    auto it = std::find_if(objects3D_.begin(), objects3D_.end(), [&](const auto &p) { return p.get() == obj; });
    if (it == objects3D_.end()) return false;
    objects3D_.erase(it);
    return true;
}

void Scene::ClearObjects2D() {
    objects2D_.clear();
}

void Scene::ClearObjects3D() {
    objects3D_.clear();
}

bool Scene::AddSceneComponent(std::unique_ptr<ISceneComponent> comp) {
    if (!comp) return false;
    comp->SetOwnerScene(this);
    comp->Initialize();
    sceneComponents_.push_back(std::move(comp));
    return true;
}

bool Scene::RemoveSceneComponent(ISceneComponent *comp) {
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

void Scene::ClearSceneComponents() {
    for (auto &c : sceneComponents_) {
        if (!c) continue;
        c->Finalize();
        c->SetOwnerScene(nullptr);
    }
    sceneComponents_.clear();
}

} // namespace KashipanEngine
