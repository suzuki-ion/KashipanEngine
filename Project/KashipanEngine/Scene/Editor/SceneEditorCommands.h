#pragma once
#ifdef USE_IMGUI
#include <imgui.h>

namespace KashipanEngine {

class SceneEditor;

class SceneEditorCommands final {
public:
    SceneEditorCommands(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneEditorCommands() = default;

    void ShowImGui();

private:
    SceneEditorContext *context_ = nullptr;
};

} // namespace KashipanEngine

#endif // USE_IMGUI