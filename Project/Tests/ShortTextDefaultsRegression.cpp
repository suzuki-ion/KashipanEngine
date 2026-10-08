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
            input.pressed.fill(true); input.held.fill(true); input.released.fill(true);
            for (int step = 0; step < 30; ++step) {
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
        Require(recipes.size() >= 16 && events.size() == 6 && actions.size() == 9 && conditions.size() == 4, "generated coverage must include all implemented parts");
        const auto jump = defaults.Interpret("スペースキーを押したらvelocityに16.0fを代入する");
        Require(jump.success && jump.recipe.kind == Kind::Rules, "user's requested phrase is present");
        bool found = false;
        for (const auto &r : jump.recipe.rules) if (r.event == Event::KeyPressed && r.key == Key::Space)
            for (const auto &a : r.actions) if (a.operation == Operation::SetVelocity && a.axis == 1 && a.value == 16) found = true;
        Require(found, "Space event writes Y velocity 16");
        std::cout << "Generated defaults passed: " << defaults.Entries().size() << " phrases, " << recipes.size() << " recipes; all 6 events / 4 conditions / 9 actions covered.\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
