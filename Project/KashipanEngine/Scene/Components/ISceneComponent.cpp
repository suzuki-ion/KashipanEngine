#include "Scene/Components/ISceneComponent.h"

#include "Scene/SceneContext.h"

namespace KashipanEngine {

SceneBase *ISceneComponent::GetOwnerScene() const {
    auto *ctx = GetOwnerContext();
    return ctx ? ctx->GetOwner() : nullptr;
}

} // namespace KashipanEngine
