#pragma once
#include "Scene/SceneBase.h"

namespace KashipanEngine {

class SceneEditorContext final {
public:
    SceneEditorContext(Passkey<SceneBase>, SceneBase *owner) : owner_(owner) {}
    ~SceneEditorContext() = default;

    SceneEditorContext(const SceneEditorContext &) = delete;
    SceneEditorContext &operator=(const SceneEditorContext &) = delete;
    SceneEditorContext(SceneEditorContext &&) = delete;
    SceneEditorContext &operator=(SceneEditorContext &&) = delete;

    SceneBase *GetOwner() const { return owner_; }

    bool AddObject2D(std::unique_ptr<Object2DBase> obj);
    bool AddObject3D(std::unique_ptr<Object3DBase> obj);

    bool RemoveObject2D(Object2DBase *obj);
    bool RemoveObject3D(Object3DBase *obj);

    const std::vector<std::unique_ptr<Object2DBase>> &GetObjects2D() const { return owner_->GetObjects2D(); }
    const std::vector<std::unique_ptr<Object3DBase>> &GetObjects3D() const { return owner_->GetObjects3D(); }

private:
    SceneBase *owner_ = nullptr;
};

} // namespace KashipanEngine