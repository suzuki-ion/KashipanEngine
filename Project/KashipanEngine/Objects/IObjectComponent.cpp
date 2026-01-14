#include "Objects/IObjectComponent.h"
#include "Objects/ObjectContext.h"

namespace KashipanEngine {

Object2DContext *IObjectComponent2D::GetOwner2DContext() const {
    return static_cast<Object2DContext *>(GetOwnerContext());
}

Object3DContext *IObjectComponent3D::GetOwner3DContext() const {
    return static_cast<Object3DContext *>(GetOwnerContext());
}

} // namespace KashipanEngine
