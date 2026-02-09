#include "Scene/SceneContext.h"

namespace KashipanEngine {

const std::string &SceneContext::GetName() const {
    static const std::string kEmpty;
    if (!owner_) return kEmpty;
    return owner_->GetName();
}

std::vector<ISceneComponent *> SceneContext::GetComponents(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return {};
    return owner_->GetSceneComponents(componentName);
}

ISceneComponent *SceneContext::GetComponent(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return nullptr;
    return owner_->GetSceneComponent(componentName);
}

size_t SceneContext::HasComponents(const std::string &componentName) const {
    if (componentName.empty() || !owner_) return 0;
    return owner_->HasSceneComponents(componentName);
}

} // namespace KashipanEngine
