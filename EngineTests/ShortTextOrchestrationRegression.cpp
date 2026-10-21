#include "Utilities/ShortTextRecipeJson.h"
#include <iostream>
#include <stdexcept>
using namespace KashipanEngine;
using namespace KashipanEngine::ShortText;
void Require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void Near(float a, float b) { Require(std::abs(a - b) < 0.0001f, "numeric effect"); }
ObjectSnapshot Object(const char *id, const char *name, const char *tag, float x) {
    ObjectSnapshot o; o.id = id; o.name = name; o.tag = tag; o.position[0] = x; o.world[0] = x; return o;
}
int main() {
    try {
        ProgramState s; s.selfId = "controller";
        s.objects = {Object("door", "Door", "Doors", 0), Object("player", "Player", "Actors", 10), Object("enemy1", "E1", "Enemy", 1), Object("enemy2", "E2", "Enemy", 2)};
        InputSnapshot input; ProgramMemory memory;
        Rule named; named.event = Event::Start;
        Action disable{Operation::SetActive, 0, 0}; disable.target = Target::Name; disable.targetName = "Door";
        named.actions = {disable}; TickProgram({named}, memory, s, input, 0.1);
        Require(s.commands.size() == 1 && s.commands[0].targetId == "door", "name resolves stable target ID");
        named.actions[0].target = Target::Tag; named.actions[0].targetName = "Enemy"; memory = {};
        TickProgram({named}, memory, s, input, 0.1); Require(s.commands.size() == 2 && s.commands[0].targetId != s.commands[1].targetId, "tag selects multiple objects");
        named.actions[0].target = Target::Contact; named.event = Event::ContactEnter;
        input.contactReady[0] = true; input.contacts[0] = {"enemy1", "enemy2"}; memory = {};
        TickProgram({named}, memory, s, input, 0.1); Require(s.commands.size() == 2, "contact partner targets are distinct");
        Rule delayed; delayed.event = Event::Start;
        Action wait{Operation::Wait}; wait.duration = 0.5f;
        delayed.actions = {{Operation::SetVariable, 0, 1, "score"}, wait, {Operation::SetVariable, 0, 2, "score"}};
        memory = {}; input = {}; TickProgram({delayed}, memory, s, input, 0.2); Near(s.variables.at("score"), 1);
        TickProgram({delayed}, memory, s, input, 0.3); Near(s.variables.at("score"), 2);
        Rule once; once.once = true; once.actions = {{Operation::AddVariable, 0, 1, "once"}};
        memory = {}; TickProgram({once}, memory, s, input, 0.1); TickProgram({once}, memory, s, input, 0.1); Near(s.variables.at("once"), 1);
        Rule tween; tween.event = Event::Start;
        Action animation{Operation::TweenPosition, 0, 4}; animation.target = Target::Name; animation.targetName = "Door"; animation.duration = 1;
        tween.actions = {animation}; memory = {};
        TickProgram({tween}, memory, s, input, 0.25); Near(FindObject(s, "door")->position[0], 1);
        TickProgram({tween}, memory, s, input, 0.25); Near(FindObject(s, "door")->position[0], 2);
        s.objects.erase(s.objects.begin()); s.objects.push_back(Object("replacement", "Door", "Doors", 100));
        TickProgram({tween}, memory, s, input, 0.5); Require(s.commands.empty(), "deleted target is not replaced by an object with the same name");
        Near(FindObject(s, "replacement")->position[0], 100);
        Rule changed; changed.event = Event::VariableChanged; changed.variable = "score"; changed.actions = {{Operation::AddVariable, 0, 1, "changes"}};
        memory = {}; TickProgram({changed}, memory, s, input, 0.1); Require(!s.variables.contains("changes"), "variable change records baseline without firing");
        s.variables["score"] = 8; TickProgram({changed}, memory, s, input, 0.1); Near(s.variables.at("changes"), 1);
        TickProgram({changed}, memory, s, input, 0.1); Near(s.variables.at("changes"), 1);
        Rule entered; entered.event = Event::ConditionsEntered; entered.conditions = {{Test::DistanceAtMost, 0, 5, "Player"}}; entered.actions = {{Operation::AddVariable, 0, 1, "near"}};
        memory = {}; TickProgram({entered}, memory, s, input, 0.1); Require(!s.variables.contains("near"), "far target fails distance condition");
        FindObject(s, "player")->world[0] = 3; TickProgram({entered}, memory, s, input, 0.1); Near(s.variables.at("near"), 1);
        TickProgram({entered}, memory, s, input, 0.1); Near(s.variables.at("near"), 1);
        Rule mouse; mouse.event = Event::MousePressed; mouse.button = 2; mouse.actions = {{Operation::SetVariable, 0, 5, "mouse"}};
        memory = {}; input.mousePressed[2] = true; TickProgram({mouse}, memory, s, input, 0.1); Near(s.variables.at("mouse"), 5);
        Rule button = mouse; button.event = Event::ButtonClicked;
        memory = {}; input = {}; TickProgram({button}, memory, s, input, 0.1); input.buttonClicked = true;
        TickProgram({button}, memory, s, input, 0.1); Require(s.writtenVariables.contains("mouse"), "button click event");
        TickProgram({button}, memory, s, input, 0.1); Require(s.writtenVariables.contains("mouse"), "consecutive button click pulses are not dropped");
        input.buttonClicked = false; TickProgram({button}, memory, s, input, 0.1); Require(s.writtenVariables.empty(), "no button pulse means no execution");
        Rule follow; follow.actions = {{Operation::Follow}}; follow.actions[0].reference = "Player";
        memory = {}; TickProgram({follow}, memory, s, input, 0.1); Require(s.commands.size() == 1 && s.commands[0].referenceId == "player", "follow reference is bound by ID");
        Recipe recipe; recipe.kind = Kind::Rules; recipe.rules = {named, delayed, once, tween, changed, entered, mouse, button, follow};
        Require(EncodeRecipe(DecodeRecipe(EncodeRecipe(recipe))) == EncodeRecipe(recipe), "expanded recipe serialization");
        auto malformed = recipe; malformed.rules[0].actions[0].target = static_cast<Target>(99); Require(!IsValid(malformed), "invalid target rejected");
        std::cout << "Targets, deletion identity, wait/sequence, once, interpolation, distance, mouse/button and variable-edge regression passed.\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
