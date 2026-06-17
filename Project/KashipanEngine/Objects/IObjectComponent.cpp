#include "Objects/IObjectComponent.h"
#include "Objects/ObjectContext.h"
#include "ComponentSerialize/ComponentRegistry.h"

namespace KashipanEngine {

Object2DContext *IObjectComponent2D::GetOwner2DContext() const {
    return static_cast<Object2DContext *>(GetOwnerContext());
}

SceneContext *IObjectComponent2D::GetOwnerSceneContext() const {
    return GetOwner2DContext()->GetOwnerSceneContext();
}

Object3DContext *IObjectComponent3D::GetOwner3DContext() const {
    return static_cast<Object3DContext *>(GetOwnerContext());
}

SceneContext *IObjectComponent3D::GetOwnerSceneContext() const {
    return GetOwner3DContext()->GetOwnerSceneContext();
}

} // namespace KashipanEngine
