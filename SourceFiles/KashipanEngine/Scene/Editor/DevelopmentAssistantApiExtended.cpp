#include "Scene/Editor/DevelopmentAssistantApi.h"
#ifdef USE_IMGUI
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Scene/Editor/PrefabAssetManager.h"
#include "Scene/Editor/PrefabSyncUtility.h"
#include "Scene/SceneManager.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/PrefabInstanceComponent.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/Render/TilemapRenderer.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/ShortTextLearning.h"
#include "Input/EditorTestInput.h"
#include "Debug/Logger.h"
#include <json.hpp>
#include <fstream>
#include <set>
#include <cmath>
#include <algorithm>
#include <optional>

namespace KashipanEngine {
namespace {
using J = nlohmann::json;
using Execute = std::function<bool(std::unique_ptr<IEditorCommand>)>;
void Check(bool condition, const std::string &message) { if (!condition) throw std::runtime_error(message); }
std::string Text(const J &j, const char *key) {
    const auto text = j.at(key).get<std::string>();
    Check(!text.empty() && text.size() <= 4096 && text.find('\0') == std::string::npos, std::string("Invalid ") + key);
    return text;
}
J Field(const char *type) { return {{"type", type}}; }
J Tool(const char *name, const char *description, J properties, J required = J::array(), bool read = false) {
    return {{"name", name}, {"description", description}, {"inputSchema", {{"type", "object"}, {"properties", properties},
        {"required", required}, {"additionalProperties", false}}}, {"annotations", {{"readOnlyHint", read}, {"openWorldHint", false}}}};
}
void Bounded(const J &value, unsigned depth = 0) {
    Check(depth <= 20 && value.size() <= 4096, "JSON structure exceeds limits");
    if (value.is_string()) Check(value.get_ref<const std::string &>().size() <= 65536, "String too long");
    if (value.is_number()) Check(std::isfinite(value.get<double>()) && std::abs(value.get<double>()) <= 1e9, "Invalid numeric value");
    if (value.is_structured()) for (const auto &child : value) Bounded(child, depth + 1);
}
// Only existing fields can be patched. Array/map replacement is deliberate, bounded and
// passed through the native component loader, whose failure restores the previous state.
J Patched(J state, const J &patches, const std::string &prefix) {
    Check(patches.is_array() && patches.size() <= 128, "patches must be an array of at most 128 path/value entries");
    for (const auto &patch : patches) {
        const auto path = Text(patch, "path");
        Check(path.starts_with(prefix), "Path is outside editable component data");
        const J::json_pointer pointer(path);
        Check(state.contains(pointer), "Unknown serialized path: " + path);
        const auto &value = patch.at("value"); Bounded(value);
        const auto &old = state.at(pointer);
        Check(old.is_null() || old.type() == value.type() || (old.is_number() && value.is_number()), "Serialized field type mismatch");
        state[pointer] = value;
    }
    return state;
}
EmptyObject *Object(SceneEditorContext *context, const J &args) {
    const UUID128 id(Text(args, "id")); Check(id.IsValid(), "Invalid object UUID");
    auto *object = context->GetSceneObject(id); Check(object, "Object no longer exists"); return object;
}
IObjectComponent *Component(SceneEditorContext *context, EmptyObject *object, const J &args) {
    Check(args.at("component").is_number_integer() && args.at("component").get<double>() >= 0, "Invalid component ID");
    auto *component = context->ResolveComponent({object->GetObjectID(), args.at("component").get<size_t>()});
    Check(component, "Component no longer exists"); return component;
}
std::string ReadFile(const std::filesystem::path &path, size_t limit = 16 * 1024 * 1024) {
    Check(std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) <= limit, "File missing or too large");
    std::ifstream input(path, std::ios::binary); Check(static_cast<bool>(input), "Cannot read file");
    return {std::istreambuf_iterator<char>(input), {}};
}
bool WriteFileAtomic(const std::filesystem::path &path, const std::string &data) {
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path; temporary += L".ai-" + Utf8StringToPath(UUID128(true).ToString()).wstring();
    { std::ofstream stream(temporary, std::ios::binary | std::ios::trunc); stream.write(data.data(), static_cast<std::streamsize>(data.size()));
      if (!stream) { std::filesystem::remove(temporary); return false; } }
    if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    std::filesystem::remove(temporary); return false;
}
class FileCommand final : public IEditorCommand {
public:
    FileCommand(std::filesystem::path path, std::optional<std::string> after) : path_(std::move(path)), after_(std::move(after)) {
        if (std::filesystem::exists(path_)) before_ = ReadFile(path_);
    }
    bool Execute(SceneEditorContext *) override { return Change(before_, after_); }
    bool Undo(SceneEditorContext *) override { return Change(after_, before_); }
    std::string GetName() const override { return "AI asset: " + PathToUtf8String(path_.filename()); }
private:
    bool Change(const std::optional<std::string> &expected, const std::optional<std::string> &next) {
        try {
            const bool exists = std::filesystem::exists(path_);
            if (exists != expected.has_value() || (exists && ReadFile(path_) != *expected)) return false;
            if (next) return WriteFileAtomic(path_, *next);
            return !exists || std::filesystem::remove(path_);
        } catch (...) { return false; }
    }
    std::filesystem::path path_;
    std::optional<std::string> before_, after_;
};
class ParentCommand final : public IEditorCommand {
public:
    ParentCommand(EmptyObject *object, EmptyObject *parent) : id_(object->GetObjectID()), after_(parent ? parent->GetObjectID() : UUID128{}) {
        auto *current = object->GetComponent<Transform>()->GetParentObject(); before_ = current ? current->GetObjectID() : UUID128{};
    }
    bool Execute(SceneEditorContext *context) override { return Apply(context, after_); }
    bool Undo(SceneEditorContext *context) override { return Apply(context, before_); }
    std::string GetName() const override { return "AI reparent"; }
private:
    bool Apply(SceneEditorContext *context, const UUID128 &parentID) {
        auto *object = context->GetSceneObject(id_); auto *parent = parentID.IsValid() ? context->GetSceneObject(parentID) : nullptr;
        if (!object || (parentID.IsValid() && !parent)) return false;
        std::set<UUID128, std::function<bool(const UUID128 &, const UUID128 &)>> visited([](const auto &a, const auto &b) { return a.ToString() < b.ToString(); });
        for (auto *cursor = parent; cursor; cursor = cursor->GetComponent<Transform>()->GetParentObject()) {
            if (cursor == object || !visited.insert(cursor->GetObjectID()).second) return false;
        }
        object->GetComponent<Transform>()->SetParentObject(parent); return true;
    }
    UUID128 id_, before_, after_;
};
class SceneComponentCommand final : public IEditorCommand {
public:
    SceneComponentCommand(size_t id, J before, J after) : id_(id), before_(std::move(before)), after_(std::move(after)) {}
    bool Execute(SceneEditorContext *context) override { return Apply(context, after_); }
    bool Undo(SceneEditorContext *context) override { return Apply(context, before_); }
    std::string GetName() const override { return "AI scene component"; }
private:
    bool Apply(SceneEditorContext *context, const J &state) {
        const auto old = context->SaveSceneComponent(id_);
        if (state.is_null()) {
            for (const auto &[component, id] : context->GetAllComponents()) if (component && id == id_) return context->RemoveComponent(component.get());
            return false;
        }
        if (old.is_null()) return context->RestoreSceneComponent(id_, state);
        try { if (context->LoadSceneComponent(id_, state)) return true; } catch (...) { }
        bool restored = false; try { restored = context->LoadSceneComponent(id_, old); } catch (...) { }
        SetAppliedChangesAfterFailure(!restored); return false;
    }
    size_t id_;
    J before_, after_;
};
class ScriptFieldCommand final : public IEditorCommand {
public:
    ScriptFieldCommand(ScriptComponent *script, std::string name, J before, J after)
        : ref_(script->GetComponentRef()), name_(std::move(name)), before_(std::move(before)), after_(std::move(after)) {}
    bool Execute(SceneEditorContext *context) override { return Apply(context, after_); }
    bool Undo(SceneEditorContext *context) override { return Apply(context, before_); }
    std::string GetName() const override { return "AI script field: " + name_; }
private:
    bool Apply(SceneEditorContext *context, const J &value) { auto *script = dynamic_cast<ScriptComponent *>(context->ResolveComponent(ref_)); return script && script->SetSerializedVariable(name_, value); }
    ComponentRef ref_; std::string name_; J before_, after_;
};
J Substitute(J value, const J &aliases) {
    if (value.is_string()) {
        const auto text = value.get<std::string>();
        if (text.starts_with("$") && aliases.contains(text.substr(1))) return aliases.at(text.substr(1));
    } else if (value.is_structured()) for (auto &child : value) child = Substitute(std::move(child), aliases);
    return value;
}
class BatchCommand final : public IEditorCommand {
public:
    BatchCommand(J operations, UUID128 selected, std::shared_ptr<J> result) : operations_(std::move(operations)), selected_(selected), result_(std::move(result)) {}
    bool Execute(SceneEditorContext *context) override {
        if (built_) {
            for (size_t i = 0; i < commands_.size(); ++i) { applied_[i] = commands_[i]->Execute(context); if (!applied_[i]) { applied_[i] = commands_[i]->HasAppliedChanges(); Undo(context); return false; } }
            return true;
        }
        J aliases = J::object(); *result_ = J::array();
        try {
        for (const auto &operation : operations_) {
            auto result = J::parse(ExecuteEditorApi(context, selected_, [&](auto command) {
                const bool success = command->Execute(context);
                if (success || command->HasAppliedChanges()) { commands_.push_back(std::move(command)); applied_.push_back(true); }
                return success;
            }, operation.at("tool").get<std::string>(), Substitute(operation.at("arguments"), aliases).dump()));
            result_->push_back(result);
            if (!result.value("ok", false)) { Undo(context); return false; }
            if (operation.contains("as")) { Check(result.contains("id"), "Alias requires an object-producing operation"); aliases[Text(operation, "as")] = result.at("id"); }
        }
        } catch (const std::exception &e) { result_->push_back({{"ok", false}, {"error", e.what()}}); Undo(context); return false; }
        built_ = true; return true;
    }
    bool Undo(SceneEditorContext *context) override {
        for (size_t i = commands_.size(); i > 0; --i) if (applied_[i - 1]) { if (!commands_[i - 1]->Undo(context)) return false; applied_[i - 1] = false; }
        return true;
    }
    bool HasAppliedChanges() const noexcept override { return std::find(applied_.begin(), applied_.end(), true) != applied_.end(); }
    std::string GetName() const override { return "AI batch"; }
private:
    J operations_; UUID128 selected_; std::shared_ptr<J> result_; bool built_ = false;
    std::vector<std::unique_ptr<IEditorCommand>> commands_; std::vector<bool> applied_;
};
}
std::string EditorApiProjectPath(const std::string &relative, bool assetsOnly) {
    Check(!relative.empty() && relative.size() <= 2048 && relative.find('\0') == std::string::npos && relative.find(':') == std::string::npos, "Invalid project-relative path");
    const auto path = Utf8StringToPath(relative); Check(!path.is_absolute() && !path.has_root_name(), "Absolute paths are not allowed");
    for (const auto &part : path) Check(part != L".." && !part.native().starts_with(L"."), "Hidden/parent paths are not allowed");
    const auto project = std::filesystem::weakly_canonical(Utf8StringToPath(ProjectPaths::ProjectRoot().empty() ? PathToUtf8String(std::filesystem::current_path()) : ProjectPaths::ProjectRoot()));
    const auto root = assetsOnly ? project / L"Assets" : project;
    const auto resolved = std::filesystem::weakly_canonical(project / path);
    const auto projectRelative = resolved.lexically_relative(project);
    Check(!projectRelative.empty() && *projectRelative.begin() != L"..", "Path escapes project through a link");
    const auto within = resolved.lexically_relative(std::filesystem::weakly_canonical(root));
    Check(!within.empty() && within != L"." && *within.begin() != L"..", "Path escapes the project Assets directory");
    return PathToUtf8String(resolved);
}
std::string ExtendedEditorApiCatalog() {
    const auto text = Field("string"), boolean = Field("boolean"), integer = Field("integer");
    const auto any = J::object(); const J patches{{"type", "array"}, {"items", {{"type", "object"}}}};
    return J::array({
        Tool("scene_files", "Scene management: action=list/new/load/save/switch. new/load/switch require discard_unsaved=true or an already saved scene. save path is project Assets/*.json or *.scene; overwriting a file other than the current scene requires overwrite=true. A switch is queued; the editor reconnects this request to the destination scene. Retry scene after the transition and inspect fresh IDs.", {{"action", text}, {"name", text}, {"path", text}, {"discard_unsaved", boolean}, {"runtime", boolean}, {"overwrite", boolean}}, {"action"}),
        Tool("play", "action=status/start/stop/pause/resume/step/run_frames. run_frames queues 1..600 paused simulation steps; poll runtime_status for completion. Stop restores edit state. Runtime actions are not Undo edits.", {{"action", text}, {"frames", integer}}, {"action"}),
        Tool("selection", "action=get/set/clear. Set exact object UUIDs in ids (maximum 200). UI state, not an Undo edit.", {{"action", text}, {"ids", {{"type", "array"}, {"items", text}}}}, {"action"}),
        Tool("view", "action=get/set/focus. set settings is a patch object from get; focus accepts object id. Operates the editor camera, not a game camera; angular settings are radians.", {{"action", text}, {"id", text}, {"settings", {{"type", "object"}}}}, {"action"}),
        Tool("editor_window", "Show/hide a named editor panel: scene,hierarchy,inspector,scene_components,assets,history,logger,scene_list,input. Not operating-system UI automation.", {{"name", text}, {"visible", boolean}}, {"name", "visible"}),
        Tool("hierarchy", "Undoable reparent/reorder: id, optional parent (null detaches), optional index (scene object ordering). Local Transform values are preserved. Cycles are rejected.", {{"id", text}, {"parent", any}, {"index", integer}}, {"id"}),
        Tool("duplicate", "Clone an object and descendants with fresh UUIDs and remapped internal references; optional parent. Returns new root id. Undoable.", {{"id", text}, {"parent", text}}, {"id"}),
        Tool("prefab", "action=save/instantiate/apply/revert/unpack. save uses id+path, instantiate uses path+optional parent. apply/revert/unpack use id. save/apply change asset files and are NOT Undoable; use overwrite=true to overwrite an existing asset.", {{"action", text}, {"id", text}, {"path", text}, {"parent", text}, {"overwrite", boolean}}, {"action"}),
        Tool("scene_components", "action=list/get/add/patch/remove. list returns registered types and stable component IDs. add uses type; get/patch/remove use component. patch uses existing JSON pointer paths under /data/. Edits are Undoable.", {{"action", text}, {"component", integer}, {"type", text}, {"patches", patches}}, {"action"}),
        Tool("component_json", "Read or patch native serialized object-component data, including arrays/maps/enums/references/matrices and other complex fields. action=get/patch, id+component. Patch entries {path,value} must refer to existing paths under /data/customData/. Inspect first; native loader validates and failed edits roll back. Undoable.", {{"action", text}, {"id", text}, {"component", integer}, {"patches", patches}}, {"action", "id", "component"}),
        Tool("script", "action=inspect/get_field/set_field/reload/call. id+component must identify ScriptComponent. Fields use field+value. call uses method+args and requires Play; arbitrary runtime effects are not Undoable. reload returns compiler/runtime error. Complex serialized fields can also use component_json.", {{"action", text}, {"id", text}, {"component", integer}, {"field", text}, {"value", any}, {"method", text}, {"args", {{"type", "array"}, {"items", any}}}}, {"action", "id", "component"}),
        Tool("asset", "Project-scoped asset management: action=list/read/write/import/move/delete/reload. path is Assets-relative-to-project (Assets/...). write uses UTF-8 text; import uses source within project; move uses destination. overwrite=true required for replacement. File edits are Undoable with conflict checks; reload is explicit. No executable/shell files or external paths.", {{"action", text}, {"path", text}, {"text", text}, {"source", text}, {"destination", text}, {"overwrite", boolean}, {"offset", integer}, {"limit", integer}}, {"action"}),
        Tool("keyframes", "KeyFrameAnimator detailed editing: action=get/set/remove. id+component and name. set uses animation (KeyframeAnimation JSON), optional bindings array, loop and play_on_start. Validated and Undoable.", {{"action", text}, {"id", text}, {"component", integer}, {"name", text}, {"animation", any}, {"bindings", {{"type", "array"}, {"items", {{"type", "object"}}}}}, {"loop", boolean}, {"play_on_start", boolean}}, {"action", "id", "component"}),
        Tool("tilemap", "TilemapRenderer detailed editing: action=get/resize/set_cells/clear. set_cells uses array {x,y,tile}; resize uses width,height (up to 512 each). Updates are validated, batched and Undoable; tile=-1 clears a cell. Tile type definitions use component_json.", {{"action", text}, {"id", text}, {"component", integer}, {"width", integer}, {"height", integer}, {"cells", {{"type", "array"}, {"items", {{"type", "object"}}}}}}, {"action", "id", "component"}),
        Tool("capture", "Capture the last rendered scene-view or ScreenBufferObject as PNG and return an MCP image. Optional id+component select a game ScreenBuffer; otherwise editor scene view. Optional path under Assets. Requires an initialized graphics device and an already rendered frame.", {{"path", text}, {"id", text}, {"component", integer}}, J::array(), true),
        Tool("logs", "Read recent engine/application logs (limit up to 200), including script errors. No file-system log scraping.", {{"limit", integer}}, J::array(), true),
        Tool("runtime_status", "Read play/pause state, editor frame counter, pending test steps and selection. Use repeated status calls to observe queued run_frames completion.", J::object(), J::array(), true),
        Tool("input", "Inject engine-local keyboard or mouse button input during Play. device=key/mouse, code is Key enum integer or mouse button 0..7; down bool, frames 1..3600. device=pointer uses screen-space x,y; device=wheel uses delta (Windows wheel units). device=clear releases synthetic state. Never sends OS input. A paused scene queues one simulation step.", {{"device", text}, {"code", integer}, {"down", boolean}, {"frames", integer}, {"x", integer}, {"y", integer}, {"delta", integer}}, {"device"}),
        Tool("history", "action=status/undo/redo. Operates the editor Undo history. Cannot cross an active Play session.", {{"action", text}}, {"action"}),
        Tool("batch", "Execute up to 64 scene edit operations as ONE Undo command. Failure rolls back previous operations. operations=[{tool,arguments,as?}]. An object-producing operation may have as=name; later exact strings $name resolve to its id. Excludes files/lifecycle/runtime/history/nested batch and arbitrary script calls.", {{"operations", {{"type", "array"}, {"items", {{"type", "object"}}}}}}, {"operations"})
    }).dump();
}
std::string ExecuteExtendedEditorApi(SceneEditorContext *context, const UUID128 &selected, const Execute &execute,
    const std::string &tool, const std::string &arguments, EditorApiServices *services) {
    const auto args = J::parse(arguments); J result{{"ok", true}};
    auto run = [&](std::unique_ptr<IEditorCommand> command) { Check(command && execute && execute(std::move(command)), "Editor command failed; inspect Undo history if restoration failed"); result["undoable"] = true; };
    auto stopped = [&] { Check(!context->IsPlaying(), "Stop Play before editing"); };
    auto host = [&]() { Check(services && services->editor, "This operation requires the live editor host"); return services->editor(tool, arguments); };
    if (tool == "scene_files" || tool == "play" || tool == "selection" || tool == "view" || tool == "editor_window" || tool == "runtime_status" || tool == "capture") return host();
    if (tool == "logs") {
        const int limit = args.value("limit", 100); Check(limit >= 1 && limit <= 200, "Invalid log limit"); result["lines"] = J::array();
        size_t bytes = 0; for (const auto &line : GetRecentLogLines(static_cast<size_t>(limit))) { if ((bytes += line.text.size()) > 64000) break; result["lines"].push_back({{"text", line.text}, {"severity", static_cast<int>(line.severity)}}); }
    } else if (tool == "input") {
        Check(context->IsPlaying(), "Synthetic input requires Play"); const auto device = Text(args, "device");
        const int duration = args.value("frames", 1);
        Check(duration >= 1 && duration <= 3600 && (!context->IsPaused() || duration <= 600), "Invalid input duration (paused tests allow at most 600 frames)");
        if (device == "clear") EditorTestInput::Clear();
        else if (device == "pointer") EditorTestInput::Pointer(args.at("x").get<int>(), args.at("y").get<int>(), static_cast<unsigned>(args.value("frames", 1)));
        else if (device == "wheel") EditorTestInput::Wheel(args.at("delta").get<int>());
        else { const int code = args.at("code").get<int>(), frames = args.value("frames", 1); Check(code >= 0 && frames >= 1 && frames <= 3600, "Invalid input arguments");
            if (device == "key") EditorTestInput::Key(static_cast<unsigned>(code), args.at("down").get<bool>(), static_cast<unsigned>(frames));
            else if (device == "mouse") EditorTestInput::Button(static_cast<unsigned>(code), args.at("down").get<bool>(), static_cast<unsigned>(frames));
            else throw std::runtime_error("Unknown input device");
        }
        if (device != "clear" && context->IsPaused()) {
            if (services && services->editor) {
                const auto queued = J::parse(services->editor("play", J{{"action", "run_frames"}, {"frames", duration}}.dump()));
                Check(queued.value("ok", false), "Unable to queue input test frames");
            } else context->RequestStepFrame();
        }
        result["queued"] = true; result["undoable"] = false;
    } else if (tool == "history") {
        Check(services && services->commands, "Undo history unavailable"); stopped(); auto *commands = services->commands;
        const auto action = Text(args, "action");
        if (action == "undo") Check(commands->Undo(), "Nothing to undo or Undo failed");
        else if (action == "redo") Check(commands->Redo(), "Nothing to redo or Redo failed");
        else Check(action == "status", "Unknown history action");
        result["can_undo"] = commands->CanUndo(); result["can_redo"] = commands->CanRedo(); result["undo_name"] = commands->GetUndoName(); result["redo_name"] = commands->GetRedoName();
    } else if (tool == "batch") {
        stopped(); const auto &operations = args.at("operations"); Check(operations.is_array() && !operations.empty() && operations.size() <= 64, "Invalid batch size");
        const std::set<std::string> allowed{"scene", "object", "create_object", "delete_object", "set_object", "add_component", "remove_component", "set_component_active", "set_property", "apply_sentence", "attach_script", "hierarchy", "duplicate", "component_json", "scene_components", "keyframes", "tilemap"};
        for (const auto &op : operations) {
            Check(op.is_object() && allowed.contains(Text(op, "tool")) && op.at("arguments").is_object(), "Operation is not allowed in a batch");
            if (op.contains("as")) Check(op.at("as").is_string() && op.at("as").get<std::string>().size() <= 64, "Invalid batch alias");
        }
        auto results = std::make_shared<J>(); auto command = std::make_unique<BatchCommand>(operations, selected, results);
        const bool success = execute && execute(std::move(command)); result["ok"] = success; result["results"] = *results;
        if (!success) result["error"] = "Batch failed; successfully applied edits were rolled back unless an Undo failure is reported in history";
        result["undoable"] = true;
    } else if (tool == "hierarchy") {
        stopped(); auto *object = Object(context, args); auto group = std::make_unique<CompositeCommand>("AI hierarchy");
        if (args.contains("parent")) { EmptyObject *parent = nullptr; if (!args.at("parent").is_null()) { J parentArg{{"id", args.at("parent")}}; parent = Object(context, parentArg); }
            group->AddCommand(std::make_unique<ParentCommand>(object, parent)); }
        if (args.contains("index")) { const int index = args.at("index").get<int>(); Check(index >= 0 && static_cast<size_t>(index) < context->GetSceneObjects().size(), "Invalid ordering index"); group->AddCommand(std::make_unique<MoveObjectCommand>(object, static_cast<size_t>(index))); }
        Check(args.contains("parent") || args.contains("index"), "Specify parent or index"); run(std::move(group));
    } else if (tool == "duplicate") {
        stopped(); auto *object = Object(context, args); std::vector<PasteObjectCommand::Node> source;
        PrefabUtility::CollectSubtreeNodes(context, object, -1, source); Check(!source.empty() && source.size() <= 1024, "Invalid duplicate subtree");
        auto nodes = PrefabUtility::PrepareNodesForInstantiation(source, !args.contains("parent")); Check(!nodes.empty(), "Cannot clone subtree");
        result["id"] = nodes.front().json.at("objectID"); EmptyObject *parent = nullptr;
        if (args.contains("parent")) parent = Object(context, J{{"id", args.at("parent")}});
        run(std::make_unique<PasteObjectCommand>(std::move(nodes), parent, MAXSIZE_T, object->GetName(), "AI duplicate", !args.contains("parent")));
    } else if (tool == "scene_components") {
        const auto action = Text(args, "action");
        if (action == "list") { result["types"] = GetRegisteredSceneComponentTypes(); result["components"] = J::array();
            for (const auto &[component, id] : context->GetAllComponents()) if (component) result["components"].push_back({{"component", id}, {"type", component->GetComponentType()}, {"active", component->IsActive()}});
        } else if (action == "add") {
            stopped(); const auto type = Text(args, "type"); const auto &types = GetRegisteredSceneComponentTypes();
            Check(std::find(types.begin(), types.end(), type) != types.end(), "Unknown scene component type");
            const size_t id = context->GetNextSceneComponentID();
            Check(id < MAXSIZE_T - 1, "Scene component identity exhausted");
            result["component"] = id; run(std::make_unique<SceneComponentCommand>(id, J(), J{{"type", type}, {"data", {{"priority", 1}, {"isActive", true}, {"tag", ""}, {"data", J::object()}}}}));
        } else {
            const auto id = args.at("component").get<size_t>(); const auto before = context->SaveSceneComponent(id); Check(!before.is_null(), "Scene component no longer exists");
            if (action == "get") result["state"] = before;
            else { stopped(); Check(action == "patch" || action == "remove", "Unknown scene component action"); run(std::make_unique<SceneComponentCommand>(id, before, action == "remove" ? J() : Patched(before, args.at("patches"), "/data/"))); }
        }
    } else if (tool == "component_json") {
        auto *object = Object(context, args); auto *component = Component(context, object, args); const auto before = object->SaveComponentToJson(component);
        const auto action = Text(args, "action"); if (action == "get") result["state"] = before;
        else {
            stopped(); Check(action == "patch", "Unknown component action");
            auto after = Patched(before, args.at("patches"), "/data/customData/");
            const auto &data = after.at("data").at("customData");
            if (dynamic_cast<TilemapRenderer *>(component)) {
                const int width = data.at("gridWidth").get<int>(), height = data.at("gridHeight").get<int>();
                Check(width >= 0 && height >= 0 && width <= 512 && height <= 512, "Tilemap size exceeds 512x512");
            }
            if (dynamic_cast<ScriptComponent *>(component)) {
                const auto path = data.value("scriptPath", std::string{});
                if (!path.empty()) {
                    const auto resolved = Utf8StringToPath(EditorApiProjectPath(path));
                    Check(resolved.extension() == ".as" && std::filesystem::is_regular_file(resolved), "Expected an existing Assets/*.as script");
                }
            }
            run(std::make_unique<ComponentEditCommand>(object, component, before, after));
        }
    } else if (tool == "script") {
        auto *object = Object(context, args); auto *script = dynamic_cast<ScriptComponent *>(Component(context, object, args)); Check(script, "Expected ScriptComponent");
        const auto action = Text(args, "action");
        if (action == "inspect") result["state"] = object->SaveComponentToJson(script);
        else if (action == "get_field" || action == "set_field") {
            const auto field = Text(args, "field"); J value; Check(script->GetSerializedVariable(field, value), "Unknown or unsupported serialized field");
            if (action == "get_field") result["value"] = value;
            else { stopped(); Bounded(args.at("value")); run(std::make_unique<ScriptFieldCommand>(script, field, value, args.at("value"))); }
        } else if (action == "reload") { stopped(); script->ReloadFromDisk(); result["ok"] = script->GetLastError().empty(); result["undoable"] = false; }
        else if (action == "call") {
            Check(context->IsPlaying(), "Call script methods in Play; Stop restores the pre-Play scene");
            const auto values = args.value("args", J::array()); Check(values.is_array() && values.size() <= 16, "Invalid method arguments"); Bounded(values);
            result["ok"] = script->InvokeMethodWithValues(Text(args, "method"), values.get<std::vector<J>>()); result["undoable"] = false;
        } else throw std::runtime_error("Unknown script action");
        result["script_error"] = script->GetLastError(); if (!result.at("ok").get<bool>()) result["error"] = script->GetLastError();
    } else if (tool == "keyframes") {
        auto *object = Object(context, args); auto *animator = dynamic_cast<KeyFrameAnimator *>(Component(context, object, args)); Check(animator, "Expected KeyFrameAnimator");
        const auto action = Text(args, "action"); const auto before = object->SaveComponentToJson(animator); auto after = before;
        if (action == "get") result["state"] = before;
        else { stopped(); const auto name = Text(args, "name"); auto &entries = after["data"]["customData"]["animations"]; if (!entries.is_array()) entries = J::array();
            auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto &item) { return item.value("name", std::string{}) == name; });
            if (action == "remove") { Check(entry != entries.end(), "Unknown animation name"); entries.erase(entry); }
            else { Check(action == "set", "Unknown keyframe action"); KeyframeAnimation animation; Check(animation.LoadFromJson(args.at("animation")), "Invalid keyframes/easing");
                J value = entry == entries.end() ? J{{"name", name}, {"bindings", J::array()}} : *entry;
                value["embeddedAnimation"] = animation.SaveToJson(); value["jsonPath"] = "";
                if (args.contains("bindings")) { Check(args.at("bindings").is_array() && args.at("bindings").size() <= 128, "Invalid bindings"); Bounded(args.at("bindings")); value["bindings"] = args.at("bindings"); }
                value["loop"] = args.value("loop", false); value["playOnStart"] = args.value("play_on_start", false);
                if (entry == entries.end()) entries.push_back(value); else *entry = value;
            }
            run(std::make_unique<ComponentEditCommand>(object, animator, before, after));
        }
    } else if (tool == "tilemap") {
        auto *object = Object(context, args); auto *tilemap = dynamic_cast<TilemapRenderer *>(Component(context, object, args)); Check(tilemap, "Expected TilemapRenderer");
        const auto action = Text(args, "action"); const auto before = object->SaveComponentToJson(tilemap);
        if (action == "get") { result["state"] = before; result["width"] = tilemap->GetGridWidth(); result["height"] = tilemap->GetGridHeight(); }
        else { stopped();
            auto after = before; auto &data = after["data"]["customData"];
            int width = tilemap->GetGridWidth(), height = tilemap->GetGridHeight();
            std::vector<int> cells(static_cast<size_t>(width) * height, -1);
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) cells[static_cast<size_t>(y) * width + x] = tilemap->GetTile(x, y);
            if (action == "resize") {
                const int newWidth = args.at("width").get<int>(), newHeight = args.at("height").get<int>(); Check(newWidth >= 0 && newHeight >= 0 && newWidth <= 512 && newHeight <= 512, "Tilemap size exceeds 512x512");
                std::vector<int> next(static_cast<size_t>(newWidth) * newHeight, -1);
                for (int y = 0; y < (std::min)(height, newHeight); ++y) for (int x = 0; x < (std::min)(width, newWidth); ++x) next[static_cast<size_t>(y) * newWidth + x] = cells[static_cast<size_t>(y) * width + x];
                width = newWidth; height = newHeight; cells = std::move(next);
            } else if (action == "clear") std::fill(cells.begin(), cells.end(), -1);
            else { Check(action == "set_cells", "Unknown tilemap action"); const auto &updates = args.at("cells"); Check(updates.is_array() && updates.size() <= 4096, "Too many cells");
                for (const auto &cell : updates) { const int x = cell.at("x").get<int>(), y = cell.at("y").get<int>(), tile = cell.at("tile").get<int>();
                    Check(x >= 0 && y >= 0 && x < width && y < height && tile >= -1 && tile < tilemap->GetTileTypeCount(), "Invalid tile coordinate/type"); cells[static_cast<size_t>(y) * width + x] = tile; }
            }
            data["gridWidth"] = width; data["gridHeight"] = height; data["cells"] = cells;
            run(std::make_unique<ComponentEditCommand>(object, tilemap, before, after));
        }
    } else if (tool == "asset") {
        const auto action = Text(args, "action");
        if (action == "list") {
            const auto requested = args.value("path", std::string("Assets"));
            const auto root = Utf8StringToPath(EditorApiProjectPath(requested == "Assets" ? "Assets/_" : requested));
            auto directory = requested == "Assets" ? root.parent_path() : root; Check(std::filesystem::is_directory(directory), "Asset directory not found");
            std::vector<std::string> paths;
            for (auto iterator = std::filesystem::recursive_directory_iterator(directory, std::filesystem::directory_options::skip_permission_denied); iterator != std::filesystem::recursive_directory_iterator{}; ++iterator) {
                if (iterator->is_symlink()) { iterator.disable_recursion_pending(); continue; }
                const auto relative = PathToUtf8String(iterator->path().lexically_relative(Utf8StringToPath(ProjectPaths::ProjectRoot().empty() ? PathToUtf8String(std::filesystem::current_path()) : ProjectPaths::ProjectRoot())));
                try { EditorApiProjectPath(relative); } catch (...) { iterator.disable_recursion_pending(); continue; }
                if (iterator->is_regular_file()) paths.push_back(relative);
                if (paths.size() >= 4096) break;
            }
            std::sort(paths.begin(), paths.end()); const int offset = args.value("offset", 0), limit = args.value("limit", 100); Check(offset >= 0 && limit >= 1 && limit <= 200, "Invalid asset pagination");
            result["paths"] = J::array(); for (size_t i = static_cast<size_t>(offset); i < paths.size() && i < static_cast<size_t>(offset) + limit; ++i) result["paths"].push_back(paths[i]); result["total"] = paths.size();
        } else if (action == "reload") return host();
        else {
            const auto path = Utf8StringToPath(EditorApiProjectPath(Text(args, "path"))); const auto extension = PathToUtf8String(path.extension());
            const std::set<std::string> textTypes{".as", ".json", ".prefab", ".material", ".hlsl", ".txt", ".csv", ".xml"};
            const std::set<std::string> binaryTypes{".png", ".jpg", ".jpeg", ".bmp", ".dds", ".tga", ".gif", ".wav", ".mp3", ".ogg", ".flac", ".fbx", ".obj", ".gltf", ".glb", ".mp4", ".ttf", ".otf"};
            Check(textTypes.contains(extension) || binaryTypes.contains(extension), "Unsupported asset extension");
            if (action == "read") { Check(textTypes.contains(extension), "Read supports UTF-8 text/JSON assets; use capture for rendered images"); result["text"] = ReadFile(path, 100000); }
            else { stopped();
                if (action == "delete") { Check(std::filesystem::is_regular_file(path), "Asset not found"); run(std::make_unique<FileCommand>(path, std::nullopt)); }
                else if (action == "write" || action == "import") {
                    Check(!std::filesystem::exists(path) || args.value("overwrite", false), "Asset exists; overwrite=true is required");
                    std::string data;
                    if (action == "write") { Check(textTypes.contains(extension), "Binary asset writes require import"); data = args.at("text").get<std::string>(); Check(data.size() <= 100000, "Asset text too large"); }
                    else { const auto source = Utf8StringToPath(EditorApiProjectPath(Text(args, "source"), false)); Check(source.extension() == path.extension(), "Import must preserve extension"); data = ReadFile(source); }
                    if (extension == ".json" || extension == ".prefab" || extension == ".material") Check(J::accept(data), "Invalid JSON asset");
                    run(std::make_unique<FileCommand>(path, data));
                } else if (action == "move") {
                    const auto destination = Utf8StringToPath(EditorApiProjectPath(Text(args, "destination"))); Check(destination != path && destination.extension() == path.extension(), "Invalid destination");
                    Check(!std::filesystem::exists(destination) || args.value("overwrite", false), "Destination exists");
                    auto group = std::make_unique<CompositeCommand>("AI move asset"); group->AddCommand(std::make_unique<FileCommand>(destination, ReadFile(path))); group->AddCommand(std::make_unique<FileCommand>(path, std::nullopt)); run(std::move(group));
                } else throw std::runtime_error("Unknown asset action");
                result["reload_required"] = true;
            }
        }
    } else if (tool == "prefab") {
        stopped(); const auto action = Text(args, "action");
        if (action == "instantiate") {
            const auto path = EditorApiProjectPath(Text(args, "path")); Check(Utf8StringToPath(path).extension() == L".prefab", "Expected .prefab");
            const auto json = J::parse(ReadFile(Utf8StringToPath(path))); auto nodes = PrefabUtility::PrepareNodesForInstantiation(PrefabUtility::LoadPrefabNodes(json), false);
            Check(!nodes.empty() && nodes.size() <= 1024, "Invalid prefab nodes"); result["id"] = nodes.front().json.at("objectID");
            auto &components = nodes.front().json["components"]; if (!components.is_array()) components = J::array();
            components.push_back({{"type", "PrefabInstanceComponent"}, {"data", {{"customData", {{"prefabID", json.at("prefabID")}}}}}});
            EmptyObject *parent = args.contains("parent") ? Object(context, J{{"id", args.at("parent")}}) : nullptr;
            run(std::make_unique<PasteObjectCommand>(std::move(nodes), parent, MAXSIZE_T, "Prefab", "AI instantiate prefab"));
        } else {
            auto *object = Object(context, args); auto *marker = object->GetComponent<PrefabInstanceComponent>();
            if (action == "save") {
                const auto path = EditorApiProjectPath(Text(args, "path")); Check(Utf8StringToPath(path).extension() == L".prefab", "Expected .prefab");
                Check(!std::filesystem::exists(Utf8StringToPath(path)) || args.value("overwrite", false), "Prefab exists; overwrite=true is required");
                const UUID128 prefabID(true); const auto json = PrefabUtility::BuildPrefabJson(context, object, prefabID);
                Check(PrefabAssetManager::CreatePrefabFile(prefabID, json, path), "Cannot save prefab"); result["undoable"] = false; result["prefab_id"] = prefabID.ToString();
            } else {
                Check(marker, "Object is not a prefab instance");
                if (action == "unpack") run(std::make_unique<RemoveComponentCommand>(object, marker));
                else {
                    const auto path = marker->GetPrefabPath(); Check(!path.empty(), "Prefab source missing");
                    const auto logical = ProjectPaths::ToLogical(path); EditorApiProjectPath(logical);
                    if (action == "apply") { Check(args.value("overwrite", false), "Applying to prefab asset requires overwrite=true"); Check(PrefabSyncUtility::ApplyAll(context, object), "Prefab apply failed"); result["undoable"] = false; }
                    else { Check(action == "revert" && services && services->commands, "Prefab revert requires live history"); Check(PrefabSyncUtility::RevertAll(context, services->commands, object), "Prefab revert failed"); result["undoable"] = true; }
                }
            }
        }
    } else return {};
    return result.dump();
}
}
#endif
