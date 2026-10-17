#pragma once
#ifdef USE_IMGUI
#include <memory>
#include <optional>
#include "Utilities/ShortTextBehavior.h"
#include "Utilities/ShortTextLearning.h"
#include "Scene/Editor/ShortTextComponentPlan.h"
#include "Utilities/ShortTextEventRequest.h"
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
    bool SetInput(const std::string &sentence);
private:
    void ShowLearning(EmptyObject *selected);
    char input_[768] = "上下にふわふわ動かして";
    ShortText::Result parsed_;
    // 「〇〇の〇〇」 requests become real components instead of a ShortTextBehavior recipe.
    ShortText::PropertyParseResult property_;
    // User-defined events and gameplay components (「GoalReachedイベントを作成」「体力を100にして…」).
    ShortText::EventParseResult event_;
    ShortTextComponentPlan plan_;
    UUID128 targetID_;
    UUID128 sceneID_;
    std::string status_;
    std::optional<ShortText::LearningStore> learning_;
    std::optional<ShortText::LearningStore> defaults_;
    std::string defaultsStatus_;
    std::string learningProjectRoot_;
    std::string learningStatus_;
    char teachPhrase_[768] = "";
    char teachReplacement_[768] = "ふわふわ";
    ShortText::Recipe teachRecipe_;
    int teachMode_ = 0;
    int learningSelection_ = -1;
};
} // namespace KashipanEngine
#endif
