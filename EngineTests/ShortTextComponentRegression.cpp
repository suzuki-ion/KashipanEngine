// Native regression: short text → real components through undoable editor commands.
// Uses real Scene / SceneEditorContext / components. Headless: no window, GPU or audio device.
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/ShortTextBehaviorEditor.h"
#include "Scene/Editor/ShortTextComponentPlan.h"
#include "Objects/Components/PropertyMotion.h"
#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"
#include "Objects/Components/AudioSource.h"
#include "Objects/Components/Collider/BoxCollider.h"
#include "Objects/Components/Collider/Box2DCollider.h"

using namespace KashipanEngine;
namespace {
void Require(bool value, const std::string &message) { if (!value) throw std::runtime_error(message); }
bool Near(float a, float b, float e = 1e-4f) { return std::abs(a - b) <= e; }
size_t MotionCount(const EmptyObject *object) { return object ? object->GetComponents<PropertyMotion>().size() : 0; }
PropertyMotion *Motion(const EmptyObject *object, size_t index = 0) {
    auto motions = object ? object->GetComponents<PropertyMotion>() : std::vector<PropertyMotion *>{};
    return index < motions.size() ? motions[index] : nullptr;
}
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
// The full rejection path: parse or plan fails, nothing in the scene changes.
void RejectWithoutMutation(Scene &scene, EmptyObject *target, const std::string &text) {
    const auto before = scene.SaveToJSON().dump();
    const auto parsed = ParseShortTextPropertyRequest(text, target);
    if (parsed.success) {
        ShortTextComponentPlan plan;
        Require(!MakeShortTextPropertyCommand(target, parsed.request, text, &plan) && !plan.success, "must reject: " + text);
    } else Require(parsed.recognized && !parsed.message.empty(), "rejected with message: " + text);
    Require(scene.SaveToJSON().dump() == before, "rejection must not mutate: " + text);
}
}

int main() {
    const auto original = std::filesystem::current_path();
    const auto sandbox = std::filesystem::temp_directory_path() /
        ("KashipanShortTextComponent-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(sandbox);
    std::filesystem::current_path(sandbox);
    int result = 0;
    try {
        Scene scene(std::string("ShortTextComponentRegression"));
        // Test-only access token (Passkey is trivially copyable); GameEngine is not booted.
        SceneEditorContext context(std::bit_cast<Passkey<Scene>>(std::uint8_t{}), &scene);
        auto *sceneContext = scene.GetSceneContext();
        const auto updateKey = std::bit_cast<Passkey<EmptyObject>>(std::uint8_t{});

        // --- Transform move: PropertyMotion with absolute local range ---
        auto *subject = sceneContext->CreateEmptyObject("subject");
        const auto subjectID = subject->GetObjectID();
        subject->GetComponent<Transform>()->SetTranslate(Vector3(5, 7, 9));
        const std::string move = "Transformの移動X軸を-1.0fから1.0fの間で往復移動";
        {
            const auto parsed = ParseShortTextPropertyRequest(move, subject);
            ShortTextComponentPlan plan;
            auto command = MakeShortTextPropertyCommand(subject, parsed.request, move, &plan);
            Require(command && plan.success && !plan.steps.empty(), "move plan");
            Require(MotionCount(subject) == 0, "planning is side-effect free");
        }
        auto moveApplied = Apply(context, subject, move);
        Require(MotionCount(subject) == 1, "PropertyMotion added");
        auto *motion = Motion(subject);
        const auto &settings = motion->GetSettings();
        Require(settings.component == "Transform" && settings.member == "translate_" && settings.axis == 0 &&
            Near(settings.from, -1) && Near(settings.to, 1) && Near(settings.period, 2) && settings.shortTextManaged &&
            settings.sourceText == move && settings.unit == ShortText::PropertyUnit::Raw && settings.wave == ShortText::PropertyWave::Linear, "motion settings");
        auto translate = subject->GetComponent<Transform>()->GetTranslate();
        Require(Near(translate.x, 5) && Near(translate.y, 7) && Near(translate.z, 9), "edit-time position is not moved by applying");
        motion->UpdateInterface(updateKey);
        Require(Near(subject->GetComponent<Transform>()->GetTranslate().x, 5), "Update does nothing while not playing");
        // Re-applying the same text updates instead of duplicating.
        auto again = Apply(context, subject, move);
        Require(MotionCount(subject) == 1, "re-application does not add a duplicate");
        Require(dynamic_cast<CompositeCommand *>(again.command.get()) != nullptr, "one composite command");
        // Absolute evaluation while advancing.
        Require(motion->Advance(0) && Near(subject->GetComponent<Transform>()->GetTranslate().x, -1), "t=0 writes the start value");
        Require(motion->Advance(0.5) && Near(subject->GetComponent<Transform>()->GetTranslate().x, 0), "quarter period");
        Require(motion->Advance(0.5) && Near(subject->GetComponent<Transform>()->GetTranslate().x, 1), "half period");
        for (int i = 0; i < 400; ++i) {
            Require(motion->Advance(0.0371), "advance");
            translate = subject->GetComponent<Transform>()->GetTranslate();
            Require(translate.x >= -1.0001f && translate.x <= 1.0001f, "X stays in absolute -1..1 from initial 5");
            Require(Near(translate.y, 7) && Near(translate.z, 9), "Y/Z preserved");
        }
        Require(!motion->Advance(std::numeric_limits<double>::quiet_NaN()) && !motion->Advance(-1), "invalid dt rejected");

        // --- Play / Stop restore the edit-time position ---
        subject->GetComponent<Transform>()->SetTranslate(Vector3(5, 7, 9));
        scene.PlayStart();
        Require(scene.IsPlaying(), "playing");
        subject = sceneContext->GetSceneObject(subjectID);
        motion = Motion(subject);
        Require(motion != nullptr, "motion while playing");
        motion->UpdateInterface(updateKey);
        Require(Near(static_cast<float>(motion->GetElapsed()), 0) && Near(subject->GetComponent<Transform>()->GetTranslate().x, -1), "first played frame starts at the start value");
        Require(Near(subject->GetComponent<Transform>()->GetTranslate().y, 7), "first played frame keeps Y");
        Require(motion->Advance(0.25) && Near(subject->GetComponent<Transform>()->GetTranslate().x, -0.5f), "playing moves to absolute value");
        scene.PlayStop();
        subject = sceneContext->GetSceneObject(subjectID);
        Require(subject && Near(subject->GetComponent<Transform>()->GetTranslate().x, 5), "stop restores the edit-time value");
        Require(MotionCount(subject) == 1 && Near(Motion(subject)->GetSettings().from, -1), "motion restored after stop");

        // --- Rotation in degrees, separate axis coexists ---
        auto rotateApplied = Apply(context, subject, "Transformの回転Y軸を-90.0fから90.0fの間で往復");
        Require(MotionCount(subject) == 2, "different target adds a second motion");
        auto *rotation = Motion(subject, 1);
        Require(rotation->GetSettings().unit == ShortText::PropertyUnit::Degrees && Near(rotation->GetSettings().from, -90), "rotation stored in degrees");
        Require(rotation->Advance(0) && Near(subject->GetComponent<Transform>()->GetRotate().y, -1.5707963f), "degrees written as radians");
        Require(rotation->Advance(1) && Near(subject->GetComponent<Transform>()->GetRotate().y, 1.5707963f), "rotation reaches +90 degrees");
        Require(rotateApplied.command->Undo(&context) && MotionCount(subject) == 1, "undo rotation motion");
        Require(rotateApplied.command->Execute(&context) && MotionCount(subject) == 2, "redo rotation motion");

        // --- Undo / Redo of the first application ---
        auto *undoTarget = sceneContext->CreateEmptyObject("undo");
        auto undoApplied = Apply(context, undoTarget, "Transformの移動 X 軸を -1 から 1 まで往復移動");
        Require(MotionCount(undoTarget) == 1, "applied");
        Require(undoApplied.command->Undo(&context) && MotionCount(undoTarget) == 0, "undo removes the motion");
        Require(undoApplied.command->Execute(&context) && MotionCount(undoTarget) == 1 && Near(Motion(undoTarget)->GetSettings().to, 1), "redo restores the motion");

        // --- BoxCollider size: added when missing, set at edit time, undoable ---
        auto *box = sceneContext->CreateEmptyObject("box");
        const std::string setSize = "BoxColliderのサイズX軸を2.0fにする";
        auto setApplied = Apply(context, box, setSize);
        auto *collider = box->GetComponent<BoxCollider>();
        Require(collider && Near(collider->GetSize().x, 2), "BoxCollider added and size set");
        const auto defaultSize = BoxCollider().GetSize();
        Require(Near(collider->GetSize().y, defaultSize.y) && Near(collider->GetSize().z, defaultSize.z), "other size components keep defaults");
        Require(MotionCount(box) == 0, "a set does not create a runtime controller");
        Require(setApplied.command->Undo(&context) && !box->GetComponent<BoxCollider>(), "undo removes the added collider");
        Require(setApplied.command->Execute(&context) && box->GetComponent<BoxCollider>() && Near(box->GetComponent<BoxCollider>()->GetSize().x, 2), "redo adds and sets again");
        // Ping-pong reuses the existing collider.
        auto boxRange = Apply(context, box, "BoxColliderのサイズX軸を1.0fから3.0fの間で往復");
        Require(box->GetComponents<BoxCollider>().size() == 1 && MotionCount(box) == 1, "existing collider reused, one motion");
        Require(Motion(box)->Advance(1) && Near(box->GetComponent<BoxCollider>()->GetSize().x, 3), "collider size driven");
        box->GetComponent<BoxCollider>()->SetSize(Vector3(2, defaultSize.y, defaultSize.z));
        // A later fixed value replaces the short-text motion of the same member (undoably).
        auto fixed = Apply(context, box, "BoxColliderのサイズX軸を2.5にする");
        Require(MotionCount(box) == 0 && Near(box->GetComponent<BoxCollider>()->GetSize().x, 2.5f), "set replaces managed motion");
        Require(fixed.command->Undo(&context) && MotionCount(box) == 1 && Near(box->GetComponent<BoxCollider>()->GetSize().x, 2), "undo restores motion and value");
        Require(fixed.command->Execute(&context) && MotionCount(box) == 0, "redo");
        // A manually configured motion is never deleted.
        auto *manual = box->AddComponent<PropertyMotion>();
        auto manualSettings = manual->GetSettings();
        manualSettings.component = "BoxCollider"; manualSettings.member = "size_"; manualSettings.axis = 0; manualSettings.from = 1; manualSettings.to = 4;
        Require(manual->SetSettings(manualSettings), "manual motion");
        auto keepManual = Apply(context, box, "BoxColliderのサイズX軸を3にする");
        Require(MotionCount(box) == 1 && !keepManual.plan.requirements.empty(), "manual motion kept and reported");

        // --- Box2DCollider and formal names ---
        auto *flat = sceneContext->CreateEmptyObject("flat");
        Apply(context, flat, "Box2DColliderのsize_ Y軸を0.5から1.5の間で往復");
        Require(flat->GetComponent<Box2DCollider>() && MotionCount(flat) == 1, "Box2DCollider added for Vector2 member");
        Require(Motion(flat)->Advance(0) && Near(flat->GetComponent<Box2DCollider>()->GetSize().y, 0.5f), "Vector2 member driven");

        // --- Invalid requests do not mutate ---
        for (const char *text : {"Transformの移動X軸を1から-1の間で往復移動", "Transformの移動X軸を2から2の間で往復移動",
            "Transformの移動X軸を-20000から1の間で往復移動", "Transformの移動を-1から1の間で往復移動", "BoxColliderのサイズを2にする",
            "Box2DColliderのサイズZ軸を2にする", "BoxColliderのサイズX軸を0にする", "BoxColliderのサイズX軸を-1から3の間で往復",
            "Transformの worldMatrix_ X軸を1にする", "Transformの速さX軸を1にする", "FooColliderのサイズX軸を1にする",
            "Transformの移動X軸を-1から1の間で周期0.01秒で往復", "PropertyMotionのfrom_を1にする"})
            RejectWithoutMutation(scene, flat, text);
        {
            const auto before = scene.SaveToJSON().dump();
            auto request = ParseShortTextPropertyRequest(move, flat).request;
            auto bad = request; bad.from = std::numeric_limits<float>::quiet_NaN();
            Require(!MakeShortTextPropertyCommand(flat, bad, move), "non-finite request rejected");
            bad = request; bad.axis = 2; bad.component = "Box2DCollider"; bad.member = "size_"; bad.positiveOnly = true; bad.from = 1; bad.to = 2;
            Require(!MakeShortTextPropertyCommand(flat, bad, move), "Z on Vector2 rejected at plan level");
            bad = request; bad.member = "worldMatrix_";
            Require(!MakeShortTextPropertyCommand(flat, bad, move), "readonly rejected at plan level");
            Require(!MakeShortTextPropertyCommand(nullptr, request, move), "missing target rejected");
            Require(!MakeShortTextPropertyCommand(flat, request, std::string(600, 'a')), "oversized source text rejected");
            Require(scene.SaveToJSON().dump() == before, "plan-level rejection does not mutate");
        }

        // --- Legacy ShortTextBehavior and PropertyMotion never drive the same member twice ---
        auto *legacy = sceneContext->CreateEmptyObject("legacy");
        const auto bob = ShortText::Parse("左右に振幅2で周期3秒で往復して");
        Require(bob.success, "legacy parse");
        auto legacyCommand = MakeShortTextBehaviorCommand(legacy, bob.recipe, "左右に振幅2で周期3秒で往復して");
        Require(legacyCommand && legacyCommand->Execute(&context) && legacy->GetComponent<ShortTextBehavior>(), "legacy applied");
        auto replace = Apply(context, legacy, move);
        Require(!legacy->GetComponent<ShortTextBehavior>() && MotionCount(legacy) == 1, "conflicting legacy behavior replaced");
        Require(replace.command->Undo(&context) && legacy->GetComponent<ShortTextBehavior>() && MotionCount(legacy) == 0, "undo restores legacy behavior");
        Require(replace.command->Execute(&context) && MotionCount(legacy) == 1, "redo replacement");
        auto back = MakeShortTextBehaviorCommand(legacy, bob.recipe, "左右に振幅2で周期3秒で往復して");
        Require(back && back->Execute(&context) && legacy->GetComponent<ShortTextBehavior>() && MotionCount(legacy) == 0, "legacy recipe replaces managed motion");
        Require(back->Undo(&context) && !legacy->GetComponent<ShortTextBehavior>() && MotionCount(legacy) == 1, "undo legacy replacement");
        const auto spin = ShortText::Parse("Y軸で毎秒90度回転して");
        auto spinCommand = MakeShortTextBehaviorCommand(legacy, spin.recipe, "Y軸で毎秒90度回転して");
        Require(spinCommand && spinCommand->Execute(&context) && MotionCount(legacy) == 1, "non-conflicting legacy recipe keeps motion");

        // --- Legacy recipes: required components are planned at edit time ---
        auto *rules = sceneContext->CreateEmptyObject("rules");
        ShortText::Recipe program; program.kind = ShortText::Kind::Rules;
        ShortText::Rule jump; jump.event = ShortText::Event::KeyPressed;
        jump.actions = {{ShortText::Operation::SetVelocity, 1, 16}, {ShortText::Operation::PlaySound, 0, 0}};
        program.rules.push_back(jump);
        Require(ShortText::IsValid(program), "program valid");
        const auto programPlan = PlanShortTextBehavior(rules, program);
        Require(programPlan.success && programPlan.steps.size() == 2 && !programPlan.requirements.empty(), "plan lists Velocity and AudioSource requirement");
        auto programCommand = MakeShortTextBehaviorCommand(rules, program, "ジャンプして音を鳴らす");
        Require(programCommand && programCommand->Execute(&context), "program applied");
        Require(rules->GetComponent<Velocity>() && rules->GetComponent<ShortTextBehavior>() && !rules->GetComponent<AudioSource>(), "Velocity added, AudioSource not guessed");
        Require(programCommand->Undo(&context) && !rules->GetComponent<Velocity>() && !rules->GetComponent<ShortTextBehavior>(), "one undo removes both");
        ShortText::Recipe propertyProgram; propertyProgram.kind = ShortText::Kind::Rules;
        ShortText::Rule grow; ShortText::Action set{ShortText::Operation::EngineCommand, 0, 0, "property.set"};
        set.data = ShortText::Value{{"path", "BoxCollider.size_.x"}, {"value", 2}};
        grow.actions = {set}; propertyProgram.rules.push_back(grow);
        auto needs = AnalyzeShortTextRecipeNeeds(rules, propertyProgram);
        Require(needs.add.size() == 1 && needs.add[0] == "BoxCollider", "asset-free property component is pre-added");
        set.name = "rigid_impulse"; set.data = ShortText::Value{{"value", ShortText::Value::array({0, 16, 0})}};
        propertyProgram.rules[0].actions = {set};
        needs = AnalyzeShortTextRecipeNeeds(rules, propertyProgram);
        Require(needs.add.empty() && needs.notes.size() == 1, "rigid body type is not guessed");
        ShortText::Rule touch; touch.event = ShortText::Event::ContactEnter; touch.actions = {{ShortText::Operation::AddVariable, 0, 1, "score"}};
        propertyProgram.rules = {touch};
        needs = AnalyzeShortTextRecipeNeeds(rules, propertyProgram);
        Require(needs.add.empty() && needs.notes.size() == 1, "collider shape is not guessed for contact");

        // --- Save / load / clone ---
        subject = sceneContext->GetSceneObject(subjectID);
        const auto savedMotion = subject->SaveComponentToJson(Motion(subject));
        auto invalid = savedMotion;
        invalid["data"]["customData"]["from"] = 3;
        Require(!subject->LoadComponentFromJson(Motion(subject), invalid), "reversed range rejected on load");
        invalid = savedMotion; invalid["data"]["customData"]["unit"] = "bogus";
        Require(!subject->LoadComponentFromJson(Motion(subject), invalid), "unknown unit rejected");
        invalid = savedMotion; invalid["data"]["customData"]["axis"] = 9;
        Require(!subject->LoadComponentFromJson(Motion(subject), invalid), "axis out of range rejected");
        invalid = savedMotion; invalid["data"]["customData"]["period"] = "2";
        Require(!subject->LoadComponentFromJson(Motion(subject), invalid), "wrong type rejected");
        Require(subject->SaveComponentToJson(Motion(subject)) == savedMotion, "rejected load keeps the previous state");
        auto *copy = sceneContext->CloneObject(subject, "copy", false);
        Require(copy && MotionCount(copy) == 2 && copy->SaveComponentToJson(Motion(copy)) == savedMotion, "clone keeps settings");
        Scene restored(JSON::parse(scene.SaveToJSON().dump()));
        auto *restoredSubject = restored.GetSceneContext()->GetSceneObject(subjectID);
        Require(restoredSubject && MotionCount(restoredSubject) == 2, "scene round trip keeps motions");
        Require(restoredSubject->SaveComponentToJson(Motion(restoredSubject)) == savedMotion, "scene round trip keeps complete settings");
        Require(Near(restoredSubject->GetComponent<Transform>()->GetTranslate().x, 5), "round trip keeps edit-time value");
        std::puts("Short-text components: PropertyMotion add/update/undo/redo, BoxCollider add+set, legacy replacement, requirement planning, play/stop and save/load passed.");
    } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); result = 1; }
    std::filesystem::current_path(original);
    if (sandbox.parent_path() == std::filesystem::temp_directory_path()) std::filesystem::remove_all(sandbox);
    return result;
}
