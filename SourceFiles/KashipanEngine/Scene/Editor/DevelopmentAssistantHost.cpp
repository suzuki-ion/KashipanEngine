#include "Scene/SceneEditor.h"
#ifdef USE_IMGUI
#include "Scene/Editor/DevelopmentAssistantApi.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/SceneObjectHierarchy.h"
#include "Scene/Editor/SceneEditorView.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneFileIO.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Input/EditorTestInput.h"
#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include <json.hpp>
#include <fstream>
#include <set>

namespace KashipanEngine {
namespace {
using J = nlohmann::json;
void Check(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
std::string Base64(const std::string &data) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output; output.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        const unsigned value = (static_cast<unsigned char>(data[i]) << 16) | (i + 1 < data.size() ? static_cast<unsigned char>(data[i + 1]) << 8 : 0) | (i + 2 < data.size() ? static_cast<unsigned char>(data[i + 2]) : 0);
        output += alphabet[(value >> 18) & 63]; output += alphabet[(value >> 12) & 63];
        output += i + 1 < data.size() ? alphabet[(value >> 6) & 63] : '='; output += i + 2 < data.size() ? alphabet[value & 63] : '=';
    }
    return output;
}
}
std::string SceneEditor::CallAIEditorApi(const std::string &tool, const std::string &arguments) {
    const auto ids = objectHierarchy_->GetSelectedObjectIDs();
    EditorApiServices services{commands_.get(), [this](const auto &name, const auto &args) { return ExecuteAIEditorTool(name, args); }};
    return ExecuteEditorApi(context_, ids.size() == 1 ? ids.front() : UUID128{},
        [this](auto command) { return commands_->Execute(std::move(command)); }, tool, arguments, &services);
}
std::string SceneEditor::ExecuteAIEditorTool(const std::string &tool, const std::string &arguments) {
    try {
        const auto args = J::parse(arguments); J result{{"ok", true}, {"undoable", false}};
        const auto action = args.value("action", std::string{});
        auto selection = [&] { J ids = J::array(); for (const auto &id : objectHierarchy_->GetSelectedObjectIDs()) ids.push_back(id.ToString()); return ids; };
        if (tool == "selection") {
            if (action == "clear") objectHierarchy_->ClearSelection();
            else if (action == "set") {
                const auto &ids = args.at("ids"); Check(ids.is_array() && ids.size() <= 200, "Invalid selection size");
                std::vector<UUID128> valid;
                for (const auto &value : ids) { const UUID128 id(value.get<std::string>()); Check(id.IsValid() && context_->GetSceneObject(id), "Selection contains missing object"); valid.push_back(id); }
                objectHierarchy_->RestoreSelection(valid);
            } else Check(action == "get", "Unknown selection action");
            result["ids"] = selection();
        } else if (tool == "runtime_status" || tool == "play") {
            if (tool == "play") {
                if (action == "start") { Check(!context_->IsPlaying(), "Already playing"); objectHierarchy_->ClearSelection(); commands_->BeginPlaySession(); context_->PlayStart(); }
                else if (action == "stop") { Check(context_->IsPlaying(), "Not playing"); aiStepFrames_ = 0; EditorTestInput::Clear(); context_->PlayStop(); commands_->EndPlaySession(); objectHierarchy_->ClearSelection(); }
                else if (action == "pause") { Check(context_->IsPlaying(), "Not playing"); context_->PlayPause(); }
                else if (action == "resume") { Check(context_->IsPlaying(), "Not playing"); aiStepFrames_ = 0; context_->PlayResume(); }
                else if (action == "step" || action == "run_frames") {
                    Check(context_->IsPlaying() && context_->IsPaused(), "Pause Play before stepping");
                    const int frames = action == "step" ? 1 : args.at("frames").get<int>(); Check(frames >= 1 && frames <= 600, "Invalid frame count");
                    aiStepFrames_ = static_cast<unsigned>(frames); context_->RequestStepFrame(); result["queued"] = true;
                } else Check(action == "status", "Unknown Play action");
            }
            result["playing"] = context_->IsPlaying(); result["paused"] = context_->IsPaused(); result["editor_frame"] = aiFrame_; result["pending_steps"] = aiStepFrames_; result["selection"] = selection();
        } else if (tool == "view") {
            Check(sceneView_ != nullptr, "Scene view unavailable");
            if (action == "set") Check(sceneView_->SetApiCamera(args.at("settings")), "Invalid camera settings");
            else if (action == "focus") {
                auto *object = context_->GetSceneObject(UUID128(args.at("id").get<std::string>())); Check(object, "Focus target missing");
                const auto position = object->GetComponent<Transform>()->GetWorldPosition();
                Check(sceneView_->SetApiCamera({{"target", ToJSON(position)}, {"pan_2d", ToJSON(Vector2(position.x, position.y))}, {"fly", false}}), "Focus failed");
            } else Check(action == "get", "Unknown view action");
            result["settings"] = sceneView_->GetApiCamera();
        } else if (tool == "editor_window") {
            const auto name = args.at("name").get<std::string>();
            const std::map<std::string, bool *> windows{{"scene", &isShowSceneView_}, {"hierarchy", &isShowHierarchy_}, {"inspector", &isShowObjectInspector_},
                {"scene_components", &isShowComponentInspector_}, {"assets", &isShowAssets_}, {"history", &isShowHistory_}, {"logger", &isShowLoggerWindow_}, {"scene_list", &isShowSceneList_}, {"input", &isShowInputStateWindow_}};
            Check(windows.contains(name), "Unknown editor panel"); *windows.at(name) = args.at("visible").get<bool>();
        } else if (tool == "scene_files") {
            auto *manager = context_->GetSceneManager();
            if (action == "list") {
                result["current"] = context_->GetName(); result["path"] = GetCurrentSceneFilePath(); result["unsaved"] = HasUnsavedSceneChanges(); result["scenes"] = J::array();
                if (manager) for (const auto &entry : manager->GetRegisteredScenes()) result["scenes"].push_back({{"name", entry.name}, {"path", entry.filePath}});
            } else if (action == "save") {
                const auto logical = args.value("path", GetCurrentSceneFilePath());
                const auto path = EditorApiProjectPath(ProjectPaths::ToLogical(logical));
                const auto extension = Utf8StringToPath(path).extension(); Check(extension == L".json" || extension == L".scene", "Scene path must end in .json or .scene");
                bool currentPath = false;
                const auto current = GetCurrentSceneFilePath();
                if (!current.empty()) currentPath = Utf8StringToPath(ProjectPaths::ToPhysical(current)).lexically_normal() == Utf8StringToPath(path).lexically_normal();
                Check(!std::filesystem::exists(Utf8StringToPath(path)) || currentPath || args.value("overwrite", false), "Destination scene exists; overwrite=true is required");
                const auto state = args.value("runtime", false) ? context_->SaveSceneToJSON() : GetEditableSceneJSON();
                Check(SaveSceneToPath(state, path), "Scene save failed"); RecordInternalSceneSave(path);
                if (!args.value("runtime", false)) { savedSceneSnapshot_ = state; hasSavedSceneSnapshot_ = true; }
                if (manager && manager->IsSceneRegistered(context_->GetName())) manager->SetRegisteredSceneFilePath(context_->GetName(), ProjectPaths::ToLogical(path));
                result["path"] = path;
            } else {
                Check(!context_->IsPlaying(), "Stop Play before replacing a scene");
                Check(!HasUnsavedSceneChanges() || args.value("discard_unsaved", false), "Scene has unsaved edits; save first or explicitly allow discard_unsaved");
                aiStepFrames_ = 0; EditorTestInput::Clear();
                if (action == "switch") { Check(manager && manager->ChangeScene(args.at("name").get<std::string>()), "Cannot switch to unregistered scene"); result["queued"] = true; result["will_rebind"] = true; result["target_scene"] = args.at("name"); }
                else {
                    J next;
                    if (action == "new") { const auto name = args.at("name").get<std::string>(); Check(!name.empty() && name.size() <= 256, "Invalid scene name"); next = {{"sceneName", name}, {"sceneID", UUID128(true).ToString()}, {"sceneObjects", J::array()}, {"sceneComponents", J::array()}}; }
                    else { Check(action == "load", "Unknown scene action"); const auto path = EditorApiProjectPath(args.at("path").get<std::string>()); next = LoadSceneFromPath(path); Check(next.is_object() && next.contains("sceneName"), "Scene load failed"); }
                    Check(next.value("sceneObjects", J::array()).size() <= 8192 && next.value("sceneComponents", J::array()).size() <= 256, "Scene exceeds API limits");
                    const auto previous = context_->SaveSceneToJSON();
                    if (!context_->LoadSceneFromJSON(next)) {
                        const bool restored = context_->LoadSceneFromJSON(previous);
                        commands_->Clear(); objectHierarchy_->ClearSelection();
                        throw std::runtime_error(restored ? "Scene replacement failed; previous scene restored and Undo history cleared" : "Scene replacement failed and restoration failed; inspect scene before continuing");
                    }
                    commands_->Clear(); objectHierarchy_->ClearSelection(); savedSceneSnapshot_ = context_->SaveSceneToJSON(); hasSavedSceneSnapshot_ = true;
                    result["scene"] = context_->GetSceneID().ToString();
                }
            }
        } else if (tool == "asset") {
            Check(action == "reload", "Unknown asset host action");
            const auto path = EditorApiProjectPath(args.at("path").get<std::string>());
            Check(std::filesystem::is_regular_file(Utf8StringToPath(path)), "Asset file missing");
            ProcessNonSceneExternalChanges({ProjectPaths::ToLogical(path)}); result["reloaded"] = path;
        } else if (tool == "capture") {
            const auto logical = args.value("path", std::string("Assets/AI_Captures/") + UUID128(true).ToString() + ".png");
            const auto path = EditorApiProjectPath(logical); Check(Utf8StringToPath(path).extension() == L".png", "Capture requires .png");
            Check(!std::filesystem::exists(Utf8StringToPath(path)), "Capture path already exists"); std::filesystem::create_directories(Utf8StringToPath(path).parent_path());
            bool saved = false;
            if (args.contains("id")) {
                auto *object = context_->GetSceneObject(UUID128(args.at("id").get<std::string>())); Check(object, "Capture target missing");
                auto *component = dynamic_cast<ScreenBufferObject *>(context_->ResolveComponent({object->GetObjectID(), args.at("component").get<size_t>()}));
                Check(component, "Expected ScreenBufferObject"); saved = component->RequestSave(path);
            } else saved = sceneView_ && sceneView_->SaveApiImage(path);
            Check(saved, "No rendered frame or graphics device available"); const auto size = std::filesystem::file_size(Utf8StringToPath(path));
            result["path"] = logical;
            if (size <= 2 * 1024 * 1024) { std::ifstream stream(Utf8StringToPath(path), std::ios::binary); const std::string bytes{std::istreambuf_iterator<char>(stream), {}};
                result["_mcp_images"] = J::array({{{"type", "image"}, {"mimeType", "image/png"}, {"data", Base64(bytes)}}}); }
            else { result["image_omitted"] = "PNG exceeds 2 MiB; image was saved, reduce render-buffer resolution to inspect it through MCP"; }
        } else if (tool == "api_cancel") { aiStepFrames_ = 0; EditorTestInput::Clear(); }
        else throw std::runtime_error("Unknown editor host operation");
        return result.dump();
    } catch (const std::exception &e) { return J{{"ok", false}, {"error", e.what()}}.dump(); }
}
}
#endif
