#include <cstdio>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/Transform.h"
#include "Scene/Editor/ShortTextBehaviorEditor.h"
#include "Scene/Editor/SceneEditorCommands.h"

using namespace KashipanEngine;
void Require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main() {
    const auto original = std::filesystem::current_path();
    const auto sandbox = std::filesystem::temp_directory_path() /
        ("KashipanShortTextScene-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(sandbox);
    std::filesystem::current_path(sandbox);
    int result = 0;
    try {
        Scene scene(std::string("ShortTextRegression"));
        auto *target = scene.GetSceneContext()->CreateEmptyObject("subject");
        const auto id = target->GetObjectID();
        const auto bob = ShortText::Parse("左右に振幅2で周期3秒で往復して");
        Require(bob.success, "parse scene recipe");
        auto command = MakeShortTextBehaviorCommand(target, bob.recipe, "左右に振幅2で周期3秒で往復して");
        Require(dynamic_cast<AddComponentCommand *>(command.get()) != nullptr, "first application uses undoable addition");
        Require(target->GetComponent<ShortTextBehavior>() == nullptr, "command creation is side effect free");
        auto *behavior = target->AddComponent<ShortTextBehavior>();
        Require(behavior != nullptr, "registered component creation");
        Require(target->LoadComponentFromJson(behavior, MakeShortTextBehaviorState(bob.recipe, "左右に振幅2で周期3秒で往復して")), "load recipe state");
        const auto before = target->SaveComponentToJson(behavior);
        auto bad = before;
        bad["data"]["customData"]["axis"] = 4;
        Require(!target->LoadComponentFromJson(behavior, bad), "invalid axis is rejected");
        Require(target->SaveComponentToJson(behavior) == before, "invalid recipe retains prior state");
        bad = before; bad["data"]["customData"]["kind"] = 256;
        Require(!target->LoadComponentFromJson(behavior, bad), "large enum cannot wrap into valid kind");
        bad = before; bad["data"]["customData"]["kind"] = 4294967296ULL;
        Require(!target->LoadComponentFromJson(behavior, bad), "integer overflow rejected before conversion");
        bad = before; bad["data"]["customData"]["axis"] = 0.5;
        Require(!target->LoadComponentFromJson(behavior, bad), "fractional axis cannot truncate into a valid axis");
        bad = before; bad["data"]["customData"]["direction"] = 18446744073709551615ULL;
        Require(!target->LoadComponentFromJson(behavior, bad), "unsigned direction cannot wrap to negative one");
        bad = before; bad["data"]["customData"]["period"] = "oops";
        Require(!target->LoadComponentFromJson(behavior, bad), "invalid serialized types rejected");
        const auto spin = ShortText::Parse("右に毎秒90度回して");
        command = MakeShortTextBehaviorCommand(target, spin.recipe, "右に毎秒90度回して");
        Require(dynamic_cast<ComponentEditCommand *>(command.get()) != nullptr, "replacement uses undoable edit");
        Require(target->SaveComponentToJson(behavior) == before, "replacement command creation does not mutate");
        auto *clone = scene.GetSceneContext()->CloneObject(target, "clone", false);
        Require(clone && clone->GetComponent<ShortTextBehavior>(), "clone retains short-text component");
        Require(clone->GetComponent<ShortTextBehavior>()->GetRecipe().amplitude == 2, "clone recipe");
        const auto snapshot = scene.SaveToJSON();
        Scene restored(JSON::parse(snapshot.dump()));
        auto *restoredTarget = restored.GetSceneContext()->GetSceneObject(id);
        Require(restoredTarget && restoredTarget->GetComponent<ShortTextBehavior>(), "scene reload retains component");
        Require(restoredTarget->SaveComponentToJson(restoredTarget->GetComponent<ShortTextBehavior>()) == before, "scene round trip retains complete recipe");
        auto invalidRecipe = bob.recipe; invalidRecipe.speed = 0;
        Require(!MakeShortTextBehaviorCommand(target, invalidRecipe, "invalid"), "invalid command refused");
        Require(!MakeShortTextBehaviorCommand(nullptr, bob.recipe, "missing"), "missing target refused");
        std::puts("Short-text scene registration, clone, JSON round trip, invalid-state recovery and command routing passed.");
    } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); result = 1; }
    std::filesystem::current_path(original);
    if (sandbox.parent_path() == std::filesystem::temp_directory_path()) std::filesystem::remove_all(sandbox);
    return result;
}
