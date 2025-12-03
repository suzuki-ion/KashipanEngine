#include "Objects/GameObjects/GameObjectContext.h"
#include "Objects/GameObjects/GameObject2DBase.h"
#include "Objects/GameObjects/IGameObjectComponent.h"

namespace KashipanEngine {

//--------- GameObject2DContext ---------//

const std::string &GameObject2DContext::GetName() const {
    return owner_->GetName();
}

std::vector<IGameObjectComponent2D *> GameObject2DContext::GetComponents(const std::string &componentName) const {
    std::vector<IGameObjectComponent2D *> result;
    auto range = owner_->componentsIndexByName_.equal_range(componentName);
    for (auto it = range.first; it != range.second; ++it) {
        size_t idx = it->second;
        if (idx < owner_->components_.size()) {
            if (auto *p = dynamic_cast<IGameObjectComponent2D *>(owner_->components_[idx].get())) result.push_back(p);
        }
    }
    return result;
}

size_t GameObject2DContext::HasComponents(const std::string &componentName) const {
    auto range = owner_->componentsIndexByName_.equal_range(componentName);
    size_t count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        size_t idx = it->second;
        if (idx < owner_->components_.size() && dynamic_cast<IGameObjectComponent2D *>(owner_->components_[idx].get())) ++count;
    }
    return count;
}

//--------- GameObject3DContext ---------//

const std::string &GameObject3DContext::GetName() const {
    return owner_->GetName();
}

std::vector<IGameObjectComponent3D *> GameObject3DContext::GetComponents(const std::string &componentName) const {
    std::vector<IGameObjectComponent3D *> result;
    auto range = owner_->componentsIndexByName_.equal_range(componentName);
    for (auto it = range.first; it != range.second; ++it) {
        size_t idx = it->second;
        if (idx < owner_->components_.size()) {
            if (auto *p = dynamic_cast<IGameObjectComponent3D *>(owner_->components_[idx].get())) result.push_back(p);
        }
    }
    return result;
}

size_t GameObject3DContext::HasComponents(const std::string &componentName) const {
    auto range = owner_->componentsIndexByName_.equal_range(componentName);
    size_t count = 0;
    for (auto it = range.first; it != range.second; ++it) {
        size_t idx = it->second;
        if (idx < owner_->components_.size() && dynamic_cast<IGameObjectComponent3D *>(owner_->components_[idx].get())) ++count;
    }
    return count;
}

} // namespace KashipanEngine
