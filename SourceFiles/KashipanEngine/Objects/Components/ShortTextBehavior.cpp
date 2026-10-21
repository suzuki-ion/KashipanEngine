#include "Objects/Components/ShortTextBehavior.h"
#include "Objects/Components/ShortTextExtended.h"
#include "Objects/Components/Transform.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/ShortTextRecipeJson.h"
#include "Objects/Components/Velocity.h"
#include "Input/Input.h"
#include "Input/Keyboard.h"
#include "Input/Mouse.h"
#include "Objects/Components/UI/UIButton.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/Render/BitmapTextRenderer.h"
#include <sstream>
#include "Objects/Components/AudioSource.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Scene/Components/SceneObjectCollider.h"
#include <limits>
#include <set>

namespace KashipanEngine {
namespace {
JSON RecipeJson(const ShortText::Recipe &r, const std::string &source) {
    JSON result = ShortText::EncodeRecipe(r);
    result["sourceText"] = source;
    return result;
}
}
JSON MakeShortTextBehaviorState(const ShortText::Recipe &r, const std::string &source) {
    return JSON{{"priority", r.kind == ShortText::Kind::Rules ? 0 : 1}, {"isActive", true}, {"tag", ""}, {"customData", RecipeJson(r, source)}};
}
std::unique_ptr<IObjectComponent> ShortTextBehavior::Clone() const {
    auto copy = std::make_unique<ShortTextBehavior>();
    copy->recipe_ = recipe_;
    copy->sourceText_ = sourceText_;
    return copy;
}
void ReadShortTextSceneInput(const EmptyObject *owner, SceneContext *scene, const ShortText::Recipe &recipe,
    ShortText::ProgramState &state, ShortText::InputSnapshot &input, ShortText::ProgramMemory *memory, double dt) {
    input = {};
    state.variables.clear(); state.blockedVariables.clear(); state.objects.clear();
    state.environment=JSON::object();state.lastCreatedId.clear();state.world={};state.selfId.clear();
    if (!owner || !scene || !ShortText::IsValid(recipe)) return;
    state.selfId = owner->GetObjectID().ToString();
    auto array = [](const Vector3 &v) { return std::array<float, 3>{v.x, v.y, v.z}; };
    if (auto *transform = owner->GetComponent<Transform>()) state.world = array(transform->GetWorldPosition());
    try {ShortText::ReadExtendedValues(owner, scene, state, memory, dt);}catch(const std::exception &e){state.environment["snapshot_error"]=e.what();}
    if (memory&&memory->extensionValues.contains("last_created")&&memory->extensionValues["last_created"].is_string())state.lastCreatedId=memory->extensionValues["last_created"].get<std::string>();
    std::set<std::string> propertyPaths;
    auto collectData = [&](auto &&recurse, const JSON &data) -> void {
        if (data.is_object()) {
            if (data.contains("path") && data["path"].is_string()) propertyPaths.insert(data["path"].get<std::string>());
            if (data.contains("operation")&&data["operation"].is_string()&&data["operation"]=="tween_property"&&data.contains("name")&&data["name"].is_string())propertyPaths.insert(data["name"].get<std::string>());
            if (data.contains("read") && data["read"].is_string()) {
                const auto key = data["read"].get<std::string>();
                const auto delimiter = key.find(':', 9);
                if (key.starts_with("property:") && delimiter != std::string::npos) propertyPaths.insert(key.substr(delimiter + 1));
            }
            for (const auto &item : data.items()) recurse(recurse, item.value());
        } else if (data.is_array()) for (const auto &item : data) recurse(recurse, item);
    };
    for (const auto &rule : recipe.rules) {
        for (const auto &c : rule.conditions) collectData(collectData, c.data);
        for (const auto &a : rule.actions) {
            collectData(collectData, a.data);
            if (a.operation == ShortText::Operation::TweenProperty) propertyPaths.insert(a.name);
        }
    }
    std::unordered_map<std::string,size_t> nameCounts;
    for(auto *object:scene->GetSceneObjects())if(object)++nameCounts[object->GetName()];
    state.objects.reserve(scene->GetSceneObjects().size());
    for (auto *object : scene->GetSceneObjects()) {
        if (!object) continue;
        auto *transform = object->GetComponent<Transform>();
        ShortText::ObjectSnapshot snapshot;
        snapshot.id = object->GetObjectID().ToString(); snapshot.name = object->GetName(); snapshot.tag = object->GetTagName();
        snapshot.active = object->IsActive();
        if (transform) {
            snapshot.position = array(transform->GetTranslate()); snapshot.world = array(transform->GetWorldPosition());
            snapshot.rotation = array(transform->GetRotate()); snapshot.scale = array(transform->GetScale());
            if (auto *parent = transform->GetParentObject()) snapshot.parent = parent->GetObjectID().ToString();
        }
        for (const auto &[component, addedID] : object->GetAllComponents()) {
            (void)addedID; if (component) snapshot.components.push_back(component->GetComponentType());
        }
        for (const auto &path : propertyPaths) {
            JSON value;
            if (!ShortText::ReadExtendedProperty(object, path, value)) continue;
            snapshot.properties[path] = value;
            state.environment["property:" + snapshot.id + ":" + path] = value;
            if (object == owner) state.environment["property:self:" + path] = value;
            if (nameCounts[snapshot.name] == 1) state.environment["property:" + snapshot.name + ":" + path] = value;
        }
        JSON objectState = {{"exists", true}, {"active", snapshot.active}, {"id", snapshot.id}, {"components", snapshot.components}};
        state.environment["object:" + snapshot.id] = objectState;
        if (object == owner) state.environment["object:self"] = objectState;
        if (nameCounts[snapshot.name] == 1) state.environment["object:" + snapshot.name] = objectState;
        if (auto *v = object->GetComponent<Velocity>()) { snapshot.velocity = array(v->GetVelocity()); snapshot.acceleration = array(v->GetAcceleration()); }
        state.objects.push_back(std::move(snapshot));
    }
    if (auto *button = owner->GetComponent<UIButton>()) input.buttonClicked = button->IsClicked();
    std::set<std::string> names;
    for (const auto &rule : recipe.rules) {
        if (!rule.variable.empty()) names.insert(rule.variable);
        for (const auto &c : rule.conditions) if (ShortText::IsVariable(c.test)) names.insert(c.name);
        for (const auto &a : rule.actions) if (ShortText::IsVariable(a.operation) || a.operation == ShortText::Operation::ShowVariableText) names.insert(a.name);
    }
    for (const auto &name : names) {
        auto *value = scene->GetSceneVariable(name);
        if (!value) continue;
        double number = 0;
        if (value->IsType<float>()) number = value->AnyCast<float>();
        else if (value->IsType<double>()) number = value->AnyCast<double>();
        else if (value->IsType<int>()) number = value->AnyCast<int>();
        else { state.blockedVariables.insert(name); continue; }
        if (!std::isfinite(number) || std::abs(number) > 1000000000) state.blockedVariables.insert(name);
        else state.variables[name] = static_cast<float>(number);
    }
    if (auto *device = scene->GetInput()) {
        const auto &keyboard = device->GetKeyboard();
        const auto &mouse = device->GetMouse();
        for (int button = 0; button < 3; ++button) { input.mousePressed[button] = mouse.IsButtonTrigger(button); input.mouseHeld[button] = mouse.IsButtonDown(button); input.mouseReleased[button] = mouse.IsButtonRelease(button); }
        for (const auto &rule : recipe.rules) {
            const auto key = static_cast<size_t>(rule.key);
            input.pressed[key] = keyboard.IsTrigger(rule.key);
            input.held[key] = keyboard.IsDown(rule.key);
            input.released[key] = keyboard.IsRelease(rule.key);
        }
    }
    auto *collisions = scene->GetComponent<SceneObjectCollider>();
    if (!collisions) return;
    for (size_t i = 0; i < recipe.rules.size(); ++i) {
        const auto &rule = recipe.rules[i];
        if (!ShortText::IsContact(rule.event)) continue;
        std::set<std::string> partners;
        auto collect = [&](const auto &hits) {
            for (const auto &hit : hits) if (hit.otherObject && hit.otherObject != owner &&
                (rule.tag.empty() || hit.otherObject->GetTagName() == rule.tag)) partners.insert(hit.otherObject->GetObjectID().ToString());
        };
        for (const auto &[component, addedID] : owner->GetAllComponents()) {
            (void)addedID;
            auto *collider = dynamic_cast<ICollider *>(component);
            if (!collider || !collider->IsActive()) continue;
            if (collider->Is2D()) {
                if (auto shape = collider->BuildColliderInfo2D()) {
                    input.contactReady[i] = true; shape->sourceCollider = collider;
                    collect(collisions->Query2D(*shape, true, owner));
                }
            } else if (auto shape = collider->BuildColliderInfo3D()) {
                input.contactReady[i] = true; shape->sourceCollider = collider;
                collect(collisions->Query3D(*shape, {}, Quaternion::Identity(), true, owner));
            }
        }
        for (const auto &id : partners) { if (input.contacts[i].size() == 32) break; input.contacts[i].push_back(id); }
    }
}
namespace {
void WriteSceneNumber(SceneContext *scene, const std::string &name, float number) {
    if (!std::isfinite(number) || std::abs(number) > 1000000000) return;
    auto *old = scene->GetSceneVariable(name);
    if (!old || old->IsType<float>()) scene->AddSceneVariable<float>(name, number);
    else if (old->IsType<double>()) scene->AddSceneVariable<double>(name, number);
    else if (old->IsType<int>()) scene->AddSceneVariable<int>(name, static_cast<int>(number));
}
void SetWorldPosition(Transform *transform, const Vector3 &world) {
    Vector3 local = world;
    if (auto *parent = transform->GetParentObject()) if (auto *parentTransform = parent->GetComponent<Transform>())
        local = world.Transform(parentTransform->GetWorldMatrix().Inverse());
    if (std::isfinite(local.x) && std::isfinite(local.y) && std::isfinite(local.z)) transform->SetTranslate(local);
}
}
void ApplyShortTextSceneEffects(const EmptyObject *owner, SceneContext *scene, const ShortText::ProgramState &state,
    ShortText::ProgramMemory *memory, std::string *error) {
    if (!owner || !scene) return;
    const std::string ownerId = owner->GetObjectID().ToString();
    std::vector<std::string> deletes;
    std::string nextScene;
    int generated = 0;
    const auto &commands = state.commands.empty() ? state.effects : state.commands;
    if (error) error->clear();
    auto executionEnvironment = state.environment;
    if (state.commands.empty()) for (const auto &name : state.writtenVariables) WriteSceneNumber(scene, name, state.variables.at(name));
    for (const auto &a : commands) {
        if (a.axis < 0 || a.axis > 2) continue;
        auto *currentOwner=scene->GetSceneObject(UUID128(ownerId));
        if(!currentOwner)break;
        std::string targetId = a.targetId.empty() ? ownerId : a.targetId;
        if (targetId == "@last_created" && memory) targetId = memory->extensionValues.value("last_created", std::string{});
        auto *target = scene->GetSceneObject(UUID128(targetId));
        if (!target) continue;
        auto *transform = target->GetComponent<Transform>();
        auto *audio = target->GetComponent<AudioSource>();
        auto axisValue = [&](Vector3 &vector) -> float & { return a.axis == 0 ? vector.x : a.axis == 1 ? vector.y : vector.z; };
        auto setAxis = [&](Vector3 vector) { axisValue(vector) = a.value; return vector; };
        using Op = ShortText::Operation;
        switch (a.operation) {
        case Op::EngineCommand: {
            ShortText::CommandContext context{currentOwner, target, scene, memory, a.frameSeconds, &executionEnvironment};
            std::string failure;
            if (!ShortText::ApplyExtendedCommand(a, context, failure) && error && !failure.empty()) *error = a.name + ": " + failure;
            break;
        }
        case Op::SetVelocity: case Op::SetAcceleration: case Op::StopVelocity: {
            auto *v = target->GetComponent<Velocity>(); if (!v) v = target->AddComponent<Velocity>();
            if (!v) break;
            if (a.operation == Op::SetAcceleration) v->SetAcceleration(setAxis(v->GetAcceleration()));
            else { Vector3 value = v->GetVelocity(); axisValue(value) = a.operation == Op::StopVelocity ? 0 : a.value; v->SetVelocity(value); }
            break;
        }
        case Op::SetPosition: if (transform) transform->SetTranslate(setAxis(transform->GetTranslate())); break;
        case Op::SetRotation: if (transform) { auto value = transform->GetRotate(); axisValue(value) = a.value * 0.017453292519943295f; transform->SetRotate(value); } break;
        case Op::SetScale: if (transform) transform->SetScale(setAxis(transform->GetScale())); break;
        case Op::SetVariable:
            WriteSceneNumber(scene, a.name, a.value);
            if(auto *value=scene->GetSceneVariable(a.name)){
                if(value->IsType<int>())executionEnvironment["scene:"+a.name]=value->AnyCast<int>();
                else if(value->IsType<float>())executionEnvironment["scene:"+a.name]=value->AnyCast<float>();
                else if(value->IsType<double>())executionEnvironment["scene:"+a.name]=value->AnyCast<double>();
            }break;
        case Op::PlaySound: if (audio) audio->Play(); break;
        case Op::StopSound: if (audio) audio->Stop(); break;
        case Op::SetVolume: if (audio) audio->SetVolume(a.value); break;
        case Op::SetActive: target->SetActive(a.value != 0); break;
        case Op::SetVisible:
            for (const auto &[component, id] : target->GetAllComponents()) {
                (void)id; if (!component) continue;
                const auto &type = component->GetComponentType();
                if (type == "MeshRenderer" || type == "SkinnedMeshRenderer" || type == "SpriteRenderer" || type == "TextRenderer" || type == "BitmapTextRenderer") component->SetActive(a.value != 0);
            }
            break;
        case Op::CreateObject: case Op::CloneObject: {
            if (generated >= 16 || scene->GetSceneObjects().size() >= 4096) break;
            ++generated;
            auto *made = a.operation == Op::CreateObject ? scene->CreateEmptyObject(a.name.empty() ? "Generated" : a.name) : scene->CloneObject(target, a.name, false);
            if (made && scene->GetSceneObject(made)) {
                if (memory) memory->extensionValues["last_created"] = made->GetObjectID().ToString();
                made->SetActive(true);
                if (!scene->GetSceneObject(made)) break;
                auto *origin = scene->GetSceneObject(UUID128(ownerId));
                auto *originTransform = origin ? origin->GetComponent<Transform>() : nullptr;
                auto *madeTransform = made->GetComponent<Transform>();
                if (madeTransform && originTransform) { auto point = originTransform->GetWorldPosition(); axisValue(point) += a.value; SetWorldPosition(madeTransform, point); }
            }
            break;
        }
        case Op::DeleteObject: deletes.push_back(target->GetObjectID().ToString()); break;
        case Op::ChangeScene: if (nextScene.empty()) nextScene = a.name; break;
        case Op::SetText: case Op::ShowVariableText: {
            std::string text = a.text;
            if (a.operation == Op::ShowVariableText) {
                auto *value = scene->GetSceneVariable(a.name); if (!value) break;
                std::ostringstream out; out << text;
                if (value->IsType<int>()) out << value->AnyCast<int>();
                else if (value->IsType<float>()) out << value->AnyCast<float>();
                else if (value->IsType<double>()) out << value->AnyCast<double>();
                else break;
                text = out.str();
            }
            if (auto *renderer = target->GetComponent<TextRenderer>()) { renderer->SetUseLocalizationKey(false); renderer->SetText(text); }
            else if (auto *bitmapRenderer = target->GetComponent<BitmapTextRenderer>()) bitmapRenderer->SetText(text);
            break;
        }
        case Op::Follow: case Op::MoveToward: {
            auto *reference = scene->GetSceneObject(UUID128(a.referenceId));
            auto *referenceTransform = reference ? reference->GetComponent<Transform>() : nullptr;
            if (!transform || !referenceTransform) break;
            auto destination = referenceTransform->GetWorldPosition();
            if (a.operation == Op::Follow) axisValue(destination) += a.value;
            else {
                const auto current = transform->GetWorldPosition(); const auto difference = destination - current;
                const float length = difference.Length();
                if (length > 0) destination = current + difference * (std::min(length, a.value * a.frameSeconds) / length);
            }
            SetWorldPosition(transform, destination); break;
        }
        default: break; // rate/tween actions have already been converted to absolute setters by the runtime
        }
    }
    std::sort(deletes.begin(), deletes.end()); deletes.erase(std::unique(deletes.begin(), deletes.end()), deletes.end());
    for (const auto &id : deletes) if (auto *target = scene->GetSceneObject(UUID128(id))) scene->DeleteObject(target);
    if (!nextScene.empty()) { scene->SetNextSceneName(nextScene); scene->ChangeToNextScene(); }
}
void ShortTextBehavior::Update() {
    auto *scene = GetOwnerSceneContext();
    if (!scene || !scene->IsPlaying()) return;
    auto *object = GetOwnerObjectContext();
    auto *transform = object ? object->GetComponent<Transform>() : nullptr;
    const double dt = GetDeltaTime();
    if (!transform || !std::isfinite(dt) || dt < 0) return;
    if (recipe_.kind == ShortText::Kind::Rules) {
        for (const auto &rule : recipe_.rules) if (rule.event == ShortText::Event::ButtonClicked) {
            if (auto *button = object->GetComponent<UIButton>()) if (button->GetUpdatePriority() >= GetUpdatePriority())
                button->SetUpdatePriority(GetUpdatePriority() == std::numeric_limits<int>::min() ? GetUpdatePriority() : GetUpdatePriority() - 1);
            break;
        }
        auto *velocity = object->GetComponent<Velocity>();
        if (ShortText::NeedsVelocity(recipe_.rules) && !velocity) return;
        ShortText::ProgramState state;
        auto read = [](const Vector3 &v) { return std::array<float, 3>{v.x, v.y, v.z}; };
        state.position = read(transform->GetTranslate());
        state.rotation = read(transform->GetRotate());
        state.scale = read(transform->GetScale());
        if (velocity) { state.velocity = read(velocity->GetVelocity()); state.acceleration = read(velocity->GetAcceleration()); }
        ShortText::InputSnapshot input;
        ReadShortTextSceneInput(GetOwnerObject(), scene, recipe_, state, input, &programMemory_, dt);
        ShortText::TickProgram(recipe_.rules, programMemory_, state, input, dt);
        ApplyShortTextSceneEffects(GetOwnerObject(), scene, state, &programMemory_, &executionError_);
        return;
    }
    const auto delta = ShortText::EvaluateDelta(recipe_, elapsed_, dt);
    transform->SetTranslate(transform->GetTranslate() + Vector3(delta.translation[0], delta.translation[1], delta.translation[2]));
    transform->SetRotate(transform->GetRotate() + Vector3(delta.rotation[0], delta.rotation[1], delta.rotation[2]));
    if (recipe_.kind == ShortText::Kind::Oscillate) elapsed_ = std::fmod(elapsed_ + dt, recipe_.period);
}
JSON ShortTextBehavior::SaveToJson() const { return RecipeJson(recipe_, sourceText_); }
bool ShortTextBehavior::LoadFromJson(const JSON &json) {
    if (!json.is_object()) return false;
    try {
        auto next = ShortText::DecodeRecipe(json);
        const auto source = json.value("sourceText", std::string{});
        if (!ShortText::IsValid(next) || source.size() > 512) return false;
        recipe_ = next;
        sourceText_ = source;
        elapsed_ = 0;
        programMemory_ = {};
        return true;
    } catch (const std::exception &) { return false; }
}
#if defined(USE_IMGUI)
void ShortTextBehavior::ShowImGui() {
    ImGui::TextWrapped("%s", sourceText_.c_str());
    if (!executionError_.empty()) ImGui::TextWrapped("実行できませんでした: %s", executionError_.c_str());
    if (recipe_.kind == ShortText::Kind::Rules) {
        ImGui::TextWrapped("%s", ShortText::Describe(recipe_).c_str());
        if (ShortText::NeedsVelocity(recipe_.rules) && !GetOwnerObjectContext()->GetComponent<Velocity>())
            ImGui::TextWrapped("Velocityが必要です。短文の挙動から再適用すると追加されます。");
        for (const auto &rule : recipe_.rules) {
            ImGui::Text("%s / %s", ShortText::EventLabels[static_cast<size_t>(rule.event)], ShortText::KeyName(rule.key).c_str());
            for (const auto &action : rule.actions)
                ImGui::BulletText("%s %c = %.3f", ShortText::OperationLabels[static_cast<size_t>(action.operation)], "XYZ"[action.axis], action.value);
        }
        return;
    }
    const char *kinds[] = {"往復", "回転", "移動"};
    ImGui::Text("%s / %c軸", kinds[static_cast<int>(recipe_.kind)], "XYZ"[recipe_.axis]);
    ImGui::TextUnformatted("再生中に動作します。短文の挙動ウィンドウから置き換えられます。");
    bool changed = ImGui::Combo("軸", &recipe_.axis, "X\0Y\0Z\0");
    if (recipe_.kind == ShortText::Kind::Oscillate) {
        changed |= ImGui::DragFloat("振幅", &recipe_.amplitude, 0.01f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::DragFloat("周期 (秒)", &recipe_.period, 0.01f, 0.05f, 3600.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    } else {
        const char *label = recipe_.kind == ShortText::Kind::Rotate ? "速度 (度/秒)" : "速度 (単位/秒)";
        changed |= ImGui::DragFloat(label, &recipe_.speed, 0.1f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        bool reverse = recipe_.direction < 0;
        if (ImGui::Checkbox("逆方向", &reverse)) { recipe_.direction = reverse ? -1 : 1; changed = true; }
    }
    if (changed) elapsed_ = 0;
}
#endif
} // namespace KashipanEngine

