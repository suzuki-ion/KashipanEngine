#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/Components/Transform.h"
#include "Math/Vector3.h"

namespace KashipanEngine::ShortText {
bool ExecuteSpatialCommand(const std::string &command, const Value &args, CommandContext &context, std::string &error);
}

using namespace KashipanEngine;
static void Require(bool b, const char *s) { if (!b) throw std::runtime_error(s); }
static bool Near(double a, double b, double eps = 1e-4) { return std::abs(a - b) <= eps; }
static bool Same(const Vector3 &a, const Vector3 &b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static Transform *EnsureTransform(EmptyObject *o) { auto *t = o->GetComponent<Transform>(); if (!t) t = o->AddComponent<Transform>(); return t; }
static bool NearArray(const ShortText::Value &v, double x, double y, double z) {
    return v.is_array() && v.size() == 3 && Near(v[0].get<double>(), x) && Near(v[1].get<double>(), y) && Near(v[2].get<double>(), z);
}

int main() {
    auto original = std::filesystem::current_path();
    auto temp = std::filesystem::temp_directory_path() / ("ShortTextSpatial-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp); std::filesystem::current_path(temp); int code = 0;
    try {
        const double pi = std::numbers::pi;
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        Scene scene(std::string("SpatialRecipes")); auto *context = scene.GetSceneContext();
        auto *actor = context->CreateEmptyObject("Actor"); auto *actorT = EnsureTransform(actor); actorT->SetTranslate({1, 2, 3});
        auto *goal = context->CreateEmptyObject("Goal"); auto *goalT = EnsureTransform(goal); goalT->SetTranslate({4, 6, 3});
        auto *twinA = context->CreateEmptyObject("Twin"); auto *twinAT = EnsureTransform(twinA); twinAT->SetTranslate({1, 2, 13});
        auto *twinB = context->CreateEmptyObject("Twin"); auto *twinBT = EnsureTransform(twinB); twinBT->SetTranslate({-9, 2, 3});

        ShortText::ProgramMemory memory; ShortText::CommandContext c{actor, actor, context, &memory, 0.1}; std::string error;
        auto run = [&](const std::string &cmd, const ShortText::Value &args) { error.clear(); return ShortText::ExecuteSpatialCommand(cmd, args, c, error); };
        auto result = [&](const char *name) -> ShortText::Value { return memory.extensionValues["results"][name]; };

        Require(!run("spatial.unknown", ShortText::Value::object()) && error.empty(), "unknown spatial command must allow next adapter");
        Require(!run("look_at", {{"position", {0, 0, 0}}}) && error.empty(), "other domain command is not claimed");

        // 3-4-5 against a named reference, measured from a non-origin world source.
        Require(run("spatial.distance", {{"reference", "Goal"}, {"result", "goal3"}}), "named reference distance");
        auto r = result("goal3");
        Require(Near(r["distance"].get<double>(), 5.0), "3-4-5 distance");
        Require(NearArray(r["delta"], 3, 4, 0), "3-4-5 delta");
        Require(NearArray(r["direction"], 0.6, 0.8, 0), "3-4-5 direction");
        Require(r["coincident"] == false, "not coincident");
        Require(Near(r["yaw_degrees"].get<double>(), 90.0), "yaw atan2(x,z)");
        Require(Near(r["pitch_degrees"].get<double>(), std::atan2(4.0, 3.0) * 180.0 / pi), "pitch atan2(y,hypot(x,z))");
        Require(!r.contains("angle_degrees"), "3D result has no 2D angle");

        Require(run("spatial.direction", {{"position", {4, 2, 7}}}), "3-4-5 in XZ plane with default result");
        r = result("spatial");
        Require(Near(r["distance"].get<double>(), 5.0) && NearArray(r["direction"], 0.6, 0, 0.8), "XZ direction");
        Require(Near(r["yaw_degrees"].get<double>(), std::atan2(3.0, 4.0) * 180.0 / pi) && Near(r["pitch_degrees"].get<double>(), 0.0), "XZ yaw/pitch");

        // 2D projects onto XY and measures CCW from +X.
        Require(run("spatial.direction", {{"position", {4, 6, 100}}, {"dimension", 2}, {"result", "flat"}}), "2D ignores Z");
        r = result("flat");
        Require(Near(r["distance"].get<double>(), 5.0) && NearArray(r["delta"], 3, 4, 0) && NearArray(r["direction"], 0.6, 0.8, 0), "2D projected delta");
        Require(Near(r["angle_degrees"].get<double>(), std::atan2(4.0, 3.0) * 180.0 / pi), "2D angle");
        Require(!r.contains("yaw_degrees") && !r.contains("pitch_degrees"), "2D result has no yaw/pitch");
        Require(run("spatial.direction", {{"position", {0, 2, 0}}, {"dimension", 2}, {"result", "west"}}) && Near(result("west")["angle_degrees"].get<double>(), 180.0), "2D -X is 180");
        Require(run("spatial.direction", {{"position", {1, 1, 0}}, {"dimension", 2}, {"result", "south"}}) && Near(result("south")["angle_degrees"].get<double>(), -90.0), "2D -Y is -90");

        // Coincident positions yield zero direction and angles.
        Require(run("spatial.direction", {{"position", {1, 2, 3}}, {"result", "same"}}), "coincident 3D");
        r = result("same");
        Require(r["coincident"] == true && Near(r["distance"].get<double>(), 0.0) && NearArray(r["direction"], 0, 0, 0), "coincident zero direction");
        Require(r["yaw_degrees"].get<double>() == 0.0 && r["pitch_degrees"].get<double>() == 0.0, "coincident zero yaw/pitch");
        Require(run("spatial.direction", {{"position", {1, 2, 50}}, {"dimension", 2}, {"result", "same2"}}), "coincident 2D despite Z");
        Require(result("same2")["coincident"] == true && result("same2")["angle_degrees"].get<double>() == 0.0, "coincident zero angle");

        // UUID resolves even when the name is duplicated.
        Require(run("spatial.distance", {{"reference", twinA->GetObjectID().ToString()}, {"result", "twin"}}), "UUID reference");
        Require(Near(result("twin")["distance"].get<double>(), 10.0) && NearArray(result("twin")["direction"], 0, 0, 1), "UUID picks exact object");
        Require(run("spatial.distance", {{"position", {1, 2, 3}}, {"result", std::string(64, 'a')}}), "64-byte result name accepted");

        const Vector3 actorBefore = actorT->GetTranslate(), goalBefore = goalT->GetTranslate(), twinABefore = twinAT->GetTranslate(), twinBBefore = twinBT->GetTranslate();
        auto unchanged = [&] {
            return Same(actorT->GetTranslate(), actorBefore) && Same(goalT->GetTranslate(), goalBefore) && Same(twinAT->GetTranslate(), twinABefore) && Same(twinBT->GetTranslate(), twinBBefore);
        };
        Require(unchanged(), "successful queries do not move transforms");

        const auto before = memory.extensionValues;
        auto fails = [&](const ShortText::Value &args, const char *message) {
            Require(!run("spatial.direction", args), message);
            Require(!error.empty(), message);
            Require(memory.extensionValues == before, "failure must preserve previous results");
            Require(unchanged(), "failure must not move transforms");
        };
        fails({{"reference", "Twin"}}, "duplicate name rejected");
        fails({{"reference", "Nobody"}}, "missing object rejected");
        fails({{"reference", ""}}, "empty reference rejected");
        fails({{"reference", 5}}, "non-string reference rejected");
        fails({{"reference", "Goal"}, {"position", {0, 0, 0}}}, "both destinations rejected");
        fails(ShortText::Value::object(), "neither destination rejected");
        fails({{"result", "only"}}, "result alone is not a destination");
        fails(ShortText::Value::array(), "non-object arguments rejected");
        fails({{"positon", {0, 0, 0}}}, "unknown argument rejected");
        fails({{"position", {1, 2}}}, "2-element position rejected");
        fails({{"position", {1, 2, 3, 4}}}, "4-element position rejected");
        fails({{"position", 3}}, "scalar position rejected");
        fails({{"position", {1, "2", 3}}}, "string element rejected");
        fails({{"position", ShortText::Value::array({true, 0, 0})}}, "boolean element rejected");
        fails({{"position", ShortText::Value::array({nan, 0.0, 0.0})}}, "NaN rejected");
        fails({{"position", ShortText::Value::array({0.0, inf, 0.0})}}, "infinity rejected");
        fails({{"position", {0, 0, 1e8}}}, "out-of-bounds coordinate rejected");
        fails({{"position", {0, 0, 0}}, {"dimension", 4}}, "wrong dimension rejected");
        fails({{"position", {0, 0, 0}}, {"dimension", 1}}, "dimension 1 rejected");
        fails({{"position", {0, 0, 0}}, {"dimension", 2.5}}, "fractional dimension rejected");
        fails({{"position", {0, 0, 0}}, {"dimension", 2.0}}, "float dimension rejected");
        fails({{"position", {0, 0, 0}}, {"dimension", true}}, "boolean dimension rejected");
        fails({{"position", {0, 0, 0}}, {"dimension", "2"}}, "string dimension rejected");
        fails({{"position", {0, 0, 0}}, {"result", ""}}, "empty result name rejected");
        fails({{"position", {0, 0, 0}}, {"result", std::string(65, 'a')}}, "long result name rejected");
        fails({{"position", {0, 0, 0}}, {"result", "bad\nname"}}, "control character result name rejected");
        fails({{"position", {0, 0, 0}}, {"result", 7}}, "non-string result name rejected");

        {
            ShortText::CommandContext noMemory{actor, actor, context, nullptr, 0.1};
            error.clear(); Require(!ShortText::ExecuteSpatialCommand("spatial.distance", {{"position", {0, 0, 0}}}, noMemory, error) && !error.empty(), "memory required");
            ShortText::CommandContext noScene{actor, actor, nullptr, &memory, 0.1};
            error.clear(); Require(!ShortText::ExecuteSpatialCommand("spatial.distance", {{"position", {0, 0, 0}}}, noScene, error) && !error.empty(), "scene required");
            ShortText::CommandContext noTarget{actor, nullptr, context, &memory, 0.1};
            error.clear(); Require(!ShortText::ExecuteSpatialCommand("spatial.distance", {{"position", {0, 0, 0}}}, noTarget, error) && !error.empty(), "target required");
            Require(memory.extensionValues == before && unchanged(), "context failures preserve state");
        }
        {
            ShortText::ProgramMemory malformed; malformed.extensionValues["results"] = 5;
            ShortText::CommandContext m{actor, actor, context, &malformed, 0.1};
            error.clear(); Require(!ShortText::ExecuteSpatialCommand("spatial.distance", {{"position", {0, 0, 0}}}, m, error) && !error.empty(), "malformed results store rejected");
            Require(malformed.extensionValues["results"] == 5, "malformed results store left untouched");
            ShortText::ProgramMemory fresh; ShortText::CommandContext f{actor, actor, context, &fresh, 0.1};
            error.clear(); Require(ShortText::ExecuteSpatialCommand("spatial.distance", {{"position", {1, 2, 4}}}, f, error), "empty memory creates results store");
            Require(Near(fresh.extensionValues["results"]["spatial"]["distance"].get<double>(), 1.0), "fresh store result");
        }
        Require(run("spatial.distance", {{"position", {1, 2, 3}}, {"result", "goal3"}}) && result("goal3")["coincident"] == true, "success overwrites only the named result");
        Require(result("flat") == before.at("results").at("flat"), "other results kept on success");
        std::puts("Short-text spatial native regression passed");
    } catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); code = 1; }
    std::filesystem::current_path(original); std::filesystem::remove_all(temp); return code;
}
