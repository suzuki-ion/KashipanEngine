#pragma once
#include <vector>

#include "Scene/Components/SceneComponentHeader.h"

namespace KashipanEngine {

class ComputeShaderProcessing;

/// @brief シーン内のComputeShaderProcessingコンポーネントを収集するシーンコンポーネント
/// @details ComputeShaderProcessing::Initialize/FinalizeからRegister/Unregisterされる。
///          実際の処理（Dispatch）はRendererが毎フレーム、このコンポーネントが
///          保持するリストを参照して行う。
class SceneComputeProcessor final : public ISceneComponent {
public:
    SCENE_COMPONENT_CONSTRUCTOR(SceneComputeProcessor, 1, SetUpdatePriority(50);)
    COMPONENT_CATEGORY("Compute")
    ~SceneComputeProcessor() override = default;

    std::unique_ptr<ISceneComponent> Clone() const override {
        return std::make_unique<SceneComputeProcessor>();
    }

    void RegisterComputeShaderProcessing(ComputeShaderProcessing *component);
    void UnregisterComputeShaderProcessing(const ComputeShaderProcessing *component);

    const std::vector<ComputeShaderProcessing *> &GetComputeShaderProcessings() const noexcept { return components_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    std::vector<ComputeShaderProcessing *> components_;
};

REGISTER_COMPONENT_SCENE(SceneComputeProcessor)

} // namespace KashipanEngine
