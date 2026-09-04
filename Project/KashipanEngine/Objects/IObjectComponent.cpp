#include "IObjectComponent.h"
#include "Debug/Logger.h"
#include "Objects/EmptyObject.h"
#include "Objects/ObjectContext.h"

namespace KashipanEngine {

const EmptyObject *IObjectComponent::GetOwnerObject() const {
    return objectContext_ ? objectContext_->GetOwner() : nullptr;
}

ComponentRef IObjectComponent::GetComponentRef() const {
    LogScope scope;
    const EmptyObject *owner = GetOwnerObject();
    if (!owner) return ComponentRef{};
    return ComponentRef{ owner->GetObjectID(), owner->GetComponentAddedID(this) };
}

bool IObjectComponent::IsActive() const {
    LogScope scope;
    bool ownerActive = objectContext_ ? objectContext_->GetOwner()->IsActive() : true;
    return isActive_ && ownerActive;
}

void IObjectComponent::SetActive(bool active) {
    LogScope scope;
    if (isActive_ == active) return;
    isActive_ = active;
    if (objectContext_) {
        if (IsActive()) {
            Initialize();
        } else {
            Finalize();
        }
    }
}

} // namespace KashipanEngine
