#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

class SceneLoader final {
public:
    SceneLoader(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneLoader() = default;

    void ShowImGui();

private:
    SceneEditorContext *context_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI