#include "Objects/ObjectContext.h"
#include "Objects/Object2DBase.h"
#include "Objects/IObjectComponent.h"

namespace KashipanEngine {

//--------- Object2DContext ---------//

const std::string &Object2DContext::GetName() const {
    return owner_->GetName();
}

std::vector<IObjectComponent2D *> Object2DContext::GetComponents(const std::string &componentName) const {
    std::vector<IObjectComponent2D *> result;
    auto range = owner_->componentsIndexByName_.equal_range(componentName);
    for (auto it = range.first; it != range.second; ++it) {
        size_t idx = it->second;
        if (idx < owner_->components_.size()) {
            if (auto *p = dynamic_cast<IObjectComponent2D *>(owner_->components_[idx].get())) result.push_back(p);
        }
    }
    return result;
}

size_t Object2DContext::HasComponents(const std::string &componentName) const {
    auto range = owner_->componentsIndexByName_.equal_range(componentName);
    size_t count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        size_t idx = it->second;
        if (idx < owner_->components_.size() && dynamic_cast<IObjectComponent2D *>(owner_->components_[idx].get())) ++count;
    }
    return count;
}

//--------- Object3DContext ---------//

const std::string &Object3DContext::GetName() const {
    return owner_->GetName();
}

std::vector<IObjectComponent3D *> Object3DContext::GetComponents(const std::string &componentName) const {
    std::vector<IObjectComponent3D *> result;
    auto range = owner_->componentsIndexByName_.equal_range(componentName);
    for (auto it = range.first; it != range.second; ++it) {
        size_t idx = it->second;
        if (idx < owner_->components_.size()) {
            if (auto *p = dynamic_cast<IObjectComponent3D *>(owner_->components_[idx].get())) result.push_back(p);
        }
    }
    return result;
}

size_t Object3DContext::HasComponents(const std::string &componentName) const {
    auto range = owner_->componentsIndexByName_.equal_range(componentName);
    size_t count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        size_t idx = it->second;
        if (idx < owner_->components_.size() && dynamic_cast<IObjectComponent3D *>(owner_->components_[idx].get())) ++count;
    }
    return count;
}

} // namespace KashipanEngine
