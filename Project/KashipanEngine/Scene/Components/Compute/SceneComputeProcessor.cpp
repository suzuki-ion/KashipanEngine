#include "SceneComputeProcessor.h"

#include <algorithm>

#include "Objects/Components/Compute/ComputeShaderProcessing.h"

namespace KashipanEngine {

void SceneComputeProcessor::RegisterComputeShaderProcessing(ComputeShaderProcessing *component) {
    if (!component) return;
    if (std::find(components_.begin(), components_.end(), component) != components_.end()) return;
    components_.push_back(component);
}

void SceneComputeProcessor::UnregisterComputeShaderProcessing(const ComputeShaderProcessing *component) {
    auto it = std::find(components_.begin(), components_.end(), component);
    if (it != components_.end()) components_.erase(it);
}

#if defined(USE_IMGUI)
void SceneComputeProcessor::ShowImGui() {
    ImGui::Text("ComputeShaderProcessing: %d", static_cast<int>(components_.size()));
}
#endif

} // namespace KashipanEngine
