// Native regression: short text → a real KeyframeAnimation registered in a real KeyFrameAnimator.
// Uses real Scene / SceneEditorContext / commands / components. Headless: no window, GPU or audio device.
// GetDeltaTime() is 0 without the game loop, so time is driven with SetCurrentTime and real Update frames.
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/ShortTextBehaviorEditor.h"
#include "Scene/Editor/ShortTextComponentPlan.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/PropertyMotion.h"
#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Collider/BoxCollider.h"
#include "Utilities/KeyframeAnimation.h"

using namespace KashipanEngine;
namespace {
void Require(bool value, const std::string &message) { if (!value) throw std::runtime_error(message); }
bool Near(float a, float b, float e = 1e-4f) { return std::abs(a - b) <= e; }
const std::string kRequired = "1秒かけてTransformの移動Y軸を0.0fから4.0fまでEaseInOutCubicで移動";
const std::string kMoveName = "ShortText.Transform.translate_.y";
const auto kUpdateKey = std::bit_cast<Passkey<EmptyObject>>(std::uint8_t{});

KeyFrameAnimator *Animator(const EmptyObject *object, size_t index = 0) {
    auto animators = object ? object->GetComponents<KeyFrameAnimator>() : std::vector<KeyFrameAnimator *>{};
    return index < animators.size() ? animators[index] : nullptr;
}
size_t AnimatorCount(const EmptyObject *object) { return object ? object->GetComponents<KeyFrameAnimator>().size() : 0; }
size_t MotionCount(const EmptyObject *object) { return object ? object->GetComponents<PropertyMotion>().size() : 0; }
Vector3 Translate(const EmptyObject *object) { return object->GetComponent<Transform>()->GetTranslate(); }
void Frame(EmptyObject *object) { Animator(object)->UpdateInterface(kUpdateKey); }

struct Applied { std::unique_ptr<IEditorCommand> command; ShortTextComponentPlan plan; };
Applied Apply(SceneEditorContext &context, EmptyObject *target, const std::string &text) {
    const auto parsed = ParseShortTextPropertyRequest(text, target);
    Require(parsed.recognized && parsed.success, "parse: " + text + " / " + parsed.message);
    Applied applied;
    applied.command = MakeShortTextPropertyCommand(target, parsed.request, text, &applied.plan);
    Require(applied.command && applied.plan.success, "plan: " + text + " / " + applied.plan.message);
    Require(applied.command->Execute(&context), "execute: " + text);
    return applied;
}
void RejectWithoutMutation(Scene &scene, EmptyObject *target, const std::string &text) {
    const auto before = scene.SaveToJSON().dump();
    const auto parsed = ParseShortTextPropertyRequest(text, target);
    if (parsed.success) {
        ShortTextComponentPlan plan;
        Require(!MakeShortTextPropertyCommand(target, parsed.request, text, &plan) && !plan.success, "must reject: " + text);
    } else Require(parsed.recognized && !parsed.message.empty(), "rejected with message: " + text);
    Require(scene.SaveToJSON().dump() == before, "rejection must not mutate: " + text);
}
bool PlanMentions(const ShortTextComponentPlan &plan, const std::string &word) {
    for (const auto &step : plan.steps) if (step.find(word) != std::string::npos) return true;
    return false;
}
}

int main() {
    const auto original = std::filesystem::current_path();
    const auto sandbox = std::filesystem::temp_directory_path() /
        ("KashipanShortTextKeyframe-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(sandbox);
    std::filesystem::current_path(sandbox);
    int result = 0;
    try {
        // --- The grammar's ease table matches the engine's EaseType names exactly ---
        Require(ShortText::PropertyEaseNames().size() == static_cast<size_t>(EaseType::EaseOutInBounce) + 1, "ease table size");
        for (const auto &name : ShortText::PropertyEaseNames()) {
            EaseType type = EaseType::Linear;
            Require(TryStringToEaseType(name, type) && name == EaseTypeToString(type), "ease name known to engine: " + name);
        }
        {
            EaseType type = EaseType::EaseInQuad;
            Require(!TryStringToEaseType("easeinoutcubic", type) && type == EaseType::EaseInQuad, "engine lookup is exact and leaves out unchanged");
        }

        Scene scene(std::string("ShortTextKeyframeRegression"));
        // Test-only access token (Passkey is trivially copyable); GameEngine is not booted.
        SceneEditorContext context(std::bit_cast<Passkey<Scene>>(std::uint8_t{}), &scene);
        auto *sceneContext = scene.GetSceneContext();

        // --- Required sentence: plan is side-effect free, apply adds a KeyFrameAnimator with a real clip ---
        auto *subject = sceneContext->CreateEmptyObject("subject");
        const auto subjectID = subject->GetObjectID();
        subject->GetComponent<Transform>()->SetTranslate(Vector3(3, 9, -2));
        {
            const auto before = scene.SaveToJSON().dump();
            const auto parsed = ParseShortTextPropertyRequest(kRequired, subject);
            Require(parsed.success && parsed.request.mode == ShortText::PropertyMode::Tween, "required sentence parses as tween: " + parsed.message);
            ShortTextComponentPlan plan;
            auto command = MakeShortTextPropertyCommand(subject, parsed.request, kRequired, &plan);
            Require(command && plan.success && PlanMentions(plan, "KeyFrameAnimator を追加") && PlanMentions(plan, "EaseInOutCubic") &&
                PlanMentions(plan, "ループなし") && PlanMentions(plan, "保持"), "plan preview lists animator, ease, one-shot and hold");
            Require(dynamic_cast<CompositeCommand *>(command.get()) != nullptr, "one composite command");
            Require(AnimatorCount(subject) == 0 && scene.SaveToJSON().dump() == before, "planning is side-effect free");
        }
        auto first = Apply(context, subject, kRequired);
        Require(AnimatorCount(subject) == 1, "KeyFrameAnimator added");
        auto *animator = Animator(subject);
        Require(animator->GetAnimationCount() == 1 && animator->HasAnimation(kMoveName), "one generated animation");
        Require(animator->IsEmbedded(kMoveName) && animator->IsShortTextManaged(kMoveName), "embedded and managed");
        Require(!animator->GetLoop(kMoveName) && animator->GetPlayOnStart(kMoveName) && Near(animator->GetValueScale(kMoveName), 1) &&
            Near(animator->GetValueOffset(kMoveName), 0) && Near(animator->GetPlaybackSpeed(kMoveName), 1), "played once on start, raw units");
        {
            const auto *bindings = animator->GetBindings(kMoveName);
            Require(bindings && bindings->size() == 1, "one binding");
            const auto &b = bindings->front();
            Require(b.componentType == "Transform" && b.parameterName == "translate_" && b.channel == 1 && b.componentIndex == 0 && !b.isScriptVariable, "binding Transform.translate_.y");
        }
        // The real KeyframeAnimation: first key carries the ease, Evaluate follows EaseInOutCubic.
        const auto *clip = animator->GetAnimationData(kMoveName);
        Require(clip && clip->GetKeyframeCount() == 2 && Near(clip->GetDuration(), 1), "two keys, 1 second");
        const auto &keys = clip->GetKeyframes();
        Require(Near(keys[0].time, 0) && Near(keys[0].value, 0) && keys[0].easeType == EaseType::EaseInOutCubic, "first key 0s=0 EaseInOutCubic");
        Require(Near(keys[1].time, 1) && Near(keys[1].value, 4), "last key 1s=4");
        Require(Near(clip->Evaluate(0.0f), 0) && Near(clip->Evaluate(0.25f), 0.25f) && Near(clip->Evaluate(0.5f), 2) &&
            Near(clip->Evaluate(0.75f), 3.75f) && Near(clip->Evaluate(1.0f), 4) && Near(clip->Evaluate(2.0f), 4), "Evaluate 0/0.25/2/3.75/4");
        {
            const JSON entry = animator->SaveAnimationEntryToJson(kMoveName);
            Require(entry["embeddedAnimation"]["keyframes"][0]["ease"] == "EaseInOutCubic" && entry["jsonPath"] == "" &&
                entry["shortTextManaged"] == true && entry["sourceText"] == kRequired && entry["loop"] == false && entry["playOnStart"] == true, "saved entry JSON");
        }
        auto translate = Translate(subject);
        Require(Near(translate.x, 3) && Near(translate.y, 9) && Near(translate.z, -2), "edit-time value is not moved by applying");
        Frame(subject);
        Require(Near(Translate(subject).y, 9) && !animator->IsPlaying(kMoveName), "Update does nothing while editing");

        // --- Re-applying updates the generated entry instead of duplicating; other values replace keys ---
        auto again = Apply(context, subject, kRequired);
        Require(AnimatorCount(subject) == 1 && Animator(subject)->GetAnimationCount() == 1, "re-application does not duplicate");
        auto changed = Apply(context, subject, "Transformの移動Y軸を0から6まで2秒かけてEaseOutCubicで移動");
        animator = Animator(subject);
        Require(AnimatorCount(subject) == 1 && animator->GetAnimationCount() == 1 && Near(animator->GetDuration(kMoveName), 2) &&
            Near(animator->GetAnimationData(kMoveName)->GetKeyframes()[1].value, 6) &&
            animator->GetAnimationData(kMoveName)->GetKeyframes()[0].easeType == EaseType::EaseOutCubic, "new values replace the generated clip");
        Require(PlanMentions(changed.plan, "更新"), "plan says update");
        Require(changed.command->Undo(&context) && Near(Animator(subject)->GetAnimationData(kMoveName)->GetKeyframes()[1].value, 4), "undo restores previous clip");
        Require(changed.command->Execute(&context) && Near(Animator(subject)->GetAnimationData(kMoveName)->GetKeyframes()[1].value, 6), "redo");
        Require(changed.command->Undo(&context), "back to the required clip");

        // --- Undo / Redo of the first application on a fresh object removes/re-adds the animator ---
        auto *fresh = sceneContext->CreateEmptyObject("fresh");
        auto freshApplied = Apply(context, fresh, kRequired);
        const JSON freshSaved = fresh->SaveComponentToJson(Animator(fresh));
        Require(freshApplied.command->Undo(&context) && AnimatorCount(fresh) == 0, "undo removes the added animator");
        Require(freshApplied.command->Execute(&context) && AnimatorCount(fresh) == 1 &&
            fresh->SaveComponentToJson(Animator(fresh)) == freshSaved, "redo restores the same clip");

        // --- Rotation: keys in degrees, valueScale converts to radians ---
        auto *spinner = sceneContext->CreateEmptyObject("spinner");
        const auto spinnerID = spinner->GetObjectID();
        Apply(context, spinner, "2秒かけてTransformの回転Z軸を0.0fから90.0fまでEaseOutQuadで回転");
        const std::string spinName = "ShortText.Transform.rotate_.z";
        Require(Animator(spinner)->HasAnimation(spinName) && Near(Animator(spinner)->GetValueScale(spinName), std::numbers::pi_v<float> / 180.0f, 1e-7f), "degrees → radians scale");
        Require(Near(Animator(spinner)->GetAnimationData(spinName)->GetKeyframes()[1].value, 90) &&
            Animator(spinner)->GetAnimationData(spinName)->GetKeyframes()[0].easeType == EaseType::EaseOutQuad, "keys stored in degrees");
        Require(Animator(spinner)->GetBindings(spinName)->front().parameterName == "rotate_" && Animator(spinner)->GetBindings(spinName)->front().channel == 2, "rotate_.z binding");

        // --- Missing target component is added at edit time (BoxCollider) ---
        auto *box = sceneContext->CreateEmptyObject("box");
        const auto boxID = box->GetObjectID();
        auto boxApplied = Apply(context, box, "1秒かけてBoxColliderのサイズX軸を1.0fから3.0fまでEaseInOutCubicで変更");
        Require(box->GetComponent<BoxCollider>() && AnimatorCount(box) == 1 && PlanMentions(boxApplied.plan, "BoxCollider を追加"), "BoxCollider + animator added");
        Require(boxApplied.command->Undo(&context) && !box->GetComponent<BoxCollider>() && AnimatorCount(box) == 0, "one undo removes both");
        Require(boxApplied.command->Execute(&context) && box->GetComponent<BoxCollider>() && AnimatorCount(box) == 1, "redo adds both");

        // --- Scale with Linear ---
        auto *scaler = sceneContext->CreateEmptyObject("scaler");
        const auto scalerID = scaler->GetObjectID();
        Apply(context, scaler, "1秒かけてTransformのスケールX軸を1.0fから2.0fまでLinearで変更");

        // --- Existing hand-made clips are kept; the generated one is added next to them ---
        auto *clipOwner = sceneContext->CreateEmptyObject("clips");
        auto *handMade = clipOwner->AddComponent<KeyFrameAnimator>();
        Require(handMade && handMade->AddAnimation("Hand", "Assets/hand.json") && handMade->AddBinding("Hand", "Transform", "translate_", 0), "hand-made file clip");
        Require(handMade->AddAnimationData("ManualY", JSON{{"keyframes", JSON::array({JSON{{"time", 0}, {"value", 1}}, JSON{{"time", 2}, {"value", 5}}})}}) &&
            handMade->AddBinding("ManualY", "Transform", "translate_", 1), "hand-made embedded clip on Y");
        const JSON handBefore = handMade->SaveAnimationEntryToJson("Hand");
        const JSON manualBefore = handMade->SaveAnimationEntryToJson("ManualY");
        Require(!handBefore.contains("embeddedAnimation") && !handBefore.contains("shortTextManaged"), "file entry format unchanged");
        const JSON clipsBefore = clipOwner->SaveComponentToJson(handMade);
        auto clipsApplied = Apply(context, clipOwner, kRequired);
        handMade = Animator(clipOwner);
        Require(AnimatorCount(clipOwner) == 1 && handMade->GetAnimationCount() == 3 && handMade->HasAnimation(kMoveName), "generated clip added to the existing animator");
        Require(handMade->SaveAnimationEntryToJson("Hand") == handBefore && handMade->SaveAnimationEntryToJson("ManualY") == manualBefore, "hand-made clips untouched");
        Require(!clipsApplied.plan.requirements.empty(), "manual clip on the same member is reported");
        auto clipsAgain = Apply(context, clipOwner, kRequired);
        Require(Animator(clipOwner)->GetAnimationCount() == 3, "re-application keeps a single generated clip next to hand-made ones");
        Require(clipsAgain.command->Undo(&context) && clipsApplied.command->Undo(&context) &&
            clipOwner->SaveComponentToJson(Animator(clipOwner)) == clipsBefore, "undo restores the hand-made animator exactly");
        Require(clipsApplied.command->Execute(&context) && Animator(clipOwner)->GetAnimationCount() == 3, "redo");

        // --- Play: starts at 0 on the first frame, follows the clip, stops at the end and holds ---
        subject = sceneContext->GetSceneObject(subjectID);
        Require(subject && Near(Translate(subject).y, 9), "edit value before play");
        scene.PlayStart();
        Require(scene.IsPlaying(), "playing");
        subject = sceneContext->GetSceneObject(subjectID);
        animator = Animator(subject);
        Frame(subject);
        translate = Translate(subject);
        Require(animator->IsPlaying(kMoveName) && Near(translate.y, 0) && Near(translate.x, 3) && Near(translate.z, -2), "first played frame starts at 0 from Y=9, X/Z kept");
        Require(animator->SetCurrentTime(kMoveName, 0.25f) && Near(Translate(subject).y, 0.25f), "0.25s → 0.25");
        Require(animator->SetCurrentTime(kMoveName, 0.5f) && Near(Translate(subject).y, 2), "0.5s → 2");
        Frame(subject);
        Require(animator->IsPlaying(kMoveName) && Near(Translate(subject).y, 2), "mid-clip frame keeps playing");
        Require(animator->SetCurrentTime(kMoveName, 0.75f) && Near(Translate(subject).y, 3.75f), "0.75s → 3.75");
        Require(animator->SetCurrentTime(kMoveName, 1.0f) && Near(Translate(subject).y, 4), "1s → 4");
        Frame(subject);
        Require(!animator->IsPlaying(kMoveName) && Near(Translate(subject).y, 4) && Near(animator->GetAnimationTime(kMoveName), 1), "clip end stops once");
        for (int i = 0; i < 5; ++i) Frame(subject);
        Require(Near(Translate(subject).y, 4) && Near(Translate(subject).x, 3) && Near(Translate(subject).z, -2), "end value is held");
        // Stop keeps the position; Play restarts from 0.
        Require(animator->Play(kMoveName), "play again");
        Frame(subject);
        Require(Near(Translate(subject).y, 0), "restart begins at 0");
        Require(animator->SetCurrentTime(kMoveName, 0.5f) && animator->Stop(kMoveName), "stop mid-clip");
        Frame(subject);
        Require(!animator->IsPlaying(kMoveName) && Near(Translate(subject).y, 2) && Near(animator->GetAnimationTime(kMoveName), 0.5f), "stopped value and time kept");
        Require(animator->Play(kMoveName), "resume as replay");
        Frame(subject);
        Require(Near(Translate(subject).y, 0), "replay starts at 0 again");
        // Rotation, collider size and scale during play.
        auto *playSpinner = sceneContext->GetSceneObject(spinnerID);
        Frame(playSpinner);
        Require(Near(playSpinner->GetComponent<Transform>()->GetRotate().z, 0), "rotation starts at 0");
        Require(Animator(playSpinner)->SetCurrentTime(spinName, 2) && Near(playSpinner->GetComponent<Transform>()->GetRotate().z, std::numbers::pi_v<float> / 2), "90 degrees written as radians");
        auto *playBox = sceneContext->GetSceneObject(boxID);
        Frame(playBox);
        Require(Near(playBox->GetComponent<BoxCollider>()->GetSize().x, 1), "collider size starts at 1");
        Require(Animator(playBox)->SetCurrentTime("ShortText.BoxCollider.size_.x", 1) && Near(playBox->GetComponent<BoxCollider>()->GetSize().x, 3), "collider size reaches 3");
        auto *playScaler = sceneContext->GetSceneObject(scalerID);
        Frame(playScaler);
        Require(Animator(playScaler)->SetCurrentTime("ShortText.Transform.scale_.x", 0.5f) && Near(playScaler->GetComponent<Transform>()->GetScale().x, 1.5f), "linear scale midpoint");
        scene.PlayStop();
        subject = sceneContext->GetSceneObject(subjectID);
        Require(subject && Near(Translate(subject).y, 9) && Near(Translate(subject).x, 3), "stop restores edit-time value");
        Require(AnimatorCount(subject) == 1 && Animator(subject)->IsEmbedded(kMoveName), "generated clip survives play/stop");

        // --- Double driving: PropertyMotion, legacy behavior and keyframe tween replace each other undoably ---
        auto *mixed = sceneContext->CreateEmptyObject("mixed");
        Apply(context, mixed, "Transformの移動Y軸を-1から1の間で往復移動");
        Apply(context, mixed, "Transformの移動X軸を-1から1の間で往復移動");
        Require(MotionCount(mixed) == 2, "two ping-pong motions");
        auto toTween = Apply(context, mixed, kRequired);
        Require(MotionCount(mixed) == 1 && AnimatorCount(mixed) == 1, "same-member motion removed");
        Require(mixed->GetComponents<PropertyMotion>().front()->GetSettings().axis == 0, "other-axis motion kept");
        Require(toTween.command->Undo(&context) && MotionCount(mixed) == 2 && AnimatorCount(mixed) == 0, "undo restores the motion");
        Require(toTween.command->Execute(&context) && MotionCount(mixed) == 1 && AnimatorCount(mixed) == 1, "redo");
        auto toPingPong = Apply(context, mixed, "Transformの移動Y軸を-2から2の間で往復移動");
        Require(MotionCount(mixed) == 2 && Animator(mixed)->GetAnimationCount() == 0 && PlanMentions(toPingPong.plan, "キーフレームアニメーション"), "ping-pong removes the generated clip");
        Require(toPingPong.command->Undo(&context) && Animator(mixed)->GetAnimationCount() == 1 && MotionCount(mixed) == 1, "undo restores the clip");
        auto toSet = Apply(context, mixed, "Transformの移動Y軸を5にする");
        Require(Animator(mixed)->GetAnimationCount() == 0, "set removes the generated clip");
        Require(toSet.command->Undo(&context) && Animator(mixed)->GetAnimationCount() == 1, "undo set");

        auto *legacy = sceneContext->CreateEmptyObject("legacy");
        const auto bob = ShortText::Parse("上下にふわふわ動かして");
        Require(bob.success && bob.recipe.axis == 1, "legacy Y recipe");
        auto legacyCommand = MakeShortTextBehaviorCommand(legacy, bob.recipe, "上下にふわふわ動かして");
        Require(legacyCommand && legacyCommand->Execute(&context) && legacy->GetComponent<ShortTextBehavior>(), "legacy applied");
        auto replaceLegacy = Apply(context, legacy, kRequired);
        Require(!legacy->GetComponent<ShortTextBehavior>() && AnimatorCount(legacy) == 1, "legacy behavior on the same axis replaced");
        Require(replaceLegacy.command->Undo(&context) && legacy->GetComponent<ShortTextBehavior>() && AnimatorCount(legacy) == 0, "undo restores legacy");
        Require(replaceLegacy.command->Execute(&context), "redo replacement");
        Apply(context, legacy, "1秒かけてTransformの移動X軸を0から1までLinearで移動");
        Require(Animator(legacy)->GetAnimationCount() == 2, "two generated clips on different axes");
        const auto planBack = PlanShortTextBehavior(legacy, bob.recipe);
        Require(planBack.success && PlanMentions(planBack, "キーフレームアニメーション"), "legacy plan lists the clip removal");
        auto back = MakeShortTextBehaviorCommand(legacy, bob.recipe, "上下にふわふわ動かして");
        Require(back && back->Execute(&context) && legacy->GetComponent<ShortTextBehavior>(), "legacy re-applied");
        Require(Animator(legacy)->GetAnimationCount() == 1 && Animator(legacy)->HasAnimation("ShortText.Transform.translate_.x"), "only the same-axis clip removed");
        Require(back->Undo(&context) && Animator(legacy)->GetAnimationCount() == 2 && !legacy->GetComponent<ShortTextBehavior>(), "undo legacy replacement");

        // --- Invalid requests do not mutate ---
        for (const char *text : {"Transformの移動Y軸を0から4までEaseInOutCubicで移動", "0秒かけてTransformの移動Y軸を0から4までLinearで移動",
            "5000秒かけてTransformの移動Y軸を0から4までLinearで移動", "1秒かけてTransformの移動Y軸を0から4まで2秒かけてLinearで移動",
            "1秒かけてTransformの移動Y軸を0から4までSmoothStepで移動", "1秒かけてBoxColliderのサイズX軸を1から3までEaseOutBackで変更",
            "1秒かけてBoxColliderのサイズX軸を0から3までLinearで変更", "1秒かけてBox2DColliderのサイズZ軸を1から2までLinearで変更",
            "1秒かけてTransformのworldMatrix_ X軸を0から1までLinearで変更", "1秒かけてTransformの移動Y軸を0から20000までLinearで移動",
            "1秒かけてTransformの移動Y軸を-1から1の間で往復", "1秒かけてTransformの移動Y軸を4にする", "1秒かけてFooColliderのサイズX軸を1から2までLinearで変更"})
            RejectWithoutMutation(scene, subject, text);
        {
            const auto before = scene.SaveToJSON().dump();
            const auto request = ParseShortTextPropertyRequest(kRequired, subject).request;
            auto bad = request; bad.duration = 0;
            Require(!MakeShortTextPropertyCommand(subject, bad, kRequired), "zero duration rejected at plan level");
            bad = request; bad.duration = std::numeric_limits<float>::quiet_NaN();
            Require(!MakeShortTextPropertyCommand(subject, bad, kRequired), "NaN duration rejected");
            bad = request; bad.to = std::numeric_limits<float>::infinity();
            Require(!MakeShortTextPropertyCommand(subject, bad, kRequired), "non-finite value rejected");
            bad = request; bad.ease = "Bogus";
            Require(!MakeShortTextPropertyCommand(subject, bad, kRequired), "unknown ease rejected");
            bad = request; bad.component = "BoxCollider"; bad.member = "size_"; bad.axis = 0; bad.positiveOnly = true; bad.from = 1; bad.to = 2; bad.ease = "EaseInElastic";
            Require(!MakeShortTextPropertyCommand(subject, bad, kRequired), "overshoot on positive-only rejected");
            bad = request; bad.member = "worldMatrix_";
            Require(!MakeShortTextPropertyCommand(subject, bad, kRequired), "readonly rejected");
            Require(scene.SaveToJSON().dump() == before, "plan-level rejection does not mutate");
        }

        // --- Strict loading keeps the previous state ---
        animator = Animator(subject);
        const JSON saved = subject->SaveComponentToJson(animator);
        auto rejectLoad = [&](const std::string &what, auto mutate) {
            JSON invalid = saved;
            mutate(invalid["data"]["customData"]["animations"][0]);
            Require(!subject->LoadComponentFromJson(animator, invalid), "rejected: " + what);
            Require(subject->SaveComponentToJson(animator) == saved, "previous state kept: " + what);
        };
        rejectLoad("unknown ease", [](JSON &e) { e["embeddedAnimation"]["keyframes"][0]["ease"] = "Bogus"; });
        rejectLoad("string time", [](JSON &e) { e["embeddedAnimation"]["keyframes"][0]["time"] = "0"; });
        rejectLoad("negative time", [](JSON &e) { e["embeddedAnimation"]["keyframes"][1]["time"] = -1; });
        rejectLoad("missing value", [](JSON &e) { e["embeddedAnimation"]["keyframes"][1].erase("value"); });
        rejectLoad("keyframes not array", [](JSON &e) { e["embeddedAnimation"]["keyframes"] = 3; });
        rejectLoad("non-object key", [](JSON &e) { e["embeddedAnimation"]["keyframes"][0] = 1; });
        rejectLoad("huge value", [](JSON &e) { e["embeddedAnimation"]["keyframes"][1]["value"] = 1e30; });
        rejectLoad("bad channel", [](JSON &e) { e["bindings"][0]["channel"] = 9; });
        rejectLoad("speed type", [](JSON &e) { e["playbackSpeed"] = "1"; });
        rejectLoad("loop type", [](JSON &e) { e["loop"] = 1; });
        rejectLoad("managed type", [](JSON &e) { e["shortTextManaged"] = "yes"; });
        rejectLoad("source too long", [](JSON &e) { e["sourceText"] = std::string(600, 'a'); });
        {
            JSON invalid = saved; invalid["data"]["customData"]["animations"] = JSON::object();
            Require(!subject->LoadComponentFromJson(animator, invalid) && subject->SaveComponentToJson(animator) == saved, "animations not array");
        }
        {
            KeyframeAnimation keysOnly;
            Require(keysOnly.LoadFromJson(JSON{{"keyframes", JSON::array({JSON{{"time", 1}, {"value", 2}, {"ease", "EaseInQuad"}}, JSON{{"time", 0}, {"value", 1}}})}}), "valid keys load");
            Require(keysOnly.GetKeyframeCount() == 2 && Near(keysOnly.GetKeyframes()[0].time, 0) && keysOnly.GetKeyframes()[0].easeType == EaseType::Linear, "sorted, missing ease is Linear");
            Require(!keysOnly.LoadFromJson(JSON{{"keyframes", JSON::array({JSON{{"time", 0}, {"value", 0}, {"ease", "Nope"}}})}}) && keysOnly.GetKeyframeCount() == 2, "invalid keys keep previous");
            Require(!keysOnly.LoadFromJson(JSON::array()) && keysOnly.GetKeyframeCount() == 2, "non-object keeps previous");
            KeyFrameAnimator api;
            Require(!api.AddAnimationData("x", JSON{{"keyframes", 1}}) && api.GetAnimationCount() == 0, "invalid data is not added");
            Require(api.AddAnimationData("x", keysOnly.SaveToJson()) && !api.AddAnimationData("x", keysOnly.SaveToJson()), "duplicate name rejected");
            Require(!api.SetAnimationData("x", JSON{{"keyframes", JSON::array({1})}}) && api.GetAnimationData("x")->GetKeyframeCount() == 2, "SetAnimationData keeps previous");
            Require(!api.SetShortTextSource("x", true, std::string(600, 'a')) && !api.IsShortTextManaged("x"), "source size limit");
        }

        // --- File-based entries keep working with the strict loader (backward compatibility) ---
        {
            const auto file = (sandbox / "legacy_keys.json").string();
            { std::ofstream out(file); out << R"({"keyframes":[{"time":0,"value":10,"ease":"Linear"},{"time":2,"value":20,"ease":"Linear"}]})"; }
            auto *fileOwner = sceneContext->CreateEmptyObject("fileOwner");
            auto *fileAnimator = fileOwner->AddComponent<KeyFrameAnimator>();
            Require(fileAnimator->AddAnimation("File", file) && fileAnimator->AddBinding("File", "Transform", "translate_", 0), "file entry");
            Require(fileAnimator->SetCurrentTime("File", 1) && Near(fileOwner->GetComponent<Transform>()->GetTranslate().x, 15), "file keys load and evaluate");
            Require(!fileAnimator->IsEmbedded("File") && Near(fileAnimator->GetDuration("File"), 2), "still a file entry");
            const JSON oldFormat = JSON{{"animations", JSON::array({JSON{{"name", "Old"}, {"jsonPath", "Assets/a.json"}, {"playbackSpeed", 1.0}, {"timeOffset", 0.0},
                {"valueScale", 1.0}, {"valueOffset", 0.0}, {"loop", true}, {"playOnStart", false},
                {"bindings", JSON::array({JSON{{"componentType", "Transform"}, {"componentIndex", 0}, {"parameterName", "translate_"}, {"channel", 0}, {"isScriptVariable", false}}})}}})}};
            Require(fileOwner->LoadComponentFromJson(fileAnimator, JSON{{"customData", oldFormat}}), "old scene format loads");
            Require(fileOwner->SaveComponentToJson(fileAnimator)["data"]["customData"] == oldFormat, "old format round-trips unchanged");
        }

        // --- Clone and scene round trip keep the clip, binding and play settings ---
        subject = sceneContext->GetSceneObject(subjectID);
        const JSON animatorSaved = subject->SaveComponentToJson(Animator(subject));
        auto *copy = sceneContext->CloneObject(subject, "copy", false);
        Require(copy && AnimatorCount(copy) == 1 && copy->SaveComponentToJson(Animator(copy)) == animatorSaved, "clone keeps embedded clip");
        Require(Animator(copy)->GetAnimationData(kMoveName) && Near(Animator(copy)->GetAnimationData(kMoveName)->Evaluate(0.5f), 2), "clone evaluates");
        Scene restored(JSON::parse(scene.SaveToJSON().dump()));
        auto *restoredSubject = restored.GetSceneContext()->GetSceneObject(subjectID);
        Require(restoredSubject && AnimatorCount(restoredSubject) == 1, "round trip keeps animator");
        Require(restoredSubject->SaveComponentToJson(Animator(restoredSubject)) == animatorSaved, "round trip keeps complete clip");
        Require(Animator(restoredSubject)->IsEmbedded(kMoveName) && !Animator(restoredSubject)->GetLoop(kMoveName) && Animator(restoredSubject)->GetPlayOnStart(kMoveName), "round trip keeps play settings");
        Require(Near(restoredSubject->GetComponent<Transform>()->GetTranslate().y, 9), "round trip keeps edit-time value");
        std::puts("Short-text keyframes: real KeyframeAnimation (EaseInOutCubic 0/0.25/2/3.75/4), embedded KeyFrameAnimator entry, plan/undo/redo, re-apply, hand-made clips, play start/end/hold/stop, rotation/collider/scale, replacement, strict load, file compat, clone and round trip passed.");
    } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); result = 1; }
    std::filesystem::current_path(original);
    if (sandbox.parent_path() == std::filesystem::temp_directory_path()) std::filesystem::remove_all(sandbox);
    return result;
}
