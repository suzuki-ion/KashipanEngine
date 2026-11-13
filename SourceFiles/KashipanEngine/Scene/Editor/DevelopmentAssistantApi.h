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
// エディタースレッド専用の同期処理。ExecuteはエディターのUndo履歴を経由する必要がある。
std::string ExecuteEditorApi(SceneEditorContext *context, const UUID128 &selected,
    const std::function<bool(std::unique_ptr<IEditorCommand>)> &execute,
    const std::string &tool, const std::string &arguments, EditorApiServices *services = nullptr);
std::string ExecuteExtendedEditorApi(SceneEditorContext *context, const UUID128 &selected,
    const std::function<bool(std::unique_ptr<IEditorCommand>)> &execute,
    const std::string &tool, const std::string &arguments, EditorApiServices *services);
// プロジェクト内ファイルAPIで共有する正規化パスの検証。絶対パスや範囲外に出るパスは受け付けない。
std::string EditorApiProjectPath(const std::string &relative, bool assetsOnly = true);
}
#endif
