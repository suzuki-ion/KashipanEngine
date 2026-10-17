#pragma once
#ifdef USE_IMGUI
#include <functional>
#include <memory>
#include <string>
#include "Utilities/UUID128.h"
namespace KashipanEngine {
class SceneEditorContext;
class IEditorCommand;
class SceneEditorCommands;
struct EditorApiServices {
    SceneEditorCommands *commands = nullptr;
    std::function<std::string(const std::string &, const std::string &)> editor;
};
std::string EditorApiCatalog();
std::string ExtendedEditorApiCatalog();
// Synchronous, editor-thread only. Execute must route through the editor's Undo history.
std::string ExecuteEditorApi(SceneEditorContext *context, const UUID128 &selected,
    const std::function<bool(std::unique_ptr<IEditorCommand>)> &execute,
    const std::string &tool, const std::string &arguments, EditorApiServices *services = nullptr);
std::string ExecuteExtendedEditorApi(SceneEditorContext *context, const UUID128 &selected,
    const std::function<bool(std::unique_ptr<IEditorCommand>)> &execute,
    const std::string &tool, const std::string &arguments, EditorApiServices *services);
// Shared canonical path validation for project-scoped file APIs; never accepts absolute/escaping paths.
std::string EditorApiProjectPath(const std::string &relative, bool assetsOnly = true);
}
#endif
