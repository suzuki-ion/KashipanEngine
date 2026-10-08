#include "Utilities/ShortTextRecipeJson.h"
#include <iostream>
#include <stdexcept>
using namespace KashipanEngine;
using namespace KashipanEngine::ShortText;
void Require(bool b, const char *m) { if (!b) throw std::runtime_error(m); }
void Near(float a, float b) { Require(std::abs(a - b) < 0.0001f, "numeric effect"); }
int main() {
    try {
        InputSnapshot input; ProgramState state; ProgramMemory memory;
        Rule jump; jump.event = Event::KeyPressed; jump.key = Key::Space;
        jump.actions = {{Operation::SetVelocity, 1, 16}};
        TickProgram({jump}, memory, state, input, 0.1); Near(state.velocity[1], 0);
        input.pressed[static_cast<size_t>(Key::Space)] = true;
        TickProgram({jump}, memory, state, input, 0.1); Near(state.velocity[1], 16);
        input = {}; state.velocity[1] = 7;
        TickProgram({jump}, memory, state, input, 0.1); Near(state.velocity[1], 7);
        jump.conditions = {{Test::PositionAtMost, 1, 0}, {Test::VelocityAtMost, 1, 0}};
        input.pressed[static_cast<size_t>(Key::Space)] = true;
        state.position[1] = 2; state.velocity[1] = -1;
        TickProgram({jump}, memory, state, input, 0.1); Near(state.velocity[1], -1);
        state.position[1] = 0;
        TickProgram({jump}, memory, state, input, 0.1); Near(state.velocity[1], 16);
        Rule start; start.event = Event::Start; start.actions = {{Operation::SetAcceleration, 1, -30}};
        memory = {}; state = {}; input = {};
        TickProgram({start}, memory, state, input, 0); Near(state.acceleration[1], -30);
        state.acceleration[1] = 0;
        TickProgram({start}, memory, state, input, 0.1); Near(state.acceleration[1], 0);
        Rule held; held.event = Event::KeyHeld; held.key = Key::D;
        held.actions = {{Operation::Translate, 0, 4}, {Operation::Rotate, 2, 90}};
        input.held[static_cast<size_t>(Key::D)] = true;
        state = {}; memory = {};
        TickProgram({held}, memory, state, input, 0.5); Near(state.position[0], 2); Near(state.rotation[2], 0.7853982f);
        Rule release; release.event = Event::KeyReleased; release.key = Key::D;
        release.actions = {{Operation::StopVelocity, 0, 0}};
        state.velocity[0] = 9; input.released[static_cast<size_t>(Key::D)] = true;
        TickProgram({release}, memory, state, input, 0.1); Near(state.velocity[0], 0);
        Rule timer; timer.event = Event::Timer; timer.interval = 0.5f;
        timer.actions = {{Operation::AddVelocity, 0, 2}};
        state = {}; memory = {}; input = {};
        TickProgram({timer}, memory, state, input, 0.2); Near(state.velocity[0], 0);
        TickProgram({timer}, memory, state, input, 0.3); Near(state.velocity[0], 2);
        TickProgram({timer}, memory, state, input, 1); Near(state.velocity[0], 4); // bounded catch-up: one event per frame
        Rule set; set.actions = {{Operation::SetScale, 0, 2}, {Operation::SetPosition, 2, 3}, {Operation::SetRotation, 1, 180}};
        set.conditions = {{Test::PositionAtLeast, 0, -1}, {Test::VelocityAtLeast, 0, 0}};
        TickProgram({set}, memory, state, input, 0.1); Near(state.scale[0], 2); Near(state.position[2], 3); Near(state.rotation[1], 3.1415927f);
        Recipe recipe; recipe.kind = Kind::Rules; recipe.rules = {start, jump, timer, held, release, set};
        Require(IsValid(recipe) && NeedsVelocity(recipe.rules), "valid complete program");
        const auto json = EncodeRecipe(recipe);
        Require(EncodeRecipe(DecodeRecipe(nlohmann::json::parse(json.dump()))) == json, "program JSON round trip");
        for (const auto &name : {"Space", "A", "0", "Left", "Shift", "Enter"}) Require(KeyName(ParseKey(name)) == name, "key mapping");
        Require(ParseKey("mouse") == Key::Unknown, "unknown key rejected");
        auto bad = recipe; bad.rules[0].actions[0].axis = 4; Require(!IsValid(bad), "invalid axis");
        bad = recipe; bad.rules[0].key = static_cast<Key>(999); Require(!IsValid(bad), "invalid key before indexing");
        bad = recipe; bad.rules.resize(17); Require(!IsValid(bad), "rule bound");
        bad = recipe; bad.rules[0].actions.clear(); Require(!IsValid(bad), "no empty rules");
        bad = recipe; bad.rules[0].interval = 0; Require(!IsValid(bad), "timer bound");
        auto malformed = json; malformed["rules"][0]["actions"][0]["operation"] = "execute_script";
        bool failed = false; try { DecodeRecipe(malformed); } catch (...) { failed = true; }
        Require(failed, "unknown action rejected");
        memory = {}; state = {};
        TickProgram({start}, memory, state, input, 2); Require(!memory.started, "unsupported large delta rejected");
        std::cout << "All six events, four conditions, nine actions, keyboard mapping and program serialization passed.\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
