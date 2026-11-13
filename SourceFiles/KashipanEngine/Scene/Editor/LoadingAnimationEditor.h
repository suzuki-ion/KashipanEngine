#pragma once
#if defined(USE_IMGUI)
namespace KashipanEngine {
class SceneEditorContext;
class SceneEditorCommands;
namespace LoadingAnimationEditor {
void Menu();
void Close();
void Show(SceneEditorContext *context, SceneEditorCommands *commands);
}
}
#endif
