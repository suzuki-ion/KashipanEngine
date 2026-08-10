#include "AssetEditorWindows.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <unordered_map>

#include "Assets/MaterialManager.h"
#include "Core/ProjectPaths.h"
#include "Assets/TextureManager.h"
#include "Graphics/IShaderTexture.h"
#include "Graphics/ScreenBuffer.h"
#include "Math/Quaternion.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Transform.h"
#include "Scene/Editor/PrefabAssetManager.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Scene/SceneContext.h"
#include "Utilities/Translation.h"

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

/// @brief モデルの全頂点から求めたAABB
struct ModelBounds final {
    Vector3 min{ 0.0f, 0.0f, 0.0f };
    Vector3 max{ 0.0f, 0.0f, 0.0f };
    bool valid = false;
};
ModelBounds ComputeModelBounds(const ModelData &data) {
    ModelBounds bounds;
    for (const auto &v : data.GetVertices()) {
        const Vector3 p(v.px, v.py, v.pz);
        if (!bounds.valid) {
            bounds.min = bounds.max = p;
            bounds.valid = true;
            continue;
        }
        bounds.min.x = std::min(bounds.min.x, p.x);
        bounds.min.y = std::min(bounds.min.y, p.y);
        bounds.min.z = std::min(bounds.min.z, p.z);
        bounds.max.x = std::max(bounds.max.x, p.x);
        bounds.max.y = std::max(bounds.max.y, p.y);
        bounds.max.z = std::max(bounds.max.z, p.z);
    }
    return bounds;
}

constexpr std::uint32_t kModelPreviewBufferSize = 512;
} // namespace

//==================================================
// JSONFileEditorWindow
//==================================================

JSONFileEditorWindow::JSONFileEditorWindow(const std::string &filePath)
    : filePath_(filePath) {
    windowTitle_ = Translation("editor.assetwindow.json.title") + FileNameFromPath(filePath_) + "###JSONFileEditor_" + filePath_;
    data_ = LoadJSON(ProjectPaths::ToPhysical(filePath_));
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
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", TranslationC("editor.assetwindow.json.parsefailed"));
    }
    ImGui::TextUnformatted(filePath_.c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(!isDirty_);
    if (ImGui::Button(TranslationLabel("editor.common.save"))) {
        // .prefabファイルの場合はPrefabAssetManager経由で保存し、他インスタンスへの伝播をトリガーする
        const bool isPrefab = std::filesystem::path(filePath_).extension() == PrefabUtility::kPrefabExtension;
        const bool saved = isPrefab
            ? PrefabAssetManager::SavePrefabJsonByPath(filePath_, data_)
            : SaveJSON(data_, ProjectPaths::ToPhysical(filePath_));
        if (saved) {
            isDirty_ = false;
        }
    }
    ImGui::EndDisabled();
    if (isDirty_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", TranslationC("editor.assetwindow.unsaved"));
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
    if (ImGui::Button(TranslationLabel("editor.assetwindow.json.addmember"))) {
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
    if (ImGui::Button(TranslationLabel("editor.assetwindow.json.addelement"))) {
        arrayNode.push_back(MakeDefaultJSONValue(static_cast<JSONNewValueType>(newType)));
        isDirty_ = true;
    }
}

//==================================================
// MaterialFileEditorWindow
//==================================================

MaterialFileEditorWindow::MaterialFileEditorWindow(const std::string &assetPath)
    : assetPath_(assetPath) {
    windowTitle_ = Translation("editor.assetwindow.material.title") + FileNameFromPath(assetPath_) + "###MaterialFileEditor_" + assetPath_;
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
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s%s", TranslationC("editor.assetwindow.material.notloaded"), assetPath_.c_str());
        ImGui::End();
        return isOpen_;
    }

    ImGui::TextUnformatted(assetPath_.c_str());
    ImGui::Separator();

    MaterialManager::ShowMaterialEditorFields(*material);

    if (ImGui::Button(TranslationLabel("editor.common.save"))) {
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
    windowTitle_ = Translation("editor.assetwindow.image.title") + FileNameFromPath(assetPath_) + "###ImagePreview_" + assetPath_;
}

bool ImagePreviewWindow::ShowImGui() {
    ImGui::SetNextWindowSize(ImVec2(420.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(windowTitle_.c_str(), &isOpen_)) {
        ImGui::End();
        return isOpen_;
    }

    const auto textureHandle = TextureManager::GetTextureFromAssetPath(assetPath_);
    if (textureHandle == TextureManager::kInvalidHandle) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s%s", TranslationC("editor.assetwindow.texture.notloaded"), assetPath_.c_str());
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
    windowTitle_ = Translation("editor.assetwindow.audio.title") + FileNameFromPath(assetPath_) + "###AudioPreview_" + assetPath_;
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
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s%s", TranslationC("editor.assetwindow.sound.notloaded"), assetPath_.c_str());
        ImGui::End();
        return isOpen_;
    }

    ImGui::TextUnformatted(assetPath_.c_str());
    ImGui::Separator();

    const bool isPlaying = AudioManager::IsPlaying(playHandle_);
    const bool isPaused = AudioManager::IsPaused(playHandle_);

    if (!isPlaying) {
        if (ImGui::Button(TranslationLabel("editor.audio.play"))) {
            playHandle_ = AudioManager::Play(soundHandle_, volume_, 0.0f, false);
        }
    } else if (isPaused) {
        if (ImGui::Button(TranslationLabel("editor.audio.resume"))) {
            AudioManager::Resume(playHandle_);
        }
    } else {
        if (ImGui::Button(TranslationLabel("editor.audio.pause"))) {
            AudioManager::Pause(playHandle_);
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!isPlaying);
    if (ImGui::Button(TranslationLabel("editor.audio.stop"))) {
        AudioManager::Stop(playHandle_);
    }
    ImGui::EndDisabled();

    if (ImGui::SliderFloat(TranslationLabel("editor.audio.volume"), &volume_, 0.0f, 1.0f)) {
        if (isPlaying) AudioManager::SetVolume(playHandle_, volume_);
    }

    if (isPlaying) {
        double positionSec = 0.0;
        if (AudioManager::GetPlayPositionSeconds(playHandle_, positionSec)) {
            ImGui::Text("%s%.2f", TranslationC("editor.audio.position"), positionSec);
        }
    }

    ImGui::End();
    return isOpen_;
}

//==================================================
// VideoPreviewWindow
//==================================================

VideoPreviewWindow::VideoPreviewWindow(const std::string &assetPath)
    : assetPath_(assetPath) {
    windowTitle_ = Translation("editor.assetwindow.video.title") + FileNameFromPath(assetPath_) + "###VideoPreview_" + assetPath_;
    videoHandle_ = VideoManager::GetVideoHandleFromAssetPath(assetPath_);
}

VideoPreviewWindow::~VideoPreviewWindow() {
    if (player_) {
        VideoManager::DestroyPlayer(player_);
        player_ = nullptr;
    }
}

bool VideoPreviewWindow::ShowImGui() {
    ImGui::SetNextWindowSize(ImVec2(420.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(windowTitle_.c_str(), &isOpen_)) {
        ImGui::End();
        return isOpen_;
    }

    if (videoHandle_ == VideoManager::kInvalidHandle) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s%s", TranslationC("editor.assetwindow.video.notloaded"), assetPath_.c_str());
        ImGui::End();
        return isOpen_;
    }

    ImGui::TextUnformatted(assetPath_.c_str());
    ImGui::Separator();

    const bool isPlaying = player_ && player_->IsPlaying();
    const bool isPaused = player_ && player_->IsPaused();

    if (!isPlaying) {
        if (ImGui::Button(TranslationLabel("editor.video.play"))) {
            if (!player_) player_ = VideoManager::CreatePlayer(videoHandle_);
            if (player_) player_->Play(loop_, volume_);
        }
    } else if (isPaused) {
        if (ImGui::Button(TranslationLabel("editor.video.resume"))) {
            player_->Resume();
        }
    } else {
        if (ImGui::Button(TranslationLabel("editor.video.pause"))) {
            player_->Pause();
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!isPlaying && !isPaused);
    if (ImGui::Button(TranslationLabel("editor.video.stop"))) {
        if (player_) {
            VideoManager::DestroyPlayer(player_);
            player_ = nullptr;
        }
    }
    ImGui::EndDisabled();

    ImGui::Checkbox(TranslationLabel("editor.video.loop"), &loop_);
    ImGui::SliderFloat(TranslationLabel("editor.video.volume"), &volume_, 0.0f, 1.0f);

    if (player_) {
        const auto textureHandle = player_->GetTextureHandle();
        if (textureHandle != TextureManager::kInvalidHandle) {
            const auto view = TextureManager::GetTextureView(textureHandle);
            const D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = view.GetSrvHandle();
            const std::uint32_t width = view.GetWidth();
            const std::uint32_t height = view.GetHeight();
            if (srvHandle.ptr != 0 && width > 0 && height > 0) {
                const ImVec2 avail = ImGui::GetContentRegionAvail();
                const float scale = std::min(avail.x / static_cast<float>(width), avail.y / static_cast<float>(height));
                const ImVec2 drawSize(static_cast<float>(width) * (scale > 0.0f ? scale : 1.0f),
                    static_cast<float>(height) * (scale > 0.0f ? scale : 1.0f));
                ImGui::Image(static_cast<ImTextureID>(srvHandle.ptr), drawSize);
            }
        }
    }

    ImGui::End();
    return isOpen_;
}

//==================================================
// ModelPreviewWindow
//==================================================

ModelPreviewWindow::ModelPreviewWindow(const std::string &assetPath)
    : assetPath_(assetPath) {
    windowTitle_ = Translation("editor.assetwindow.model.title") + FileNameFromPath(assetPath_) + "###ModelPreview_" + assetPath_;
    modelHandle_ = ModelManager::GetModelHandleFromAssetPath(assetPath_);
}

bool ModelPreviewWindow::EnsurePreviewObjects(SceneContext *sceneContext) {
    if (!sceneContext) return false;
    if (objectsValid_) {
        if (sceneContext->GetSceneObject(targetObjectID_) && sceneContext->GetSceneObject(cameraObjectID_) &&
            sceneContext->GetSceneObject(lightObjectID_) && sceneContext->GetSceneObject(meshObjectID_)) {
            return true;
        }
        // シーン再読み込み等で一部が失われている場合は作り直す
        objectsValid_ = false;
    }
    if (sceneContext->IsPlaying()) return false;

    auto *targetObj = sceneContext->CreateEmptyObject("(Model Preview Target)");
    if (!targetObj) return false;
    targetObj->SetSaveEnabled(false);
    if (auto *screenBufferObject = targetObj->AddComponent<ScreenBufferObject>()) {
        screenBufferObject->SetName("ModelPreview");
        screenBufferObject->SetSize(kModelPreviewBufferSize, kModelPreviewBufferSize);
    }

    auto *cameraObj = sceneContext->CreateEmptyObject("(Model Preview Camera)");
    if (!cameraObj) { sceneContext->DeleteObject(targetObj); return false; }
    cameraObj->SetSaveEnabled(false);
    cameraObj->AddComponent<Camera3D>();
    if (auto *cameraRenderer = cameraObj->AddComponent<CameraRenderer>()) {
        cameraRenderer->SetTargetObject(targetObj);
    }

    auto *lightObj = sceneContext->CreateEmptyObject("(Model Preview Light)");
    if (!lightObj) { sceneContext->DeleteObject(cameraObj); sceneContext->DeleteObject(targetObj); return false; }
    lightObj->SetSaveEnabled(false);
    if (auto *transform = lightObj->GetComponent<Transform>()) {
        transform->SetRotate(Vector3(0.9f, -0.6f, 0.0f));
    }
    lightObj->AddComponent<Light>();
    if (auto *lightRenderer = lightObj->AddComponent<LightRenderer>()) {
        lightRenderer->SetTargetObject(targetObj);
    }

    auto *meshObj = sceneContext->CreateEmptyObject("(Model Preview Mesh)");
    if (!meshObj) { sceneContext->DeleteObject(lightObj); sceneContext->DeleteObject(cameraObj); sceneContext->DeleteObject(targetObj); return false; }
    meshObj->SetSaveEnabled(false);
    // 通常のScene Viewには映り込ませない（SceneRenderer::CollectSortableEntries参照）
    meshObj->SetHiddenFromEditorTarget(true);
    meshObj->AddComponent<MeshFilter>();
    if (auto *meshRenderer = meshObj->AddComponent<MeshRenderer>()) {
        meshRenderer->SetTargetObject(targetObj);
    }

    targetObjectID_ = targetObj->GetObjectID();
    cameraObjectID_ = cameraObj->GetObjectID();
    lightObjectID_ = lightObj->GetObjectID();
    meshObjectID_ = meshObj->GetObjectID();
    objectsValid_ = true;
    applied_ = false;
    return true;
}

void ModelPreviewWindow::DestroyPreviewObjects(SceneContext *sceneContext) {
    if (sceneContext && objectsValid_) {
        for (const UUID128 id : { meshObjectID_, lightObjectID_, cameraObjectID_, targetObjectID_ }) {
            if (auto *obj = sceneContext->GetSceneObject(id)) sceneContext->DeleteObject(obj);
        }
    }
    objectsValid_ = false;
    applied_ = false;
}

void ModelPreviewWindow::ApplyModelToPreview(SceneContext *sceneContext) {
    if (!sceneContext || !objectsValid_) return;

    auto *meshObj = sceneContext->GetSceneObject(meshObjectID_);
    auto *meshFilter = meshObj ? meshObj->GetComponent<MeshFilter>() : nullptr;
    auto *meshRenderer = meshObj ? meshObj->GetComponent<MeshRenderer>() : nullptr;
    if (!meshFilter || !meshRenderer) return;

    meshFilter->SetMeshHandle(modelHandle_);
    applied_ = true;

    if (modelHandle_ == ModelManager::kInvalidHandle) {
        target_ = Vector3(0.0f, 0.0f, 0.0f);
        distance_ = 5.0f;
        return;
    }

    const ModelData &data = ModelManager::GetModelData(modelHandle_);

    // サブメッシュごとに、インポート時に自動生成された本来のマテリアルを割り当てる
    // （見つからない場合はMeshRendererの既定"Default"マテリアルのまま）
    const auto &subMeshes = data.GetSubMeshes();
    const size_t subMeshCount = std::max<size_t>(1, subMeshes.size());
    meshRenderer->SetMaterialSlotCount(subMeshCount);
    for (size_t i = 0; i < subMeshCount; ++i) {
        const std::uint32_t materialIndex = subMeshes.empty() ? 0u : subMeshes[i].materialIndex;
        const auto *material = data.GetMaterial(materialIndex);
        const std::string materialAssetName = (material && !material->materialAssetName.empty())
            ? material->materialAssetName : std::string("Default");
        meshRenderer->SetMaterialNameAt(i, materialAssetName);
    }

    // モデル全体が収まるようカメラの注視点・距離を自動フィットさせる
    const ModelBounds bounds = ComputeModelBounds(data);
    auto *cameraObj = sceneContext->GetSceneObject(cameraObjectID_);
    auto *camera3d = cameraObj ? cameraObj->GetComponent<Camera3D>() : nullptr;
    const float fovY = camera3d ? camera3d->GetFovY() : 0.45f;
    if (bounds.valid) {
        target_ = (bounds.min + bounds.max) * 0.5f;
        const float radius = std::max((bounds.max - bounds.min).Length() * 0.5f, 0.01f);
        const float halfFov = std::max(fovY * 0.5f, 0.05f);
        distance_ = radius / std::sin(halfFov) * 1.3f;
    } else {
        target_ = Vector3(0.0f, 0.0f, 0.0f);
        distance_ = 5.0f;
    }
    yaw_ = 0.6f;
    pitch_ = 0.35f;
}

void ModelPreviewWindow::UpdateCameraTransform(SceneContext *sceneContext) {
    if (!sceneContext || !objectsValid_) return;
    auto *cameraObj = sceneContext->GetSceneObject(cameraObjectID_);
    auto *transform = cameraObj ? cameraObj->GetComponent<Transform>() : nullptr;
    if (!transform) return;

    Matrix4x4 rotateX;
    rotateX.MakeRotateX(pitch_);
    Matrix4x4 rotateY;
    rotateY.MakeRotateY(yaw_);
    const Matrix4x4 rotation = rotateX * rotateY;
    const Vector3 forward(rotation.m[2][0], rotation.m[2][1], rotation.m[2][2]);
    const Vector3 eye = target_ - forward * distance_;

    transform->SetTranslate(eye);
    transform->SetRotateQuaternion(Quaternion::MakeFromRotationMatrix(rotation));
}

void ModelPreviewWindow::HandleCameraInput() {
    if (!ImGui::IsItemHovered()) return;
    ImGuiIO &io = ImGui::GetIO();
    if (io.MouseWheel != 0.0f) {
        distance_ *= std::pow(0.9f, io.MouseWheel);
        distance_ = std::clamp(distance_, 0.01f, 10000.0f);
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        yaw_ += io.MouseDelta.x * 0.005f;
        pitch_ += io.MouseDelta.y * 0.005f;
        pitch_ = std::clamp(pitch_, -1.55f, 1.55f);
    }
}

bool ModelPreviewWindow::ShowImGui(SceneContext *sceneContext) {
    ImGui::SetNextWindowSize(ImVec2(420.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(windowTitle_.c_str(), &isOpen_)) {
        ImGui::End();
        if (!isOpen_) DestroyPreviewObjects(sceneContext);
        return isOpen_;
    }

    if (modelHandle_ == ModelManager::kInvalidHandle) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s%s", TranslationC("editor.assetwindow.model.notloaded"), assetPath_.c_str());
        ImGui::End();
        if (!isOpen_) DestroyPreviewObjects(sceneContext);
        return isOpen_;
    }

    ImGui::TextUnformatted(assetPath_.c_str());

    if (!sceneContext || sceneContext->IsPlaying()) {
        ImGui::TextUnformatted(TranslationC("editor.modelmanager.preview.unavailable_playing"));
        ImGui::End();
        if (!isOpen_) DestroyPreviewObjects(sceneContext);
        return isOpen_;
    }

    if (!EnsurePreviewObjects(sceneContext)) {
        ImGui::TextUnformatted(TranslationC("editor.modelmanager.preview.unavailable"));
        ImGui::End();
        if (!isOpen_) DestroyPreviewObjects(sceneContext);
        return isOpen_;
    }
    if (!applied_) {
        ApplyModelToPreview(sceneContext);
    }
    UpdateCameraTransform(sceneContext);

    if (ImGui::Button(TranslationLabel("editor.modelmanager.preview.reset_view"))) {
        ApplyModelToPreview(sceneContext);
        UpdateCameraTransform(sceneContext);
    }
    ImGui::Separator();

    auto *targetObj = sceneContext->GetSceneObject(targetObjectID_);
    auto *screenBufferObject = targetObj ? targetObj->GetComponent<ScreenBufferObject>() : nullptr;
    auto *screenBuffer = screenBufferObject ? screenBufferObject->GetScreenBuffer() : nullptr;
    const auto srvHandle = screenBuffer ? screenBuffer->GetPreviewSrvHandle() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
    if (!screenBuffer || srvHandle.ptr == 0) {
        ImGui::TextUnformatted(TranslationC("component.screenbufferobject.srv_not_ready"));
        ImGui::End();
        if (!isOpen_) DestroyPreviewObjects(sceneContext);
        return isOpen_;
    }

    // アスペクト比を維持して表示領域にフィットさせる（ScreenBufferObject::ShowViewerWindowと同じ手法）
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float w = static_cast<float>(screenBuffer->GetWidth());
    const float h = static_cast<float>(screenBuffer->GetHeight());
    ImVec2 drawSize = avail;
    if (w > 0.0f && h > 0.0f && avail.x > 0.0f && avail.y > 0.0f) {
        const float scale = std::min(avail.x / w, avail.y / h);
        drawSize = ImVec2(w * scale, h * scale);
    }
    ImGui::Image(static_cast<ImTextureID>(srvHandle.ptr), drawSize);
    HandleCameraInput();

    ImGui::End();
    if (!isOpen_) DestroyPreviewObjects(sceneContext);
    return isOpen_;
}

} // namespace KashipanEngine

#endif // USE_IMGUI
