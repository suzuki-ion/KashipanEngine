#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <memory>
#include <vector>
#include <string>
#include "Objects/Object2DBase.h"
#include "Objects/Object3DBase.h"
#include "Scene/Components/ISceneComponent.h"

namespace KashipanEngine {

class SceneBase;

class SceneEditorContext final {
public:
    SceneEditorContext(Passkey<SceneBase>, SceneBase *owner) : owner_(owner) {}
    ~SceneEditorContext() = default;

    SceneEditorContext(const SceneEditorContext &) = delete;
    SceneEditorContext &operator=(const SceneEditorContext &) = delete;
    SceneEditorContext(SceneEditorContext &&) = delete;
    SceneEditorContext &operator=(SceneEditorContext &&) = delete;

    SceneBase *GetOwner() const { return owner_; }

    const std::string &GetSceneName() const;

    bool AddObject2D(std::unique_ptr<Object2DBase> obj);
    bool AddObject3D(std::unique_ptr<Object3DBase> obj);

    bool RemoveObject2D(Object2DBase *obj);
    bool RemoveObject3D(Object3DBase *obj);

    const std::vector<std::unique_ptr<Object2DBase>> &GetObjects2D() const;
    const std::vector<std::unique_ptr<Object3DBase>> &GetObjects3D() const;

    bool AddSceneComponent(std::unique_ptr<ISceneComponent> comp);
    bool RemoveSceneComponent(ISceneComponent *comp);
    const std::vector<std::unique_ptr<ISceneComponent>> &GetSceneComponents() const;



private:
    SceneBase *owner_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI