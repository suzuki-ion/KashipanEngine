// Actual editor host, scene, script VM, assets and Undo. No window/GPU/real input.
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <imgui.h>
#include <json.hpp>
#include "Scene/Scene.h"
#include "Scene/SceneEditor.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/SceneFileIO.h"
#include "Scene/Editor/DevelopmentAssistantApi.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/Render/TilemapRenderer.h"
#include "Input/EditorTestInput.h"
#include "Utilities/DevelopmentAssistantTools.h"
#include "Utilities/DevelopmentAssistant.h"
#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
using namespace KashipanEngine;
using J = nlohmann::json;
namespace {
void Require(bool value, const std::string &message) { if (!value) throw std::runtime_error(message); }
std::string Read(const std::filesystem::path &path) { std::ifstream file(path, std::ios::binary); return {std::istreambuf_iterator<char>(file), {}}; }
}
int main() {
    const auto original = std::filesystem::current_path();
    const auto sandbox = original / "Generated" / ("EditorApiExtended-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(sandbox / "Assets"); std::filesystem::current_path(sandbox);
    ProjectPaths::SetActiveProject(std::bit_cast<Passkey<ProjectManager>>(std::uint8_t{}), PathToUtf8String(sandbox));
    ImGui::CreateContext(); int code = 0;
    try {
        Scene scene(std::string("API extended test"));
        SceneEditorContext context(std::bit_cast<Passkey<Scene>>(std::uint8_t{}), &scene);
        SceneEditor editor(std::bit_cast<Passkey<Scene>>(std::uint8_t{}), &context);
        auto raw = [&](const char *tool, J args) { return J::parse(editor.CallAIEditorApi(tool, args.dump())); };
        auto call = [&](const char *tool, J args) { auto result = raw(tool, args); Require(result.value("ok", false), std::string(tool) + " failed: " + result.dump()); return result; };
        auto undo = [&] { call("history", {{"action", "undo"}}); };
        auto redo = [&] { call("history", {{"action", "redo"}}); };
        auto component = [&](const std::string &id, const char *type) {
            const auto object = call("object", {{"id", id}});
            for (const auto &item : object.at("components")) if (item.at("type") == type) return item.at("component").get<size_t>();
            throw std::runtime_error(std::string("missing component ") + type);
        };
        Require(J::parse(EditorApiCatalog()).size() == 31, "31 tools");
        const auto parent = call("create_object", {{"name", "parent"}}).at("id").get<std::string>();
        const auto child = call("create_object", {{"name", "child"}}).at("id").get<std::string>();
        call("hierarchy", {{"id", child}, {"parent", parent}});
        Require(context.GetSceneObject(UUID128(child))->GetComponent<Transform>()->GetParentObject() == context.GetSceneObject(UUID128(parent)), "reparent");
        Require(!raw("hierarchy", {{"id", parent}, {"parent", child}}).at("ok"), "cycle rejected");
        undo(); Require(!context.GetSceneObject(UUID128(child))->GetComponent<Transform>()->GetParentObject(), "reparent Undo"); redo();
        const auto clone = call("duplicate", {{"id", parent}}).at("id").get<std::string>(); Require(clone != parent, "duplicate fresh ID"); undo();

        const auto transform = component(parent, "Transform");
        call("component_json", {{"action", "patch"}, {"id", parent}, {"component", transform}, {"patches", {{{"path", "/data/customData/rotate"}, {"value", {{"x", 0}, {"y", 0}, {"z", 0}, {"w", 1}}}}}}}); undo();
        const auto unchanged = context.SaveSceneToJSON().dump();
        Require(!raw("component_json", {{"action", "patch"}, {"id", parent}, {"component", transform}, {"patches", {{{"path", "/data/customData/translate"}, {"value", {{"x", 5}}}}}}}).at("ok"), "invalid vector rejected");
        Require(context.SaveSceneToJSON().dump() == unchanged, "native loader failure restored");
        auto failedBatch = raw("batch", {{"operations", {{{"tool", "set_property"}, {"arguments", {{"id", parent}, {"component", transform}, {"member", "translate_"}, {"value", {5, 6, 7}}}}},
            {{"tool", "set_property"}, {"arguments", {{"id", parent}, {"component", transform}, {"member", "missing"}, {"value", 1}}}}}}});
        Require(!failedBatch.at("ok") && context.SaveSceneToJSON().dump() == unchanged, "batch rollback");
        auto batch = call("batch", {{"operations", {{{"tool", "create_object"}, {"arguments", {{"name", "batched"}}}, {"as", "new"}},
            {{"tool", "set_property"}, {"arguments", {{"id", "$new"}, {"component", 0}, {"member", "translate_"}, {"value", {1, 2, 3}}}}}}}});
        const auto batchID = batch.at("results").at(0).at("id").get<std::string>(); undo(); Require(!context.GetSceneObject(UUID128(batchID)), "one Undo removes entire batch"); redo();
        auto aliasFailure = raw("batch", {{"operations", {{{"tool", "create_object"}, {"arguments", {{"name", "rollback-on-exception"}}}}, {{"tool", "scene"}, {"arguments", J::object()}, {"as", "invalid"}}}}});
        Require(!aliasFailure.at("ok") && !context.GetSceneObject("rollback-on-exception"), "batch exception rollback");

        call("selection", {{"action", "set"}, {"ids", {parent, child}}}); Require(call("selection", {{"action", "get"}}).at("ids").size() == 2, "multi-select");
        call("editor_window", {{"name", "assets"}, {"visible", true}});
        call("view", {{"action", "set"}, {"settings", {{"distance", 15}}}}); Require(call("view", {{"action", "get"}}).at("settings").at("distance") == 15, "view camera");
        Require(!raw("capture", J::object()).at("ok"), "no GPU capture honestly fails");
        call("logs", {{"limit", 10}});

        const auto sceneComponent = call("scene_components", {{"action", "add"}, {"type", "GameEventRouter"}}).at("component").get<size_t>();
        call("scene_components", {{"action", "patch"}, {"component", sceneComponent}, {"patches", {{{"path", "/data/isActive"}, {"value", false}}}}});
        Require(!call("scene_components", {{"action", "get"}, {"component", sceneComponent}}).at("state").at("data").at("isActive"), "scene component disabled");
        call("scene_components", {{"action", "remove"}, {"component", sceneComponent}}); undo(); undo();
        Require(call("scene_components", {{"action", "get"}, {"component", sceneComponent}}).at("state").at("data").at("isActive"), "scene component identity and Undo");
        call("scene_components", {{"action", "remove"}, {"component", sceneComponent}});
        const auto nextSceneComponent = call("scene_components", {{"action", "add"}, {"type", "GameEventRouter"}}).at("component").get<size_t>();
        Require(nextSceneComponent > sceneComponent, "scene component identities are monotonic"); undo(); undo();

        call("add_component", {{"id", parent}, {"type", "KeyFrameAnimator"}}); const auto animator = component(parent, "KeyFrameAnimator");
        call("keyframes", {{"action", "set"}, {"id", parent}, {"component", animator}, {"name", "test"}, {"animation", {{"keyframes", {{{"time", 0}, {"value", 0}, {"ease", "EaseInOutCubic"}}, {{"time", 1}, {"value", 4}}}}}}});
        Require(context.GetSceneObject(UUID128(parent))->GetComponent<KeyFrameAnimator>()->GetAnimationNames().size() == 1, "detailed keyframe edit"); undo();
        call("add_component", {{"id", parent}, {"type", "TilemapRenderer"}}); const auto tilemap = component(parent, "TilemapRenderer");
        call("set_component_active", {{"id", parent}, {"component", tilemap}, {"active", false}});
        call("tilemap", {{"action", "resize"}, {"id", parent}, {"component", tilemap}, {"width", 4}, {"height", 3}});
        Require(!raw("component_json", {{"action", "patch"}, {"id", parent}, {"component", tilemap}, {"patches", {{{"path", "/data/customData/gridWidth"}, {"value", -1}}}}}).at("ok"), "invalid generic Tilemap dimensions rejected before allocation");
        call("component_json", {{"action", "patch"}, {"id", parent}, {"component", tilemap}, {"patches", {{{"path", "/data/customData/tileTypes"}, {"value", {{{"connectsToTileTypes", {0}}, {"tilesetOriginPx", {{"x", 0}, {"y", 0}}}, {"isSolid", true}}}}}}}});
        call("tilemap", {{"action", "set_cells"}, {"id", parent}, {"component", tilemap}, {"cells", {{{"x", 2}, {"y", 1}, {"tile", 0}}}}});
        Require(context.GetSceneObject(UUID128(parent))->GetComponent<TilemapRenderer>()->GetTile(2, 1) == 0, "tile edit"); undo();
        Require(context.GetSceneObject(UUID128(parent))->GetComponent<TilemapRenderer>()->GetTile(2, 1) == -1, "tile Undo");

        call("asset", {{"action", "write"}, {"path", "Assets/test.txt"}, {"text", "first"}}); Require(Read("Assets/test.txt") == "first", "asset write");
        std::ofstream("Assets/test.txt") << "outside"; Require(!raw("history", {{"action", "undo"}}).at("ok"), "asset Undo does not overwrite external edits");
        std::ofstream("Assets/test.txt") << "first"; undo(); Require(!std::filesystem::exists("Assets/test.txt"), "asset Undo"); redo();
        call("asset", {{"action", "move"}, {"path", "Assets/test.txt"}, {"destination", "Assets/moved.txt"}}); Require(!std::filesystem::exists("Assets/test.txt") && Read("Assets/moved.txt") == "first", "asset move"); undo();
        Require(!raw("asset", {{"action", "write"}, {"path", "Assets/../../escape.as"}, {"text", "bad"}}).at("ok"), "path traversal rejected");
        Require(!raw("asset", {{"action", "write"}, {"path", "Assets/run.exe"}, {"text", "bad"}}).at("ok"), "executable blocked");
        call("asset", {{"action", "list"}});
        std::filesystem::create_directories("Incoming"); std::ofstream("Incoming/source.txt") << "imported";
        call("asset", {{"action", "import"}, {"path", "Assets/imported.txt"}, {"source", "Incoming/source.txt"}});
        Require(call("asset", {{"action", "read"}, {"path", "Assets/imported.txt"}}).at("text") == "imported", "asset import/read");
        call("asset", {{"action", "delete"}, {"path", "Assets/imported.txt"}}); undo(); Require(Read("Assets/imported.txt") == "imported", "asset delete Undo");
        call("prefab", {{"action", "save"}, {"id", parent}, {"path", "Assets/test.prefab"}});
        const auto instance = call("prefab", {{"action", "instantiate"}, {"path", "Assets/test.prefab"}}).at("id").get<std::string>();
        Require(instance != parent, "prefab fresh identity"); call("prefab", {{"action", "unpack"}, {"id", instance}}); undo();
        call("set_object", {{"id", instance}, {"name", "prefab-override"}});
        call("prefab", {{"action", "revert"}, {"id", instance}});
        Require(context.GetSceneObject(UUID128(instance))->GetName() != "prefab-override", "prefab revert");
        undo(); Require(context.GetSceneObject(UUID128(instance))->GetName() == "prefab-override", "prefab revert Undo");
        call("prefab", {{"action", "apply"}, {"id", instance}, {"overwrite", true}});
        Require(J::parse(Read("Assets/test.prefab")).at("name") == "prefab-override", "prefab asset apply");

        call("asset", {{"action", "write"}, {"path", "Assets/Echo.as"}, {"text", "class Echo : ScriptComponentBehavior { [SerializeField] float total = 2; void Bump(float amount) { total += amount; } }"}});
        const auto attached = call("attach_script", {{"id", child}, {"path", "Assets/Echo.as"}}); Require(attached.at("script_error").get<std::string>().empty(), "script compiled: " + attached.dump());
        const auto script = attached.at("component").get<size_t>();
        Require(!raw("component_json", {{"action", "patch"}, {"id", child}, {"component", script}, {"patches", {{{"path", "/data/customData/scriptPath"}, {"value", "../../outside.as"}}}}}).at("ok"), "generic script patch cannot escape Assets");
        call("script", {{"action", "set_field"}, {"id", child}, {"component", script}, {"field", "total"}, {"value", 3.0}});
        Require(call("script", {{"action", "get_field"}, {"id", child}, {"component", script}, {"field", "total"}}).at("value") == 3, "script field");
        call("script", {{"action", "reload"}, {"id", child}, {"component", script}});
        call("scene_files", {{"action", "save"}, {"path", "Assets/edit.json"}});
        call("play", {{"action", "start"}}); call("play", {{"action", "pause"}});
        Require(!raw("set_object", {{"id", parent}, {"name", "not-in-play"}}).at("ok"), "edit refused in Play");
        const auto runtimeScript = component(child, "ScriptComponent");
        call("script", {{"action", "call"}, {"id", child}, {"component", runtimeScript}, {"method", "Bump"}, {"args", {4.0}}});
        Require(call("script", {{"action", "get_field"}, {"id", child}, {"component", runtimeScript}, {"field", "total"}}).at("value") == 7, "script call with arguments");
        call("input", {{"device", "key"}, {"code", 32}, {"down", true}, {"frames", 2}});
        std::array<std::uint8_t, 256> keys{}; EditorTestInput::ApplyKeys(keys); Require(keys[32] == 0x80, "local key injection"); EditorTestInput::ApplyKeys(keys); keys.fill(0); EditorTestInput::ApplyKeys(keys); Require(keys[32] == 0, "input duration/release");
        call("input", {{"device", "pointer"}, {"x", 12}, {"y", 34}});
        call("input", {{"device", "wheel"}, {"delta", 120}});
        struct Point { int x, y; }; Point pointer{0, 0}, previous{2, 4}; int dx = 0, dy = 0, wheel = 0, total = 0;
        EditorTestInput::ApplyMotion(pointer, previous, dx, dy, wheel, total);
        Require(pointer.x == 12 && pointer.y == 34 && dx == 10 && dy == 30 && wheel == 120 && total == 120, "engine-local pointer and wheel");
        call("play", {{"action", "run_frames"}, {"frames", 2}}); Require(call("runtime_status", J::object()).at("pending_steps") == 2, "step job queued");
        call("scene_files", {{"action", "save"}, {"path", "Assets/playing.json"}});
        Require(Read("Assets/playing.json").find("editorAddedID") == std::string::npos, "transient IDs excluded from scene files");
        call("scene_files", {{"action", "save"}, {"path", "Assets/baked.json"}, {"runtime", true}});
        Require(!raw("scene_files", {{"action", "save"}, {"path", "Assets/edit.json"}}).at("ok"), "existing other scene requires overwrite");
        call("play", {{"action", "stop"}});
        Require(component(child, "ScriptComponent") == script, "component identity survives Play/Stop");
        Require(call("script", {{"action", "get_field"}, {"id", child}, {"component", script}, {"field", "total"}}).at("value") == 3, "Stop restores field state");
        call("scene_files", {{"action", "new"}, {"name", "New"}, {"discard_unsaved", true}}); Require(context.GetName() == "New", "new scene");
        call("scene_files", {{"action", "load"}, {"path", "Assets/edit.json"}, {"discard_unsaved", true}}); Require(context.GetSceneObject(UUID128(parent)), "scene load");
        call("history", {{"action", "status"}});

        if (std::getenv("KASHIPAN_EDITOR_API_EXTENDED_LIVE")) {
            auto tools = std::make_shared<DevelopmentAI::EditorTools>(EditorApiCatalog()); DevelopmentAI::Session session;
            session.Refresh("", ""); while (session.Get().busy) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            Require(session.Start(DevelopmentAI::Provider::Codex, DevelopmentAI::Mode::EditScene, "gpt-6-luna", sandbox,
                "Use ONLY kashipan_editor MCP. Call runtime_status. Use one batch to create an object named LIVE_EXTENDED with as=new and set its Transform translate_ (component 0) to [2,3,4] using id=$new. Select the created object with selection set. Create Assets/LiveEcho.as using asset write, text='class LiveEcho : ScriptComponentBehavior {}', then attach_script to the new object. Inspect object and report results. Do not run shell commands or change unrelated objects or files.", "auto", tools), "live start");
            while (session.Get().busy) { tools->Pump([&](const auto &tool, const auto &args) { const auto result = editor.CallAIEditorApi(tool, args); std::printf("LIVE %s: %s\n", tool.c_str(), J::parse(result).value("ok", false) ? "ok" : "error"); return result; }); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
            std::printf("%s\n%s\n%s\n", session.Get().status.c_str(), session.Get().output.c_str(), session.Get().diagnostics.c_str());
            auto *live = context.GetSceneObject("LIVE_EXTENDED"); Require(live && live->GetComponent<Transform>()->GetTranslate().y == 3 && live->GetComponent<ScriptComponent>(), "live batch, asset and script attachment"); tools->Stop();
        }
        std::puts("DevelopmentAssistantExtendedRegression passed: all 12 categories, headless host; rendered image unavailable without GPU.");
    } catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); code = 1; }
    ImGui::DestroyContext(); std::filesystem::current_path(original);
    // Keep the isolated fixture under Generated for diagnosis; never modify user scene files.
    return code;
}
