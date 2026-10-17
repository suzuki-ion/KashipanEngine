#include "Scene/Editor/DevelopmentAssistantApi.h"
#ifdef USE_IMGUI
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/ShortTextBehaviorEditor.h"
#include "Scene/Editor/ShortTextEventPlan.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/ScriptComponent.h"
#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include <json.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace KashipanEngine {
namespace {
using J = nlohmann::json;
J Field(const char *type, const char *description = "") { return {{"type", type}, {"description", description}}; }
J Tool(const char *name, const char *description, J properties, J required, bool readOnly = false) {
    return {{"name", name}, {"description", description}, {"inputSchema", {{"type", "object"}, {"properties", properties},
        {"required", required}, {"additionalProperties", false}}},
        {"annotations", {{"readOnlyHint", readOnly}, {"destructiveHint", !readOnly}, {"openWorldHint", false}}}};
}
std::string Text(const J &j, const char *key, size_t maximum = 256) {
    auto value = j.at(key).get<std::string>();
    if (value.empty() || value.size() > maximum || value.find('\0') != std::string::npos) throw std::runtime_error(std::string("Invalid ") + key);
    return value;
}
template<class T> J Encode(const T &v) {
    if constexpr (std::is_same_v<T, Vector2>) return J::array({v.x, v.y});
    else if constexpr (std::is_same_v<T, Vector3>) return J::array({v.x, v.y, v.z});
    else if constexpr (std::is_same_v<T, Vector4>) return J::array({v.x, v.y, v.z, v.w});
    else if constexpr (std::is_same_v<T, Color>) return J::array({v.r, v.g, v.b, v.a});
    else return v;
}
template<class T> T Decode(const J &j) {
    if constexpr (std::is_same_v<T, std::string>) {
        auto value = j.get<std::string>();
        if (value.size() > 4096 || value.find('\0') != std::string::npos) throw std::runtime_error("String too large");
        return value;
    } else if constexpr (std::is_same_v<T, bool>) return j.get<bool>();
    else if constexpr (std::is_arithmetic_v<T>) {
        if (!j.is_number() || (std::is_integral_v<T> && !j.is_number_integer())) throw std::runtime_error("Wrong numeric type");
        const double value = j.get<double>();
        if (!std::isfinite(value) || std::abs(value) > 1000000 || value < static_cast<double>(std::numeric_limits<T>::lowest()) ||
            value > static_cast<double>(std::numeric_limits<T>::max())) throw std::runtime_error("Numeric value out of range");
        return static_cast<T>(value);
    } else {
        constexpr size_t count = std::is_same_v<T, Vector2> ? 2 : std::is_same_v<T, Vector3> ? 3 : 4;
        if (!j.is_array() || j.size() != count) throw std::runtime_error("Wrong vector dimension");
        if constexpr (count == 2) return T(Decode<float>(j[0]), Decode<float>(j[1]));
        else if constexpr (count == 3) return T(Decode<float>(j[0]), Decode<float>(j[1]), Decode<float>(j[2]));
        else return T(Decode<float>(j[0]), Decode<float>(j[1]), Decode<float>(j[2]), Decode<float>(j[3]));
    }
}
// Registered setters fire the same notifications as Inspector edits; references survive pool movement.
template<class T> class MemberCommand final : public IEditorCommand {
public:
    MemberCommand(IObjectComponent *component, std::string member, T before, T after)
        : ref_(component->GetComponentRef()), member_(std::move(member)), before_(before), after_(after) {}
    bool Execute(SceneEditorContext *context) override { return Write(context, after_); }
    bool Undo(SceneEditorContext *context) override { return Write(context, before_); }
    std::string GetName() const override { return "AI: " + member_; }
private:
    bool Write(SceneEditorContext *context, const T &value) {
        auto *component = context->ResolveComponent(ref_);
        if (!component) return false;
        T previous{};
        if (!component->GetMemberValue(member_, previous)) return false;
        try { if (component->SetMemberValue(member_, value)) return true; } catch (...) { }
        bool restored = false;
        try { restored = component->SetMemberValue(member_, previous); } catch (...) { }
        SetAppliedChangesAfterFailure(!restored);
        return false;
    }
    ComponentRef ref_;
    std::string member_;
    T before_, after_;
};
template<class T> bool Member(IObjectComponent *component, const std::string &name, J &description,
    const J *value, std::unique_ptr<IEditorCommand> &command) {
    T before{};
    if (!component->GetMemberValue(name, before)) return false;
    description["value"] = Encode(before);
    const bool writable = component->GetMemberVariable(name)->IsWritable();
    description["writable"] = writable;
    if (value) {
        if (!writable) throw std::runtime_error("Member is read-only");
        command = std::make_unique<MemberCommand<T>>(component, name, before, Decode<T>(*value));
    }
    return true;
}
J MemberInfo(IObjectComponent *component, const std::string &name, const J *value = nullptr,
    std::unique_ptr<IEditorCommand> *output = nullptr) {
    J info{{"name", name}, {"writable", false}};
    std::unique_ptr<IEditorCommand> command;
    const bool known = Member<bool>(component, name, info, value, command) || Member<int>(component, name, info, value, command) ||
        Member<unsigned>(component, name, info, value, command) || Member<float>(component, name, info, value, command) ||
        Member<double>(component, name, info, value, command) || Member<std::string>(component, name, info, value, command) ||
        Member<Vector2>(component, name, info, value, command) || Member<Vector3>(component, name, info, value, command) ||
        Member<Vector4>(component, name, info, value, command) || Member<Color>(component, name, info, value, command);
    info["supported"] = known;
    if (value && !known) throw std::runtime_error("Member type is not supported by editor API");
    if (output) *output = std::move(command);
    return info;
}
J Summary(EmptyObject *object) {
    auto *transform = object->GetComponent<Transform>();
    auto *parent = transform ? transform->GetParentObject() : nullptr;
    return {{"id", object->GetObjectID().ToString()}, {"name", object->GetName()}, {"active_in_hierarchy", object->IsActive()},
        {"parent", parent ? J(parent->GetObjectID().ToString()) : J(nullptr)}};
}
class ComponentActiveCommand final : public IEditorCommand {
public:
    ComponentActiveCommand(IObjectComponent *component, bool active) : ref_(component->GetComponentRef()), before_(component->IsActive()), after_(active) {}
    bool Execute(SceneEditorContext *context) override { return Apply(context, after_); }
    bool Undo(SceneEditorContext *context) override { return Apply(context, before_); }
    std::string GetName() const override { return "AI component active"; }
private:
    bool Apply(SceneEditorContext *context, bool active) { auto *component = context->ResolveComponent(ref_); if (!component) return false; component->SetActive(active); return true; }
    ComponentRef ref_; bool before_, after_;
};
}
std::string EditorApiCatalog() {
    const auto id = Field("string", "Exact object UUID from scene/create_object; names are not identifiers");
    const auto component = Field("integer", "Stable component ID returned by object; never use an array index");
    auto catalog = J::array({
        Tool("scene", "Read current scene, selected UUID, registered component types, and paged object summaries.",
            {{"offset", Field("integer")}, {"limit", Field("integer")}}, J::array(), true),
        Tool("object", "Inspect an object and its registered members/values. Unsupported member types are read-only.", {{"id", id}}, {"id"}, true),
        Tool("create_object", "Create an empty object (with Transform), optionally under a parent; returns its UUID. Undoable.",
            {{"name", Field("string")}, {"parent", id}}, {"name"}),
        Tool("delete_object", "Delete an object and descendants. Undoable.", {{"id", id}}, {"id"}),
        Tool("set_object", "Rename/enable/disable object. Undoable.", {{"id", id}, {"name", Field("string")}, {"active", Field("boolean")}}, {"id"}),
        Tool("add_component", "Add a registered object component with default settings; inspect object afterwards for its stable component ID. Undoable.",
            {{"id", id}, {"type", Field("string")}}, {"id", "type"}),
        Tool("remove_component", "Remove the exact stable component instance. Undoable.", {{"id", id}, {"component", component}}, {"id", "component"}),
        Tool("set_component_active", "Enable/disable a component instance. Undoable.",
            {{"id", id}, {"component", component}, {"active", Field("boolean")}}, {"id", "component", "active"}),
        Tool("set_property", "Set an existing registered member through its typed setter. Scalars/string/bool or whole numeric vectors/colors (arrays of 2/3/4). Use exact member names from object. Transform rotation is radians. Undoable.",
            {{"id", id}, {"component", component}, {"member", Field("string")}, {"value", {{"description", "A typed scalar or numeric array; must match the inspected member"}}}}, {"id", "component", "member", "value"}),
        Tool("apply_sentence", "Apply a supported Japanese short sentence using existing component/event/keyframe plans. Returns validation steps. Set preview=true first. Undoable; unsupported requests fail without changes.",
            {{"id", id}, {"sentence", Field("string")}, {"preview", Field("boolean")}}, {"id", "sentence", "preview"}),
        Tool("attach_script", "Attach an existing project Assets/*.as script as ScriptComponent. Does not execute game Play; compilation may occur. Undoable. Files must be created with file-edit mode first.",
            {{"id", id}, {"path", Field("string", "Project-relative Assets/... .as path")}, {"class", Field("string")}}, {"id", "path"})
    });
    for (const auto &tool : J::parse(ExtendedEditorApiCatalog())) catalog.push_back(tool);
    return catalog.dump();
}
std::string ExecuteEditorApi(SceneEditorContext *context, const UUID128 &selected,
    const std::function<bool(std::unique_ptr<IEditorCommand>)> &execute,
    const std::string &tool, const std::string &arguments, EditorApiServices *services) {
    try {
        if (!context) throw std::runtime_error("Editor is unavailable");
        if (arguments.size() > 131072) throw std::runtime_error("Arguments too large");
        const auto args = J::parse(arguments);
        const auto catalog = J::parse(EditorApiCatalog());
        auto spec = std::find_if(catalog.begin(), catalog.end(), [&](const auto &item) { return item.at("name") == tool; });
        if (spec == catalog.end() || !args.is_object()) throw std::runtime_error("Unknown editor tool");
        const auto &schema = spec->at("inputSchema");
        for (const auto &key : schema.at("required")) if (!args.contains(key.get<std::string>())) throw std::runtime_error("Missing argument: " + key.get<std::string>());
        for (auto it = args.begin(); it != args.end(); ++it) if (!schema.at("properties").contains(it.key())) throw std::runtime_error("Unknown argument: " + it.key());
        for (auto it = args.begin(); it != args.end(); ++it) {
            const auto type = schema.at("properties").at(it.key()).value("type", std::string{});
            if ((type == "string" && !it->is_string()) || (type == "integer" && !it->is_number_integer()) ||
                (type == "boolean" && !it->is_boolean())) throw std::runtime_error("Wrong argument type: " + it.key());
        }
        const auto extended = ExecuteExtendedEditorApi(context, selected, execute, tool, arguments, services);
        if (!extended.empty()) return extended;
        if (context->IsPlaying() && tool != "scene" && tool != "object") throw std::runtime_error("Stop Play before editing");
        J result{{"ok", true}};
        if (tool == "scene") {
            const auto &objects = context->GetSceneObjects();
            const int offset = args.value("offset", 0), limit = args.value("limit", 100);
            if (offset < 0 || limit < 1 || limit > 200) throw std::runtime_error("Invalid pagination");
            result["scene"] = context->GetSceneID().ToString(); result["name"] = context->GetName();
            result["selected"] = context->GetSceneObject(selected) ? J(selected.ToString()) : J(nullptr);
            result["component_types"] = GetRegisteredObjectComponentTypes(); result["objects"] = J::array();
            const size_t last = std::min(objects.size(), static_cast<size_t>(offset) + limit);
            for (size_t i = static_cast<size_t>(offset); i < last; ++i) if (objects[i]) result["objects"].push_back(Summary(objects[i]));
            result["total"] = objects.size(); result["next_offset"] = last < objects.size() ? J(last) : J(nullptr);
            return result.dump();
        }
        auto resolve = [&](const char *key) {
            const UUID128 id(Text(args, key));
            if (!id.IsValid()) throw std::runtime_error("Invalid object UUID");
            auto *object = context->GetSceneObject(id);
            if (!object) throw std::runtime_error("Object no longer exists");
            return object;
        };
        std::unique_ptr<IEditorCommand> command;
        if (tool == "create_object") {
            const auto name = Text(args, "name");
            if (args.contains("parent")) {
                auto create = std::make_unique<CreateChildObjectCommand>(name, resolve("parent"));
                result["id"] = create->GetObjectID().ToString(); command = std::move(create);
            } else {
                auto create = std::make_unique<CreateObjectCommand>(name);
                result["id"] = create->GetObjectID().ToString(); command = std::move(create);
            }
        } else {
            auto *object = resolve("id"); result["id"] = object->GetObjectID().ToString();
            if (tool == "object") {
                result["object"] = Summary(object); result["components"] = J::array();
                result["object"]["active"] = context->SaveObjectToJson(object).value("isActive", object->IsActive());
                for (const auto &[component, addedID] : object->GetAllComponents()) {
                    if (!component) continue;
                    J item{{"component", addedID}, {"type", component->GetComponentType()}, {"active", component->IsActive()}, {"members", J::array()}};
                    for (const auto &[name, member] : component->GetAllMemberVariables()) { (void)member; item["members"].push_back(MemberInfo(component, name)); }
                    if (auto *script = dynamic_cast<ScriptComponent *>(component)) {
                        item["script_path"] = script->GetScriptPath(); item["script_class"] = script->GetComponentClass(); item["script_error"] = script->GetLastError();
                    }
                    result["components"].push_back(std::move(item));
                }
                if (result.dump().size() > 120000) throw std::runtime_error("Object metadata too large");
                return result.dump();
            }
            if (tool == "delete_object") command = std::make_unique<DeleteObjectCommand>(object);
            else if (tool == "set_object") {
                const auto name = args.contains("name") ? Text(args, "name") : object->GetName();
                const auto saved = context->SaveObjectToJson(object);
                if (saved.empty()) throw std::runtime_error("Object does not support undoable serialization");
                const bool active = saved.value("isActive", true);
                command = std::make_unique<ObjectPropertyCommand>(object, object->GetName(), name, active, args.value("active", active));
            } else if (tool == "add_component") {
                const auto type = Text(args, "type"); const auto &types = GetRegisteredObjectComponentTypes();
                if (std::find(types.begin(), types.end(), type) == types.end()) throw std::runtime_error("Unknown component type");
                command = std::make_unique<AddComponentCommand>(object, type);
            } else if (tool == "apply_sentence") {
                const auto text = Text(args, "sentence", 767);
                ShortTextComponentPlan plan;
                const auto property = ParseShortTextPropertyRequest(text, object);
                if (property.recognized) {
                    if (!property.success) throw std::runtime_error(property.message);
                    command = MakeShortTextPropertyCommand(object, property.request, text, &plan);
                } else {
                    const auto event = ShortText::ParseEventRequest(text);
                    if (event.recognized) {
                        if (!event.success) throw std::runtime_error(event.message);
                        command = MakeShortTextEventCommand(context, object, event.request, text, &plan);
                    } else {
                        const auto parsed = ShortText::Parse(text);
                        if (!parsed.success) throw std::runtime_error(parsed.message);
                        plan = PlanShortTextBehavior(object, parsed.recipe);
                        if (plan.success) command = MakeShortTextBehaviorCommand(object, parsed.recipe, text);
                    }
                }
                if (!plan.success || !command) throw std::runtime_error(plan.message.empty() ? "Unsupported sentence" : plan.message);
                result["steps"] = plan.steps; result["requirements"] = plan.requirements;
                if (args.at("preview").get<bool>()) { result["preview"] = true; return result.dump(); }
            } else if (tool == "attach_script") {
                const auto path = Text(args, "path", 1024);
                const auto relative = Utf8StringToPath(path);
                const auto root = std::filesystem::weakly_canonical(Utf8StringToPath(ProjectPaths::ProjectRoot()) / L"Assets");
                const auto physical = std::filesystem::weakly_canonical(Utf8StringToPath(ProjectPaths::ProjectRoot()) / relative);
                const auto within = physical.lexically_relative(root);
                if (relative.is_absolute() || relative.extension() != L".as" || within.empty() || *within.begin() == L".." || !std::filesystem::is_regular_file(physical))
                    throw std::runtime_error("Script must be an existing project Assets/*.as file");
                J data{{"scriptPath", path}, {"fields", J::object()}};
                if (args.contains("class")) data["componentClass"] = Text(args, "class");
                command = std::make_unique<AddComponentCommand>(object, "ScriptComponent", J{{"type", "ScriptComponent"}, {"data", {{"customData", data}}}});
            } else {
                if (!args.at("component").is_number_unsigned() && !args.at("component").is_number_integer()) throw std::runtime_error("Invalid component ID");
                if (args.at("component").get<double>() < 0) throw std::runtime_error("Invalid component ID");
                const auto componentID = args.at("component").get<size_t>();
                auto *component = context->ResolveComponent(ComponentRef(object->GetObjectID(), componentID));
                if (!component) throw std::runtime_error("Component no longer exists");
                if (tool == "remove_component") command = std::make_unique<RemoveComponentCommand>(object, component);
                else if (tool == "set_component_active") {
                    command = std::make_unique<ComponentActiveCommand>(component, args.at("active").get<bool>());
                } else if (tool == "set_property") {
                    const auto name = Text(args, "member");
                    if (!component->GetMemberVariable(name)) throw std::runtime_error("Unknown registered member");
                    MemberInfo(component, name, &args.at("value"), &command);
                }
            }
        }
        if (!command || !execute || !execute(std::move(command))) throw std::runtime_error("Editor command failed; inspect Undo history if rollback also failed");
        result["undoable"] = true;
        if (tool == "attach_script") if (auto *object = context->GetSceneObject(UUID128(result.at("id").get<std::string>()))) {
            auto scripts = object->GetComponents<ScriptComponent>();
            if (!scripts.empty()) {
                auto *script = *std::max_element(scripts.begin(), scripts.end(), [](const auto *a, const auto *b) { return a->GetComponentRef().addedID < b->GetComponentRef().addedID; });
                result["component"] = script->GetComponentRef().addedID;
                result["script_error"] = script->GetLastError();
                result["script_class"] = script->GetClassName();
            }
        }
        return result.dump();
    } catch (const std::exception &e) { return J{{"ok", false}, {"error", e.what()}}.dump(); }
}
}
#endif
