#include "AssetEditorWindows.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <unordered_map>

#include "Assets/MaterialManager.h"
#include "Assets/TextureManager.h"
#include "Graphics/IShaderTexture.h"
#include "Scene/Editor/PrefabAssetManager.h"
#include "Scene/Editor/PrefabUtility.h"

namespace KashipanEngine {

namespace {
enum class JSONNewValueType {
    String,
    Number,
    Bool,
    Object,
    Array,
    Null,
};
constexpr const char *kJSONTypeNames[] = { "String", "Number", "Bool", "Object", "Array", "Null" };

JSON MakeDefaultJSONValue(JSONNewValueType type) {
    switch (type) {
    case JSONNewValueType::String: return JSON("");
    case JSONNewValueType::Number: return JSON(0.0);
    case JSONNewValueType::Bool:   return JSON(false);
    case JSONNewValueType::Object: return JSON::object();
    case JSONNewValueType::Array:  return JSON::array();
    case JSONNewValueType::Null:
    default:                      return JSON(nullptr);
    }
}

std::string FileNameFromPath(const std::string &path) {
    return std::filesystem::path(path).filename().string();
}
} // namespace

//==================================================
// JSONFileEditorWindow
//==================================================

JSONFileEditorWindow::JSONFileEditorWindow(const std::string &filePath)
    : filePath_(filePath) {
    windowTitle_ = "JSON: " + FileNameFromPath(filePath_) + "###JSONFileEditor_" + filePath_;
    data_ = LoadJSON(filePath_);
    loadFailed_ = data_.is_discarded();
    if (loadFailed_) data_ = JSON::object();
}

bool JSONFileEditorWindow::ShowImGui() {
    ImGui::SetNextWindowSize(ImVec2(420.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(windowTitle_.c_str(), &isOpen_)) {
        ImGui::End();
        return isOpen_;
    }

    if (loadFailed_) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Failed to parse JSON file.");
    }
    ImGui::TextUnformatted(filePath_.c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(!isDirty_);
    if (ImGui::Button("Save")) {
        // .prefabファイルの場合はPrefabAssetManager経由で保存し、他インスタンスへの伝播をトリガーする
        const bool isPrefab = std::filesystem::path(filePath_).extension() == PrefabUtility::kPrefabExtension;
        const bool saved = isPrefab
            ? PrefabAssetManager::SavePrefabJsonByPath(filePath_, data_)
            : SaveJSON(data_, filePath_);
        if (saved) {
            isDirty_ = false;
        }
    }
    ImGui::EndDisabled();
    if (isDirty_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "(unsaved)");
    }
    ImGui::Separator();

    ImGui::BeginChild("JSONFileEditorTree");
    ShowNode(FileNameFromPath(filePath_), data_);
    ImGui::EndChild();

    ImGui::End();
    return isOpen_;
}

void JSONFileEditorWindow::ShowNode(const std::string &label, JSON &node) {
    switch (node.type()) {
    case JSON::value_t::object: {
        const std::string header = label + " {" + std::to_string(node.size()) + "}";
        if (ImGui::TreeNode(header.c_str())) {
            std::string pendingErase;
            std::pair<std::string, std::string> pendingRename;
            for (auto it = node.begin(); it != node.end(); ++it) {
                const std::string key = it.key();
                ImGui::PushID(key.c_str());
                if (ImGui::SmallButton("x")) pendingErase = key;
                ImGui::SameLine();

                std::string keyBuf = key;
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputText("##key", &keyBuf, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (!keyBuf.empty() && keyBuf != key && !node.contains(keyBuf)) {
                        pendingRename = { key, keyBuf };
                    }
                }
                ImGui::SameLine();
                ShowNode(key, it.value());
                ImGui::PopID();
            }
            if (!pendingErase.empty()) {
                node.erase(pendingErase);
                isDirty_ = true;
            }
            if (!pendingRename.first.empty()) {
                node[pendingRename.second] = node[pendingRename.first];
                node.erase(pendingRename.first);
                isDirty_ = true;
            }
            ShowAddMemberRow(node);
            ImGui::TreePop();
        }
        break;
    }
    case JSON::value_t::array: {
        const std::string header = label + " [" + std::to_string(node.size()) + "]";
        if (ImGui::TreeNode(header.c_str())) {
            int eraseIndex = -1;
            for (size_t i = 0; i < node.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::SmallButton("x")) eraseIndex = static_cast<int>(i);
                ImGui::SameLine();
                ShowNode("[" + std::to_string(i) + "]", node[i]);
                ImGui::PopID();
            }
            if (eraseIndex >= 0) {
                node.erase(node.begin() + eraseIndex);
                isDirty_ = true;
            }
            ShowAddElementRow(node);
            ImGui::TreePop();
        }
        break;
    }
    case JSON::value_t::string: {
        std::string value = node.get<std::string>();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText(label.c_str(), &value)) {
            node = value;
            isDirty_ = true;
        }
        break;
    }
    case JSON::value_t::number_integer: {
        std::int64_t value = node.get<std::int64_t>();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputScalar(label.c_str(), ImGuiDataType_S64, &value)) {
            node = value;
            isDirty_ = true;
        }
        break;
    }
    case JSON::value_t::number_unsigned: {
        std::uint64_t value = node.get<std::uint64_t>();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputScalar(label.c_str(), ImGuiDataType_U64, &value)) {
            node = value;
            isDirty_ = true;
        }
        break;
    }
    case JSON::value_t::number_float: {
        double value = node.get<double>();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputDouble(label.c_str(), &value)) {
            node = value;
            isDirty_ = true;
        }
        break;
    }
    case JSON::value_t::boolean: {
        bool value = node.get<bool>();
        if (ImGui::Checkbox(label.c_str(), &value)) {
            node = value;
            isDirty_ = true;
        }
        break;
    }
    case JSON::value_t::null:
    default: {
        ImGui::TextUnformatted((label + ": null").c_str());
        break;
    }
    }
}

void JSONFileEditorWindow::ShowAddMemberRow(JSON &objectNode) {
    static std::unordered_map<ImGuiID, std::string> sNewKeyBuffers;
    static std::unordered_map<ImGuiID, int> sNewTypeBuffers;
    const ImGuiID rowID = ImGui::GetID("##addMember");
    std::string &newKey = sNewKeyBuffers[rowID];
    int &newType = sNewTypeBuffers[rowID];

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##newKey", &newKey);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("##newType", &newType, kJSONTypeNames, IM_ARRAYSIZE(kJSONTypeNames));
    ImGui::SameLine();
    ImGui::BeginDisabled(newKey.empty() || objectNode.contains(newKey));
    if (ImGui::Button("+ Add")) {
        objectNode[newKey] = MakeDefaultJSONValue(static_cast<JSONNewValueType>(newType));
        newKey.clear();
        isDirty_ = true;
    }
    ImGui::EndDisabled();
}

void JSONFileEditorWindow::ShowAddElementRow(JSON &arrayNode) {
    static std::unordered_map<ImGuiID, int> sNewTypeBuffers;
    const ImGuiID rowID = ImGui::GetID("##addElement");
    int &newType = sNewTypeBuffers[rowID];

    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("##newType", &newType, kJSONTypeNames, IM_ARRAYSIZE(kJSONTypeNames));
    ImGui::SameLine();
    if (ImGui::Button("+ Add Element")) {
        arrayNode.push_back(MakeDefaultJSONValue(static_cast<JSONNewValueType>(newType)));
        isDirty_ = true;
    }
}

//==================================================
// MaterialFileEditorWindow
//==================================================

MaterialFileEditorWindow::MaterialFileEditorWindow(const std::string &assetPath)
    : assetPath_(assetPath) {
    windowTitle_ = "Material: " + FileNameFromPath(assetPath_) + "###MaterialFileEditor_" + assetPath_;
}

bool MaterialFileEditorWindow::ShowImGui() {
    ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(windowTitle_.c_str(), &isOpen_)) {
        ImGui::End();
        return isOpen_;
    }

    // このマテリアルファイルに対応する読み込み済みマテリアルを探す（起動時に自動で読み込まれている前提）
    MaterialManager::Material *material = nullptr;
    for (const auto &entry : MaterialManager::GetLoadedMaterialListEntries()) {
        if (entry.assetPath == assetPath_) {
            material = MaterialManager::GetMaterial(entry.material.name);
            break;
        }
    }

    if (!material) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Material is not loaded: %s", assetPath_.c_str());
        ImGui::End();
        return isOpen_;
    }

    ImGui::TextUnformatted(assetPath_.c_str());
    ImGui::Separator();

    MaterialManager::ShowMaterialEditorFields(*material);

    if (ImGui::Button("Save")) {
        MaterialManager::SaveMaterial(MaterialManager::GetMaterialHandleFromName(material->name));
    }

    ImGui::End();
    return isOpen_;
}

//==================================================
// ImagePreviewWindow
//==================================================

ImagePreviewWindow::ImagePreviewWindow(const std::string &assetPath)
    : assetPath_(assetPath) {
    windowTitle_ = "Image: " + FileNameFromPath(assetPath_) + "###ImagePreview_" + assetPath_;
}

bool ImagePreviewWindow::ShowImGui() {
    ImGui::SetNextWindowSize(ImVec2(420.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(windowTitle_.c_str(), &isOpen_)) {
        ImGui::End();
        return isOpen_;
    }

    const auto textureHandle = TextureManager::GetTextureFromAssetPath(assetPath_);
    if (textureHandle == TextureManager::kInvalidHandle) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Texture is not loaded: %s", assetPath_.c_str());
        ImGui::End();
        return isOpen_;
    }

    const auto view = TextureManager::GetTextureView(textureHandle);
    const D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = view.GetSrvHandle();
    const std::uint32_t width = view.GetWidth();
    const std::uint32_t height = view.GetHeight();

    ImGui::Text("%s (%u x %u)", assetPath_.c_str(), width, height);
    ImGui::Separator();

    if (srvHandle.ptr != 0 && width > 0 && height > 0) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float scale = std::min(avail.x / static_cast<float>(width), avail.y / static_cast<float>(height));
        const ImVec2 drawSize(static_cast<float>(width) * (scale > 0.0f ? scale : 1.0f),
            static_cast<float>(height) * (scale > 0.0f ? scale : 1.0f));
        ImGui::Image(static_cast<ImTextureID>(srvHandle.ptr), drawSize);
    }

    ImGui::End();
    return isOpen_;
}

//==================================================
// AudioPreviewWindow
//==================================================

AudioPreviewWindow::AudioPreviewWindow(const std::string &assetPath)
    : assetPath_(assetPath) {
    windowTitle_ = "Audio: " + FileNameFromPath(assetPath_) + "###AudioPreview_" + assetPath_;
    soundHandle_ = AudioManager::GetSoundHandleFromAssetPath(assetPath_);
}

AudioPreviewWindow::~AudioPreviewWindow() {
    if (playHandle_ != AudioManager::kInvalidPlayHandle) {
        AudioManager::Stop(playHandle_);
    }
}

bool AudioPreviewWindow::ShowImGui() {
    ImGui::SetNextWindowSize(ImVec2(340.0f, 160.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(windowTitle_.c_str(), &isOpen_)) {
        ImGui::End();
        return isOpen_;
    }

    if (soundHandle_ == AudioManager::kInvalidSoundHandle) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Sound is not loaded: %s", assetPath_.c_str());
        ImGui::End();
        return isOpen_;
    }

    ImGui::TextUnformatted(assetPath_.c_str());
    ImGui::Separator();

    const bool isPlaying = AudioManager::IsPlaying(playHandle_);
    const bool isPaused = AudioManager::IsPaused(playHandle_);

    if (!isPlaying) {
        if (ImGui::Button("Play")) {
            playHandle_ = AudioManager::Play(soundHandle_, volume_, 0.0f, false);
        }
    } else if (isPaused) {
        if (ImGui::Button("Resume")) {
            AudioManager::Resume(playHandle_);
        }
    } else {
        if (ImGui::Button("Pause")) {
            AudioManager::Pause(playHandle_);
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!isPlaying);
    if (ImGui::Button("Stop")) {
        AudioManager::Stop(playHandle_);
    }
    ImGui::EndDisabled();

    if (ImGui::SliderFloat("Volume", &volume_, 0.0f, 1.0f)) {
        if (isPlaying) AudioManager::SetVolume(playHandle_, volume_);
    }

    if (isPlaying) {
        double positionSec = 0.0;
        if (AudioManager::GetPlayPositionSeconds(playHandle_, positionSec)) {
            ImGui::Text("Position: %.2f sec", positionSec);
        }
    }

    ImGui::End();
    return isOpen_;
}

} // namespace KashipanEngine

#endif // USE_IMGUI
