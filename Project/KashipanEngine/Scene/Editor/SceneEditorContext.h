#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <memory>
#include <vector>
#include <string>
#include "Objects/Object2DBase.h"
#include "Objects/EmptyObject.h"
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
    bool AddObject(std::unique_ptr<EmptyObject> obj);

    bool InsertObject2D(std::unique_ptr<Object2DBase> obj, size_t index);
    bool InsertObject(std::unique_ptr<EmptyObject> obj, size_t index);

    bool RemoveObject2D(Object2DBase *obj);
    bool RemoveObject(EmptyObject *obj);

    bool ReleaseObject2D(Object2DBase *obj);
    bool ReleaseObject(EmptyObject *obj);

    bool MoveObject2D(Object2DBase *obj, size_t newIndex);
    bool MoveObject(EmptyObject *obj, size_t newIndex);

    const std::vector<std::unique_ptr<Object2DBase>> &GetObjects2D() const;
    const std::vector<std::unique_ptr<EmptyObject>> &GetObjects3D() const;

    bool AddSceneComponent(std::unique_ptr<ISceneComponent> comp);
    bool RemoveSceneComponent(ISceneComponent *comp);
    const std::vector<std::unique_ptr<ISceneComponent>> &GetSceneComponents() const;



private:
    SceneBase *owner_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI