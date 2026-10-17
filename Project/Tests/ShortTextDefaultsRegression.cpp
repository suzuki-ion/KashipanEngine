#include "Utilities/ShortTextLearning.h"
#include "Utilities/ShortTextRecipeJson.h"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace KashipanEngine;
using namespace KashipanEngine::ShortText;
void Require(bool b, const char *m) { if (!b) throw std::runtime_error(m); }
int wmain(int argc, wchar_t **argv) {
    try {
        Require(argc == 2, "pass the generated candidate JSON path");
        LearningStore defaults{std::filesystem::path(argv[1])};
        std::string error;
        if (!defaults.Reload(error)) throw std::runtime_error(error);
        Require(defaults.Entries().size() >= 40, "at least 40 verified phrases expected");
        std::set<std::string> recipes;
        std::set<int> events, actions, conditions;
        for (const auto &entry : defaults.Entries()) {
            Require(entry.mode == LearningMode::Recipe, "generated examples must be exact recipes");
            const auto decoded = defaults.Interpret(entry.phrase);
            Require(decoded.success && EncodeRecipe(decoded.recipe) == EncodeRecipe(entry.recipe), "generated phrase resolves to exactly its saved recipe");
            recipes.insert(EncodeRecipe(entry.recipe).dump());
            for (const auto &r : entry.recipe.rules) {
                events.insert(static_cast<int>(r.event));
                for (const auto &a : r.actions) actions.insert(static_cast<int>(a.operation));
                for (const auto &c : r.conditions) conditions.insert(static_cast<int>(c.test));
            }
            ProgramMemory memory; ProgramState state; InputSnapshot input;
            state.selfId = "controller";
            for (const auto &[id, name, tag] : std::vector<std::array<std::string, 3>>{{"controller", "Controller", ""}, {"player", "Player", "Player"}, {"door", "Door", "Door"},
                {"hud", "HUD", "UI"}, {"enemy", "Enemy", "Enemy"}, {"template", "BulletTemplate", "Bullet"}, {"A", "CoinA", "Coin"}, {"B", "CoinB", "Coin"}}) {
                ObjectSnapshot o; o.id = id; o.name = name; o.tag = tag; state.objects.push_back(o);
            }
            state.variables["score"] = 0; state.variables["hp"] = 100;
            input.pressed.fill(true); input.held.fill(true); input.released.fill(true);
            for (int step = 0; step < 30; ++step) {
                input.mousePressed.fill(step == 1); input.mouseHeld.fill(step < 10); input.mouseReleased.fill(step == 10);
                input.buttonClicked = step == 1;
                if (step == 10) state.variables["score"] = 10;
                if (step == 20) state.variables["hp"] = 0;
                FindObject(state, "player")->world[0] = step < 10 ? 20.0f : 3.0f;
                for (size_t i = 0; i < entry.recipe.rules.size(); ++i) if (IsContact(entry.recipe.rules[i].event)) {
                    input.contactReady[i] = true;
                    input.contacts[i] = step < 10 ? std::vector<std::string>{"A", "B"} : step < 20 ? std::vector<std::string>{"B"} : std::vector<std::string>{};
                }
                TickProgram(entry.recipe.rules, memory, state, input, 0.1);
                if (NeedsVelocity(entry.recipe.rules)) for (size_t axis = 0; axis < 3; ++axis) {
                    state.velocity[axis] += state.acceleration[axis] * 0.1f;
                    state.position[axis] += state.velocity[axis] * 0.1f;
                }
                for (const auto &v : {state.position, state.rotation, state.scale, state.velocity, state.acceleration})
                    for (float value : v) Require(std::isfinite(value), "generated program stays finite in synthetic execution");
                input.pressed.fill(false); input.released.fill(false);
            }
        }
        Require(recipes.size() >= 16 && events.size() == EventNames.size() && actions.size() == OperationNames.size() && conditions.size() == TestNames.size(), "generated coverage must include all implemented parts");
        const auto jump = defaults.Interpret("スペースキーを押したらvelocityに16.0fを代入する");
        Require(jump.success && jump.recipe.kind == Kind::Rules, "user's requested phrase is present");
        bool found = false;
        for (const auto &r : jump.recipe.rules) if (r.event == Event::KeyPressed && r.key == Key::Space)
            for (const auto &a : r.actions) if (a.operation == Operation::SetVelocity && a.axis == 1 && a.value == 16) found = true;
        Require(found, "Space event writes Y velocity 16");
        const auto pickup = defaults.Interpret("Playerに触れたらscoreを1増やして音を鳴らす");
        Require(pickup.success && pickup.recipe.kind == Kind::Rules, "contact score and audio phrase present");
        ProgramMemory pickupMemory; ProgramState pickupState; InputSnapshot pickupInput;
        for (size_t i = 0; i < pickup.recipe.rules.size(); ++i) if (IsContact(pickup.recipe.rules[i].event)) {
            Require(pickup.recipe.rules[i].tag == "Player", "Player filter preserved");
            pickupInput.contactReady[i] = true; pickupInput.contacts[i] = {"player"};
        }
        TickProgram(pickup.recipe.rules, pickupMemory, pickupState, pickupInput, 0.1);
        Require(pickupState.variables.at("score") == 1 && pickupState.effects.size() == 1 && pickupState.effects[0].operation == Operation::PlaySound,
            "contact phrase adds score and plays configured sound");
        TickProgram(pickup.recipe.rules, pickupMemory, pickupState, pickupInput, 0.1);
        Require(pickupState.variables.at("score") == 1 && pickupState.effects.empty(), "continued contact does not repeat pickup");
        // One-shot keyframe sentences belong to the property grammar; the dictionary must not claim them first.
        for (const char *tween : {"1秒かけてTransformの移動Y軸を0.0fから4.0fまでEaseInOutCubicで移動", "Transformの移動Y軸を0から4まで1秒かけてEaseInOutCubicで移動",
            "2秒かけてTransformの回転Z軸を0.0fから90.0fまでEaseOutQuadで回転", "1秒かけてTransformのスケールX軸を1.0fから2.0fまでLinearで変更",
            "1秒かけてBoxColliderのサイズX軸を1.0fから3.0fまでEaseInOutCubicで変更"})
            Require(!defaults.Interpret(tween).success, "dictionary does not claim keyframe sentences");
        std::cout << "Generated defaults passed: " << defaults.Entries().size() << " phrases, " << recipes.size() << " recipes; all " << events.size() << " events / " << conditions.size() << " conditions / " << actions.size() << " actions covered.\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
