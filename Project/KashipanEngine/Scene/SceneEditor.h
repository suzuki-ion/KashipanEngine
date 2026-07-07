#pragma once
#ifdef USE_IMGUI
#include <memory>

#include "Scene/SceneEditorContext.h"

namespace KashipanEngine {

class SceneEditorCommands;
class SceneObjectHierarchy;
class SceneObjectInspector;
class SceneComponentInspector;
class SceneVariablesMenu;
class SceneEditorView;
class AssetsWindow;
class SceneSaver;
class SceneLoader;

class SceneEditor final {
public:
    SceneEditor(Passkey<Scene>, SceneEditorContext *context);
    ~SceneEditor();

    void ShowImGui();

private:
    void ShowMainWindow();
    void HandleShortcuts();
    void PerformUndo();
    void PerformRedo();

    SceneEditorContext *context_ = nullptr;

    std::unique_ptr<SceneEditorCommands> commands_;
    std::unique_ptr<SceneObjectHierarchy> objectHierarchy_;
    std::unique_ptr<SceneObjectInspector> objectInspector_;
    std::unique_ptr<SceneComponentInspector> componentInspector_;
    std::unique_ptr<SceneVariablesMenu> variablesMenu_;
    std::unique_ptr<SceneEditorView> sceneView_;
    std::unique_ptr<AssetsWindow> assetsWindow_;
    std::unique_ptr<SceneSaver> saver_;
    std::unique_ptr<SceneLoader> loader_;

    bool isShowSceneView_ = true;
    bool isShowHierarchy_ = true;
    bool isShowObjectInspector_ = true;
    bool isShowComponentInspector_ = true;
    bool isShowVariablesMenu_ = true;
    bool isShowAssets_ = true;
};

} // namespace KashipanEngine
#endif // USE_IMGUI
