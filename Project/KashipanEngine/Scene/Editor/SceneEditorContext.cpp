#include "SceneEditorContext.h"
#include "Scene/SceneBase.h"

namespace KashipanEngine {

const std::string &SceneEditorContext::GetSceneName() const { return owner_->GetName(); }
bool SceneEditorContext::AddObject2D(std::unique_ptr<Object2DBase> obj) { return owner_->AddObject2D(std::move(obj)); }
bool SceneEditorContext::AddObject3D(std::unique_ptr<Object3DBase> obj) { return owner_->AddObject3D(std::move(obj)); }
bool SceneEditorContext::RemoveObject2D(Object2DBase *obj) { return owner_->RemoveObject2D(obj); }
bool SceneEditorContext::RemoveObject3D(Object3DBase *obj) { return owner_->RemoveObject3D(obj); }
const std::vector<std::unique_ptr<Object2DBase>> &SceneEditorContext::GetObjects2D() const { return owner_->GetObjects2D(); }
const std::vector<std::unique_ptr<Object3DBase>> &SceneEditorContext::GetObjects3D() const { return owner_->GetObjects3D(); }
bool SceneEditorContext::AddSceneComponent(std::unique_ptr<ISceneComponent> comp) { return owner_->AddSceneComponent(std::move(comp)); }
bool SceneEditorContext::RemoveSceneComponent(ISceneComponent *comp) { return owner_->RemoveSceneComponent(comp); }
const std::vector<std::unique_ptr<ISceneComponent>> &SceneEditorContext::GetSceneComponents() const { return owner_->GetSceneComponents(); }

} // namespace KashipanEngine