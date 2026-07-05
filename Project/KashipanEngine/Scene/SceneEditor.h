#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/Editor/SceneComponentHierarchy.h"
#include "Scene/Editor/SceneComponentInspector.h"
#include "Scene/Editor/SceneComponentRegisterMenu.h"
#include "Scene/Editor/SceneObjectHierarchy.h"
#include "Scene/Editor/SceneObjectInspector.h"
#include "Scene/Editor/SceneObjectRegisterMenu.h"
#include "Scene/Editor/SceneVariablesMenu.h"
#include "Scene/Editor/SceneSaver.h"
#include "Scene/Editor/SceneLoder.h"
#include "Scene/Editor/SceneEditorCommands.h"

namespace KashipanEngine {

class SceneEditor final {
public:
    SceneEditor(Passkey<Scene>, SceneEditorContext *context) {
        context_ = context;
        objectHierarchy_ = std::make_unique<SceneObjectHierarchy>(Passkey<SceneEditor>{}, context_);
        objectInspector_ = std::make_unique<SceneObjectInspector>(Passkey<SceneEditor>{}, context_, objectHierarchy_.get());
    }
    ~SceneEditor() = default;

    void ShowImGui() {
        if (objectHierarchy_) objectHierarchy_->ShowImGui();
        if (objectInspector_) objectInspector_->ShowImGui();
    }

private:
    SceneEditorContext *context_;
    std::unique_ptr<SceneObjectHierarchy> objectHierarchy_;
    std::unique_ptr<SceneObjectInspector> objectInspector_;
};

} // namespace KashipanEngine
#endif // USE_IMGUI