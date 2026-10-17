#pragma once
#ifdef USE_IMGUI
#include <memory>
#include <string>
#include <vector>
#include "Utilities/ShortTextBehavior.h"
#include "Utilities/ShortTextPropertyRequest.h"

namespace KashipanEngine {
class EmptyObject;
class IEditorCommand;
class CompositeCommand;

// Edit-time composition plan shown before applying. Building a plan or command never mutates the scene.
struct ShortTextComponentPlan {
    bool success = false;
    std::string message;
    std::vector<std::string> steps;        // 実際に追加・設定・更新・削除する内容
    std::vector<std::string> requirements; // 自動では作らない、利用者が設定すべき内容
};

// Registered components and members of the engine, preferring the target's existing instances.
ShortText::PropertySchema MakeShortTextPropertySchema(const EmptyObject *target);
ShortText::PropertyParseResult ParseShortTextPropertyRequest(const std::string &text, const EmptyObject *target);

// Set: edits the real component at edit time. PingPong: adds or updates a PropertyMotion.
// Tween: adds (or updates) a KeyFrameAnimator entry holding an embedded KeyframeAnimation bound to the member.
// Short-text drivers of the same member/channel (PropertyMotion, keyframe entry, legacy behavior) are replaced.
// Missing target components are added with defaults; everything is one undoable CompositeCommand.
// Tween options for event-driven clips: no autoplay, a chosen entry name, and the final entry name
// (an updated short-text entry keeps its existing name).
struct ShortTextTweenOptions {
    bool playOnStart = true;
    std::string entryName;
    std::string *appliedName = nullptr;
};
std::unique_ptr<IEditorCommand> MakeShortTextPropertyCommand(EmptyObject *target, const ShortText::PropertyRequest &request,
    const std::string &sourceText, ShortTextComponentPlan *plan = nullptr, const ShortTextTweenOptions *tween = nullptr);

// Components a legacy recipe needs on the selected object. Only asset-free components are added;
// colliders of unknown shape, audio, text, physics bodies and other targets are reported instead.
struct ShortTextRecipeNeeds {
    std::vector<std::string> add;
    std::vector<std::string> notes;
};
ShortTextRecipeNeeds AnalyzeShortTextRecipeNeeds(const EmptyObject *target, const ShortText::Recipe &recipe);
// Short-text PropertyMotions driving the same Transform member/axis as a simple legacy recipe.
// They are replaced (undoably) so one property is never driven twice by short text.
std::vector<class PropertyMotion *> FindConflictingShortTextMotions(const EmptyObject *target, const ShortText::Recipe &recipe);
ShortTextComponentPlan PlanShortTextBehavior(const EmptyObject *target, const ShortText::Recipe &recipe);
// Short-text KeyFrameAnimator entries driving the same Transform member/axis as a simple legacy recipe.
// With a group, undoable removals are appended; the plan always receives the steps.
void AppendShortTextKeyframeConflicts(CompositeCommand *group, const EmptyObject *target, const ShortText::Recipe &recipe, ShortTextComponentPlan &plan);
} // namespace KashipanEngine
#endif
