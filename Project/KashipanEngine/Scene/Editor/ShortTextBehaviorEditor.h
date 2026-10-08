#pragma once
#ifdef USE_IMGUI
#include <memory>
#include "Utilities/ShortTextBehavior.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {
class EmptyObject;
class SceneEditorContext;
class SceneEditorCommands;
class IEditorCommand;

std::unique_ptr<IEditorCommand> MakeShortTextBehaviorCommand(EmptyObject *target,
    const ShortText::Recipe &recipe, const std::string &sourceText);

// An isolated experiment. No global selection or scene pointers are retained across frames.
class ShortTextBehaviorEditor {
public:
    bool open = false;
    void Show(SceneEditorContext *context, SceneEditorCommands *commands, EmptyObject *selected);
private:
    char input_[768] = "上下にふわふわ動かして";
    ShortText::Result parsed_;
    UUID128 targetID_;
    UUID128 sceneID_;
    std::string status_;
};
} // namespace KashipanEngine
#endif
