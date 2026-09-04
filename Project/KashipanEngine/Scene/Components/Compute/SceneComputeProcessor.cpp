#include "SceneComputeProcessor.h"

#include <algorithm>

#include "Debug/Logger.h"
#include "Objects/Components/Compute/ComputeShaderProcessing.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void SceneComputeProcessor::RegisterComputeShaderProcessing(ComputeShaderProcessing *component) {
    LogScope scope;
    if (!component) return;
    if (std::find(components_.begin(), components_.end(), component) != components_.end()) return;
    components_.push_back(component);
}

void SceneComputeProcessor::UnregisterComputeShaderProcessing(const ComputeShaderProcessing *component) {
    LogScope scope;
    auto it = std::find(components_.begin(), components_.end(), component);
    if (it != components_.end()) components_.erase(it);
}

#if defined(USE_IMGUI)
void SceneComputeProcessor::ShowImGui() {
    LogScope scope;
    ImGui::Text("%s%d", TranslationC("editor.scenecomputeprocessor.count"), static_cast<int>(components_.size()));
}
#endif

} // namespace KashipanEngine
