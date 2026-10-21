#include <cstdio>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"
#include "Objects/Components/Collider/SphereCollider.h"
#include "Objects/Components/Collider/Box2DCollider.h"
#include "Objects/Components/AudioSource.h"
#include "Objects/Components/Render/BitmapTextRenderer.h"
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
        ShortText::Recipe program;
        program.kind = ShortText::Kind::Rules;
        ShortText::Rule jump;
        jump.event = ShortText::Event::KeyPressed;
        jump.actions = {{ShortText::Operation::SetVelocity, 1, 16}};
        program.rules.push_back(jump);
        command = MakeShortTextBehaviorCommand(target, program, "スペースキーを押したらY方向の速度を16にする");
        Require(dynamic_cast<CompositeCommand *>(command.get()) != nullptr, "required Velocity and recipe grouped into one undo command");
        Require(!target->GetComponent<Velocity>(), "program command creation is side effect free");
        Require(target->AddComponent<Velocity>() != nullptr, "required Velocity creation");
        Require(target->LoadComponentFromJson(behavior, MakeShortTextBehaviorState(program, "スペースキーを押したらY方向の速度を16にする")), "program component load");
        Require(behavior->GetUpdatePriority() == 0, "rules execute before default Velocity integration");
        const auto programSnapshot = scene.SaveToJSON();
        Scene programRestored(JSON::parse(programSnapshot.dump()));
        const auto *programTarget = programRestored.GetSceneContext()->GetSceneObject(id);
        Require(programTarget && programTarget->GetComponent<ShortTextBehavior>()->GetRecipe().kind == ShortText::Kind::Rules, "program survives scene reload");
        Require(programTarget->GetComponent<Velocity>() != nullptr, "required component survives scene reload");
        auto *receiver = scene.GetSceneContext()->CreateEmptyObject("receiver");
        receiver->AddComponent<SphereCollider>();
        auto *player = scene.GetSceneContext()->CreateEmptyObject("player");
        player->SetTag("Player"); player->AddComponent<SphereCollider>();
        ShortText::Recipe pickup; pickup.kind = ShortText::Kind::Rules;
        ShortText::Rule contact; contact.event = ShortText::Event::ContactEnter; contact.tag = "Player";
        contact.actions = {{ShortText::Operation::AddVariable, 0, 1, "score"}};
        pickup.rules.push_back(contact);
        scene.GetSceneContext()->AddSceneVariable<int>("score", 0);
        ShortText::ProgramState effectState; ShortText::InputSnapshot contactInput; ShortText::ProgramMemory contactMemory;
        auto stepContacts = [&]() {
            ReadShortTextSceneInput(receiver, scene.GetSceneContext(), pickup, effectState, contactInput);
            ShortText::TickProgram(pickup.rules, contactMemory, effectState, contactInput, 0.1);
            ApplyShortTextSceneEffects(receiver, scene.GetSceneContext(), effectState);
        };
        stepContacts();
        Require(contactInput.contactReady[0] && contactInput.contacts[0].size() == 1, "real 3D overlap filtered by object tag");
        Require(scene.GetSceneContext()->GetSceneVariable("score")->IsType<int>() && scene.GetSceneContext()->GetSceneVariable("score")->AnyCast<int>() == 1, "score changes while preserving integer type");
        stepContacts(); Require(scene.GetSceneContext()->GetSceneVariable("score")->AnyCast<int>() == 1, "continued contact does not duplicate score");
        player->GetComponent<Transform>()->SetTranslate(Vector3(100, 0, 0)); stepContacts();
        player->GetComponent<Transform>()->SetTranslate(Vector3(0, 0, 0)); stepContacts();
        Require(scene.GetSceneContext()->GetSceneVariable("score")->AnyCast<int>() == 2, "re-entering a real collider scores again");
        receiver->AddComponent<Box2DCollider>(); player->AddComponent<Box2DCollider>();
        contactMemory = {}; stepContacts();
        Require(contactInput.contacts[0].size() == 1, "2D and 3D colliders on one partner are deduplicated");
        scene.GetSceneContext()->AddSceneVariable<std::string>("score", "keep");
        contactMemory = {}; stepContacts();
        Require(scene.GetSceneContext()->GetSceneVariable("score")->AnyCast<std::string>() == "keep", "non-numeric variable is preserved");
        auto *audio = receiver->AddComponent<AudioSource>(); audio->SetPlayOnAwake(false);
        effectState.effects = {{ShortText::Operation::SetVolume, 0, 0.25f}};
        effectState.writtenVariables.clear();
        ApplyShortTextSceneEffects(receiver, scene.GetSceneContext(), effectState);
        Require(audio->GetVolume() == 0.25f, "audio volume effect reaches the real AudioSource");
        auto *door = scene.GetSceneContext()->CreateEmptyObject("Door");
        auto *label = door->AddComponent<BitmapTextRenderer>();
        scene.GetSceneContext()->AddSceneVariable<int>("displayScore", 7);
        auto applyRecipe = [&](const ShortText::Recipe &r, double dt = 0.1) {
            ShortText::ProgramState state;
            auto *tr = receiver->GetComponent<Transform>();
            const auto p = tr->GetTranslate(), rot = tr->GetRotate(), scale = tr->GetScale();
            state.position = {p.x, p.y, p.z}; state.rotation = {rot.x, rot.y, rot.z}; state.scale = {scale.x, scale.y, scale.z};
            ShortText::InputSnapshot inputs; ShortText::ProgramMemory runtime;
            ReadShortTextSceneInput(receiver, scene.GetSceneContext(), r, state, inputs);
            ShortText::TickProgram(r.rules, runtime, state, inputs, dt);
            ApplyShortTextSceneEffects(receiver, scene.GetSceneContext(), state);
        };
        ShortText::Recipe controls; controls.kind = ShortText::Kind::Rules;
        ShortText::Rule control;
        ShortText::Action hide{ShortText::Operation::SetVisible, 0, 0}; hide.target = ShortText::Target::Name; hide.targetName = "Door";
        ShortText::Action text{ShortText::Operation::ShowVariableText, 0, 0, "displayScore"}; text.target = hide.target; text.targetName = "Door"; text.text = "Score: ";
        control.actions = {hide, text}; controls.rules = {control}; applyRecipe(controls);
        Require(!label->IsActive() && label->GetText() == "Score: 7", "named target visibility and numeric text display");
        controls.rules[0].actions = {hide}; controls.rules[0].actions[0].operation = ShortText::Operation::SetActive;
        applyRecipe(controls); Require(!door->IsActive(), "disable named object");
        controls.rules[0].actions[0].value = 1; applyRecipe(controls); Require(door->IsActive(), "inactive named object can be enabled by another controller");
        auto *parent = scene.GetSceneContext()->CreateEmptyObject("Parent");
        parent->GetComponent<Transform>()->SetTranslate(Vector3(10, 0, 0)); parent->GetComponent<Transform>()->SetScale(Vector3(2, 2, 2));
        receiver->GetComponent<Transform>()->SetParentObject(parent);
        player->GetComponent<Transform>()->SetTranslate(Vector3(4, 2, 0));
        ShortText::Action follow{ShortText::Operation::Follow, 1, 3}; follow.reference = "player";
        controls.rules[0].actions = {follow}; applyRecipe(controls);
        auto world = receiver->GetComponent<Transform>()->GetWorldPosition();
        Require(std::abs(world.x - 4) < 0.001f && std::abs(world.y - 5) < 0.001f, "world follow converts through scaled parent");
        follow.operation = ShortText::Operation::MoveToward; follow.value = 1;
        controls.rules[0].actions = {follow}; applyRecipe(controls, 0.5);
        Require(std::abs(receiver->GetComponent<Transform>()->GetWorldPosition().y - 4.5f) < 0.001f, "move toward is speed limited");
        ShortText::Action create{ShortText::Operation::CreateObject, 0, 0, "Created"}; controls.rules[0].actions = {create}; applyRecipe(controls);
        Require(scene.GetSceneContext()->GetSceneObject("Created") != nullptr, "empty object creation");
        ShortText::Action cloneAction{ShortText::Operation::CloneObject, 0, 0, "Copy"}; cloneAction.target = ShortText::Target::Name; cloneAction.targetName = "Created";
        controls.rules[0].actions = {cloneAction}; applyRecipe(controls);
        Require(scene.GetSceneContext()->GetSceneObject("Copy") != nullptr, "named template cloning");
        cloneAction.operation = ShortText::Operation::DeleteObject; cloneAction.targetName = "Copy";
        controls.rules[0].actions = {cloneAction}; applyRecipe(controls); Require(scene.GetSceneContext()->GetSceneObject("Copy") == nullptr, "named object deletion");
        ShortText::Action transition{ShortText::Operation::ChangeScene, 0, 0, "NextScene"}; controls.rules[0].actions = {transition};
        applyRecipe(controls); Require(scene.GetSceneContext()->HasNextSceneName(), "scene transition request reaches scene context");
        std::puts("Scene round trip, command routing, 2D/3D contact identity, numeric type preservation and audio-volume bridge passed.");
    } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); result = 1; }
    std::filesystem::current_path(original);
    if (sandbox.parent_path() == std::filesystem::temp_directory_path()) std::filesystem::remove_all(sandbox);
    return result;
}
