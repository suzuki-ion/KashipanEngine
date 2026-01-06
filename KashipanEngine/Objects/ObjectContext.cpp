#include "Objects/ObjectContext.h"
#include "Objects/Object2DBase.h"
#include "Objects/Object3DBase.h"
#include "Objects/IObjectComponent.h"

namespace KashipanEngine {

//--------- Object2DContext ---------//

const std::string &Object2DContext::GetName() const {
    return owner_->GetName();
}

std::vector<IObjectComponent2D *> Object2DContext::GetComponents(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return {};
    return owner_->GetComponents2D(componentName);
}

IObjectComponent2D *Object2DContext::GetComponent(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return nullptr;
    return owner_->GetComponent2D(componentName);
}

size_t Object2DContext::HasComponents(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return 0;
    return owner_->HasComponents2D(componentName);
}

//--------- Object3DContext ---------//

const std::string &Object3DContext::GetName() const {
    return owner_->GetName();
}

std::vector<IObjectComponent3D *> Object3DContext::GetComponents(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return {};
    return owner_->GetComponents3D(componentName);
}

IObjectComponent3D *Object3DContext::GetComponent(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return nullptr;
    return owner_->GetComponent3D(componentName);
}

size_t Object3DContext::HasComponents(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return 0;
    return owner_->HasComponents3D(componentName);
}

} // namespace KashipanEngine
