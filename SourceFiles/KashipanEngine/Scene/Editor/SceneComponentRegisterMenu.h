#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditor;

class SceneComponentRegisterMenu final {
public:
    SceneComponentRegisterMenu(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneComponentRegisterMenu() = default;

    void ShowImGui();

private:
    SceneEditorContext *context_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI