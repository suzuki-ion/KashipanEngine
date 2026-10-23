#include "IObjectComponent.h"
#include "Objects/EmptyObject.h"
#include "Objects/ObjectContext.h"

namespace KashipanEngine {

const EmptyObject *IObjectComponent::GetOwnerObject() const {
    return objectContext_ ? objectContext_->GetOwner() : nullptr;
}

bool IObjectComponent::IsRegisteredToOwner() const {
    const EmptyObject *owner = GetOwnerObject();
    return owner && owner->HasComponent(this) > 0;
}

ComponentRef IObjectComponent::GetComponentRef() const {
    const EmptyObject *owner = GetOwnerObject();
    if (!owner) return ComponentRef{};
    return ComponentRef{ owner->GetObjectID(), owner->GetComponentAddedID(this) };
}

bool IObjectComponent::IsActive() const {
    return isActive_ && (!objectContext_ || objectContext_->GetOwner()->IsActive());
}

void IObjectComponent::SetUpdatePriority(int priority) {
    if (updatePriority_ == priority) return;
    updatePriority_ = priority;
    if (objectContext_) objectContext_->GetOwner()->InvalidateUpdateList(Passkey<IObjectComponent>{});
}

void IObjectComponent::NotifyUpdateListChanged() {
    if (objectContext_) objectContext_->GetOwner()->InvalidateUpdateList(Passkey<IObjectComponent>{});
    EmptyObject::InvalidateActiveStateCache(Passkey<IObjectComponent>{});
}

void IObjectComponent::SetActive(bool active) {
    if (isActive_ == active) return;
    isActive_ = active;
    EmptyObject::InvalidateActiveStateCache(Passkey<IObjectComponent>{});
    if (objectContext_) {
        objectContext_->GetOwner()->InvalidateUpdateList(Passkey<IObjectComponent>{});
        if (IsActive()) {
            Initialize();
        } else {
            Finalize();
        }
    }
}

} // namespace KashipanEngine
