#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include "Utilities/ImGuiCustom.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneObjectHierarchy.h"

namespace KashipanEngine {

class SceneEditor;

class SceneObjectInspector final {
public:
    SceneObjectInspector(Passkey<SceneEditor>, SceneEditorContext *context, SceneObjectHierarchy *objectHierarchy)
        : context_(context), objectHierarchy_(objectHierarchy) {}
    ~SceneObjectInspector() = default;

    void ShowImGui();

private:
    void ShowObjectInspector(EmptyObject *obj);

    SceneEditorContext *context_ = nullptr;
    SceneObjectHierarchy *objectHierarchy_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
