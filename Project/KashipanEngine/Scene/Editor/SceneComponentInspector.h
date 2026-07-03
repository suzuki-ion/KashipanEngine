#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

class SceneComponentInspector final {
public:
    SceneComponentInspector(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneComponentInspector() = default;

    void ShowImGui();

private:
    SceneEditorContext *context_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI