#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

class SceneComponentHierarchy final {
public:
    SceneComponentHierarchy(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneComponentHierarchy() = default;

    void ShowImGui();

private:
    SceneEditorContext *context_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI