#include "Scene/SceneContext.h"

namespace KashipanEngine {

const std::string &SceneContext::GetName() const {
    static const std::string kEmpty;
    if (!owner_) return kEmpty;
    return owner_->GetName();
}

void SceneContext::AddSceneVariable(const std::string &key, const std::any &value) {
    if (key.empty() || !owner_) return;
    owner_->AddSceneVariable(key, value);
}

const MyStd::AnyUnorderedMap &SceneContext::GetSceneVariables() const {
    static const MyStd::AnyUnorderedMap kEmpty;
    if (!owner_) return kEmpty;
    return owner_->GetSceneVariables();
}

} // namespace KashipanEngine
