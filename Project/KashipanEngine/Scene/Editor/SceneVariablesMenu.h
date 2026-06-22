#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/Editor/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

class SceneVariablesMenu final {
public:
    SceneVariablesMenu(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneVariablesMenu() = default;

    void ShowImGui();

private:
    SceneEditorContext *context_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI