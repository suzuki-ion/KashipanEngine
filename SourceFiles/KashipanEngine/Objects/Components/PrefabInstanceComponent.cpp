#include "PrefabInstanceComponent.h"
#ifdef USE_IMGUI
#include "Scene/Editor/PrefabAssetManager.h"

namespace KashipanEngine {

std::string PrefabInstanceComponent::GetPrefabPath() const {
    return PrefabAssetManager::GetPrefabPath(prefabID_);
}

} // namespace KashipanEngine

#endif // USE_IMGUI
