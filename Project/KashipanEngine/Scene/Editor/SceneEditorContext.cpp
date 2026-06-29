#include "SceneEditorContext.h"
#include "Scene/SceneBase.h"

namespace KashipanEngine {

const std::string &SceneEditorContext::GetSceneName() const { return owner_->GetName(); }
bool SceneEditorContext::AddObject2D(std::unique_ptr<Object2DBase> obj) { return owner_->AddObject2D(std::move(obj)); }
bool SceneEditorContext::AddObject(std::unique_ptr<EmptyObject> obj) { return owner_->AddObject(std::move(obj)); }
bool SceneEditorContext::InsertObject2D(std::unique_ptr<Object2DBase> obj, size_t index) { return owner_->InsertObject2D(std::move(obj), index); }
bool SceneEditorContext::InsertObject(std::unique_ptr<EmptyObject> obj, size_t index) { return owner_->InsertObject(std::move(obj), index); }
bool SceneEditorContext::RemoveObject2D(Object2DBase *obj) { return owner_->RemoveObject2D(obj); }
bool SceneEditorContext::RemoveObject(EmptyObject *obj) { return owner_->RemoveObject(obj); }
bool SceneEditorContext::ReleaseObject2D(Object2DBase *obj) { return owner_->ReleaseObject2D(obj); }
bool SceneEditorContext::ReleaseObject(EmptyObject *obj) { return owner_->ReleaseObject(obj); }
bool SceneEditorContext::MoveObject2D(Object2DBase *obj, size_t newIndex) { return owner_->MoveObject2D(obj, newIndex); }
bool SceneEditorContext::MoveObject(EmptyObject *obj, size_t newIndex) { return owner_->MoveObject(obj, newIndex); }
const std::vector<std::unique_ptr<Object2DBase>> &SceneEditorContext::GetObjects2D() const { return owner_->GetObjects2D(); }
const std::vector<std::unique_ptr<EmptyObject>> &SceneEditorContext::GetObjects3D() const { return owner_->GetObjects3D(); }
bool SceneEditorContext::AddSceneComponent(std::unique_ptr<ISceneComponent> comp) { return owner_->AddSceneComponent(std::move(comp)); }
bool SceneEditorContext::RemoveSceneComponent(ISceneComponent *comp) { return owner_->RemoveSceneComponent(comp); }
const std::vector<std::unique_ptr<ISceneComponent>> &SceneEditorContext::GetSceneComponents() const { return owner_->GetSceneComponents(); }

} // namespace KashipanEngine