#include "Scene/SceneManager.h"
#include "Core/ProjectPaths.h"
#include "Debug/Logger.h"
#include "Scene/RenderTargetCarryOverRegistry.h"
#include "Scene/SceneFileIO.h"
#include "Utilities/FileIO/JSON.h"

#include <algorithm>

namespace KashipanEngine {

SceneManager::SceneEntry *SceneManager::FindEntry(const std::string &sceneName) {
    auto it = std::find_if(registeredScenes_.begin(), registeredScenes_.end(),
        [&sceneName](const SceneEntry &entry) { return entry.name == sceneName; });
    return (it != registeredScenes_.end()) ? &(*it) : nullptr;
}

const SceneManager::SceneEntry *SceneManager::FindEntry(const std::string &sceneName) const {
    auto it = std::find_if(registeredScenes_.begin(), registeredScenes_.end(),
        [&sceneName](const SceneEntry &entry) { return entry.name == sceneName; });
    return (it != registeredScenes_.end()) ? &(*it) : nullptr;
}

bool SceneManager::ChangeScene(const std::string &sceneName) {
    if (!FindEntry(sceneName)) return false;
    pendingSceneName_ = sceneName;
    hasPendingSceneChange_ = true;
    return true;
}

bool SceneManager::RegisterScene(const std::string &sceneName, const JSON &factoryData) {
    if (sceneName.empty()) return false;
    if (FindEntry(sceneName)) return false; // すでに登録されている場合は登録できない
    registeredScenes_.push_back(SceneEntry{ sceneName, std::string(), factoryData });
    return true;
}

bool SceneManager::RegisterSceneFile(const std::string &sceneName, const std::string &filePath) {
    if (sceneName.empty()) return false;
    if (FindEntry(sceneName)) return false; // すでに登録されている場合は登録できない
    registeredScenes_.push_back(SceneEntry{ sceneName, filePath, JSON() });
    return true;
}

bool SceneManager::UnregisterScene(const std::string &sceneName) {
    auto it = std::find_if(registeredScenes_.begin(), registeredScenes_.end(),
        [&sceneName](const SceneEntry &entry) { return entry.name == sceneName; });
    if (it == registeredScenes_.end()) return false;
    registeredScenes_.erase(it);
    if (startupSceneName_ == sceneName) startupSceneName_.clear();
    return true;
}

bool SceneManager::RenameRegisteredScene(const std::string &oldName, const std::string &newName) {
    if (newName.empty()) return false;
    if (oldName == newName) return true;
    if (FindEntry(newName)) return false; // 変更後の名前が既に使われている場合は変更できない
    auto *entry = FindEntry(oldName);
    if (!entry) return false;
    entry->name = newName;
    if (startupSceneName_ == oldName) startupSceneName_ = newName;
    if (pendingSceneName_ == oldName) pendingSceneName_ = newName;
    return true;
}

bool SceneManager::SetRegisteredSceneFilePath(const std::string &sceneName, const std::string &filePath) {
    auto *entry = FindEntry(sceneName);
    if (!entry) return false;
    entry->filePath = filePath;
    return true;
}

bool SceneManager::UpdateRegisteredSceneData(const std::string &sceneName, const JSON &factoryData) {
    auto *entry = FindEntry(sceneName);
    if (!entry) return false; // 登録されていない場合は更新できない
    entry->factoryData = factoryData;
    return true;
}

bool SceneManager::LoadSceneList(const std::string &filePath) {
    LogScope scope;
    JSON json = LoadJSON(ProjectPaths::ToPhysical(filePath));
    if (json.empty() || !json.is_object()) return false;

    if (json.contains("scenes") && json["scenes"].is_array()) {
        for (const auto &sceneJson : json["scenes"]) {
            if (!sceneJson.is_object()) continue;
            const std::string name = sceneJson.value("name", std::string());
            const std::string sceneFilePath = sceneJson.value("filePath", std::string());
            if (name.empty()) continue;
            if (!RegisterSceneFile(name, sceneFilePath)) {
                Log("SceneManager: Scene \"" + name + "\" is already registered. Skipped.", LogSeverity::Warning);
            }
        }
    }
    startupSceneName_ = json.value("startupScene", startupSceneName_);
    Log("SceneManager: Scene list loaded from " + filePath, LogSeverity::Info);
    return true;
}

bool SceneManager::SaveSceneList(const std::string &filePath) const {
    JSON json;
    json["startupScene"] = startupSceneName_;
    json["scenes"] = JSON::array();
    for (const auto &entry : registeredScenes_) {
        JSON sceneJson;
        sceneJson["name"] = entry.name;
        sceneJson["filePath"] = entry.filePath;
        json["scenes"].push_back(sceneJson);
    }
    return SaveJSON(json, ProjectPaths::ToPhysical(filePath));
}

void SceneManager::Update(Passkey<GameEngine>) {
    if (currentScene_) {
        currentScene_->UpdateInterface(Passkey<SceneManager>());
    }
}

#ifdef USE_IMGUI
void SceneManager::ShowImGui(Passkey<GameEngine>) {
    if (currentScene_) {
        currentScene_->ShowImGuiInterface(Passkey<SceneManager>());
    }
}
#endif // USE_IMGUI

bool SceneManager::CommitPendingSceneChange(Passkey<GameEngine>) {
    if (!hasPendingSceneChange_) return false;
    hasPendingSceneChange_ = false;

    const SceneEntry *entry = FindEntry(pendingSceneName_);
    if (!entry) {
        pendingSceneName_.clear();
        return false;
    }

    // 描画先コンポーネント（ScreenBufferObject/NormalWindowObject等）が、
    // 破棄されるバッファ/ウィンドウを次のシーンの同名コンポーネントへ引き継げるようにする
    RenderTargetCarryOverRegistry::BeginSceneSwitch(Passkey<SceneManager>{});

    // 変更前のシーンを終了処理して破棄する
    if (currentScene_) {
        currentScene_->FinalizeInterface(Passkey<SceneManager>());
        currentScene_.reset();
    }

    // 新しいシーンを作成する
    if (!entry->filePath.empty()) {
        // ファイルパス登録の場合は切り替えのたびに最新のファイル内容を読み込む
        JSON sceneData = LoadSceneFromPath(entry->filePath);
        if (!sceneData.empty()) {
            currentScene_ = std::make_unique<Scene>(sceneData);
        } else {
            LogScope scope;
            Log("SceneManager: Failed to load scene file \"" + entry->filePath + "\". Creating an empty scene.", LogSeverity::Warning);
            currentScene_ = std::make_unique<Scene>(pendingSceneName_);
        }
    } else if (!entry->factoryData.empty()) {
        currentScene_ = std::make_unique<Scene>(entry->factoryData);
    } else {
        // ファクトリデータが空の場合は、シーン名だけでシーンを作成する
        currentScene_ = std::make_unique<Scene>(pendingSceneName_);
    }

    if (currentScene_) {
        currentScene_->SetSceneManager(Passkey<SceneManager>(), this);
        currentScene_->InitializeInterface(Passkey<SceneManager>());
    }

    // 新しいシーン側で引き取られなかった描画先リソースをここで実際に破棄する
    RenderTargetCarryOverRegistry::EndSceneSwitch(Passkey<SceneManager>{});

    pendingSceneName_.clear();
    return true;
}

MyAny *SceneManager::AddGlobalSceneVariable(const std::string &key, const MyAny &value, const TypeInfo &typeInfo) {
    globalSceneVariables_.emplace(key, MyAny(value, typeInfo));
    return &globalSceneVariables_[key];
}

MyAny *SceneManager::GetGlobalSceneVariable(const std::string &key) {
    auto it = globalSceneVariables_.find(key);
    return (it != globalSceneVariables_.end()) ? &it->second : nullptr;
}

const TypeInfo &SceneManager::GetGlobalSceneVariableTypeInfo(const std::string &key) {
    static TypeInfo noneType(ValueType::None);
    auto it = globalSceneVariables_.find(key);
    return (it != globalSceneVariables_.end()) ? it->second.GetTypeInfo() : noneType;
}

} // namespace KashipanEngine
