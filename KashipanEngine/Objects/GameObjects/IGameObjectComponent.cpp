#include "IGameObjectComponent.h"
#include "GameObjectContext.h"

namespace KashipanEngine {

GameObject2DContext *IGameObjectComponent2D::GetOwner2DContext() const {
    return static_cast<GameObject2DContext *>(GetOwnerContext());
}

GameObject3DContext *IGameObjectComponent3D::GetOwner3DContext() const {
    return static_cast<GameObject3DContext *>(GetOwnerContext());
}

} // namespace KashipanEngine
