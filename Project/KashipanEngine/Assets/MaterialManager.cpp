#include "MaterialManager.h"
#include "Assets/CaseInsensitive.h"

#include "Debug/Logger.h"
#include "Utilities/Translation.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/AssetDragDropPayload.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(USE_IMGUI)
#include <imgui.h>
#include "Utilities/ImGuiCustom.h"
#endif

namespace KashipanEngine {

namespace {

using MaterialHandle = MaterialManager::MaterialHandle;
using MaterialEntry = MaterialManager::MaterialEntry;

std::string sAssetsRootPath;
std::unordered_map<MaterialHandle, MaterialEntry> sMaterials;
FileMap<MaterialHandle> sNameToHandle;
FileMap<MaterialHandle> sFileNameToHandle;
/// @brief 次に割り当てるマテリアルハンドル（削除済みハンドルは使い回さず単調増加させる）
MaterialHandle sNextHandle = 1;

#if defined(USE_IMGUI)
MaterialManager* sActiveInstance = nullptr;
#endif

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool HasSupportedMaterialExtension(const std::filesystem::path& p) {
    const std::string ext = ToLower(p.extension().string());
    return (ext == ".mat");
}

std::string MakeAssetRelativePath(const std::string& assetsRoot, const std::string& fullPath) {
    const std::filesystem::path root = Utf8StringToPath(assetsRoot);
    const std::filesystem::path full = Utf8StringToPath(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(PathToUtf8String(full.filename()));
    }
    return NormalizePathSlashes(PathToUtf8String(rel));
}

bool LoadMaterialFromJSON(const std::string& filePath, MaterialManager::Material& outMaterial) {
    try {
        JSON json = LoadJSON(filePath);
        if (!json.is_object()) return false;

        // マテリアル名
        if (json.contains("name") && json["name"].is_string()) {
            outMaterial.name = FromJSON<std::string>(json["name"]);
        } else {
            outMaterial.name = PathToUtf8String(Utf8StringToPath(filePath).stem());
        }

        // 色
        if (json.contains("color") && json["color"].is_object()) {
            outMaterial.color = FromJSON<Vector4>(json["color"]);
        }

        // UV トランスフォーム
        if (json.contains("uvTransform") && json["uvTransform"].is_object()) {
            outMaterial.uvTransform = FromJSON<Matrix4x4>(json["uvTransform"]);
        }

        // テクスチャハンドル
        // （この時点で対象テクスチャが存在しない場合はファイル名を保持しておき、
        //   ResolveTextureHandles によってハンドルが得られるまで解決を試み続ける）
        if (json.contains("textureFile") && json["textureFile"].is_string()) {
            outMaterial.textureFileName = FromJSON<std::string>(json["textureFile"]);
            outMaterial.textureHandle = TextureManager::GetTextureFromFileName(outMaterial.textureFileName);
        }

        if (json.contains("environmentFile") && json["environmentFile"].is_string()) {
            outMaterial.environmentFileName = FromJSON<std::string>(json["environmentFile"]);
            outMaterial.environmentHandle = TextureManager::GetTextureFromFileName(outMaterial.environmentFileName);
        }

        if (json.contains("normalMapFile") && json["normalMapFile"].is_string()) {
            outMaterial.normalMapFileName = FromJSON<std::string>(json["normalMapFile"]);
            outMaterial.normalMapHandle = TextureManager::GetTextureFromFileName(outMaterial.normalMapFileName);
        }

        // サンプラーハンドル
        if (json.contains("samplerHandle") && json["samplerHandle"].is_number_unsigned()) {
            outMaterial.samplerHandle = FromJSON<SamplerManager::SamplerHandle>(json["samplerHandle"]);
        }

        // 各種パラメータ
        outMaterial.shininess = FromJSON<float>(json.value("shininess", 32.0f));

        if (json.contains("specularColor") && json["specularColor"].is_object()) {
            outMaterial.specularColor = FromJSON<Vector4>(json["specularColor"]);
        }

        outMaterial.environmentCoefficient = FromJSON<float>(json["environmentCoefficient"]);
        outMaterial.enableLighting = FromJSON<bool>(json["enableLighting"]);
        outMaterial.enableShadowMapProjection = FromJSON<bool>(json["enableShadowMapProjection"]);

        if (json.contains("rimColor") && json["rimColor"].is_object()) {
            outMaterial.rimColor = FromJSON<Vector4>(json["rimColor"]);
        }
        outMaterial.rimPower = FromJSON<float>(json.value("rimPower", 2.0f));
        outMaterial.rimIntensity = FromJSON<float>(json.value("rimIntensity", 0.0f));

        return true;
    } catch (const std::exception& e) {
        Log(Translation("engine.material.parse.failed") + e.what(), LogSeverity::Warning);
        return false;
    }
}

MaterialHandle RegisterEntry(MaterialEntry&& entry) {
    // ハンドルは使い回さず単調増加で採番する。
    // 以前は sMaterials.size() + 1 で採番していたため、マテリアルを削除した後に新規登録すると
    // 生き残っている既存ハンドルと衝突して kInvalidHandle が返り、以後の新規追加が全て失敗していた。
    while (sNextHandle == MaterialManager::kInvalidHandle || sMaterials.contains(sNextHandle)) {
        ++sNextHandle;
    }
    const MaterialHandle handle = sNextHandle++;

    sFileNameToHandle[entry.fileName] = handle;
    sNameToHandle[entry.material.name] = handle;
    sMaterials.emplace(handle, std::move(entry));
    return handle;
}

} // namespace

MaterialManager::MaterialManager(Passkey<GameEngine>, const std::string& assetsRootPath) {
    LogScope scope;
    sAssetsRootPath = NormalizePathSlashes(assetsRootPath);
    InitializeMaterialManager();
    LoadAllFromAssetsFolder();
#if defined(USE_IMGUI)
    sActiveInstance = this;
#endif
}

MaterialManager::~MaterialManager() {
    LogScope scope;
#if defined(USE_IMGUI)
    if (sActiveInstance == this) sActiveInstance = nullptr;
#endif
    FinalizeMaterialManager();
}

void MaterialManager::InitializeMaterialManager() {
    sMaterials.clear();
    sNameToHandle.clear();
    sFileNameToHandle.clear();
    sNextHandle = 1;
}

void MaterialManager::FinalizeMaterialManager() {
    sMaterials.clear();
    sNameToHandle.clear();
    sFileNameToHandle.clear();
    sNextHandle = 1;
}

void MaterialManager::LoadAllFromAssetsFolder() {
    LogScope scope;
    const auto dir = GetDirectoryData(sAssetsRootPath, true, true);

    std::vector<std::string> files;
    const auto filtered = GetDirectoryDataByExtension(dir, { ".mat" });

    std::function<void(const DirectoryData&)> flatten = [&](const DirectoryData& d) {
        for (const auto& f : d.files) files.push_back(f);
        for (const auto& sd : d.subdirectories) flatten(sd);
    };
    flatten(filtered);

    for (const auto& f : files) {
        LoadMaterial(f);
    }
}

MaterialManager::MaterialHandle MaterialManager::LoadMaterial(const std::string& filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    const std::filesystem::path p = Utf8StringToPath(filePath);
    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.material.loading.failed.notfound") + PathToUtf8String(p), LogSeverity::Warning);
        return kInvalidHandle;
    }

    if (!HasSupportedMaterialExtension(p)) {
        Log(Translation("engine.material.loading.failed.unsupported") + PathToUtf8String(p), LogSeverity::Warning);
        return kInvalidHandle;
    }

    const std::string full = NormalizePathSlashes(PathToUtf8String(p));
    const std::string asset = MakeAssetRelativePath(sAssetsRootPath, full);

    // 既に読み込み済みかチェック（assetPath -> handle の直接マップは存在しないため走査して探す。
    // Material::name とは独立した概念のため sNameToHandle では照合できない）
    {
        auto it = std::find_if(sMaterials.begin(), sMaterials.end(),
            [&asset](const auto &kv) { return kv.second.assetPath == asset; });
        if (it != sMaterials.end()) return it->first;
    }

    MaterialEntry entry{};
    entry.fullPath = full;
    entry.assetPath = asset;
    entry.fileName = PathToUtf8String(p.filename());
    entry.material = Material{};

    if (!LoadMaterialFromJSON(full, entry.material)) {
        Log(Translation("engine.material.loading.failed.parse") + PathToUtf8String(p), LogSeverity::Warning);
        return kInvalidHandle;
    }

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.material.loading.failed.register") + PathToUtf8String(p), LogSeverity::Error);
        return kInvalidHandle;
    }

    Log(Translation("engine.material.loading.succeeded") + PathToUtf8String(p), LogSeverity::Info);
    return handle;
}

#if defined(USE_IMGUI)
MaterialManager::MaterialHandle MaterialManager::LoadMaterialDynamic(const std::string &filePath) {
    if (!sActiveInstance) return kInvalidHandle;
    return sActiveInstance->LoadMaterial(filePath);
}
#endif

bool MaterialManager::SaveMaterial(MaterialHandle handle, const std::string &filePath) {
    if (handle == kInvalidHandle) return false;
    auto it = sMaterials.find(handle);
    if (it == sMaterials.end()) return false;

    std::string savePath;
    if (!filePath.empty()) {
        savePath = NormalizePathSlashes(filePath);
    } else {
        savePath = it->second.fullPath;
    }
    if (savePath.empty()) return false;

    const Material &material = it->second.material;
    JSON json = JSON::object();
    json["name"] = material.name;
    json["color"] = ToJSON(material.color);
    json["uvTransform"] = ToJSON(material.uvTransform);
    // ハンドルが未解決の場合でも読み込み時のファイル名を保持して保存する
    std::string textureFile = TextureManager::GetTextureFileName(material.textureHandle);
    if (textureFile.empty()) textureFile = material.textureFileName;
    std::string environmentFile = TextureManager::GetTextureFileName(material.environmentHandle);
    if (environmentFile.empty()) environmentFile = material.environmentFileName;
    std::string normalMapFile = TextureManager::GetTextureFileName(material.normalMapHandle);
    if (normalMapFile.empty()) normalMapFile = material.normalMapFileName;
    json["textureFile"] = textureFile;
    json["environmentFile"] = environmentFile;
    json["normalMapFile"] = normalMapFile;
    json["samplerHandle"] = material.samplerHandle;
    json["shininess"] = material.shininess;
    json["specularColor"] = ToJSON(material.specularColor);
    json["environmentCoefficient"] = material.environmentCoefficient;
    json["enableLighting"] = material.enableLighting;
    json["enableShadowMapProjection"] = material.enableShadowMapProjection;
    json["rimColor"] = ToJSON(material.rimColor);
    json["rimPower"] = material.rimPower;
    json["rimIntensity"] = material.rimIntensity;

    std::error_code ec;
    std::filesystem::create_directories(Utf8StringToPath(savePath).parent_path(), ec);
    SaveJSON(json, savePath);

    // 保存先が変わった場合はエントリ情報を更新する
    if (it->second.fullPath != savePath) {
        it->second.fullPath = savePath;
        it->second.assetPath = MakeAssetRelativePath(sAssetsRootPath, savePath);
        it->second.fileName = PathToUtf8String(Utf8StringToPath(savePath).filename());
    }
    return true;
}

bool MaterialManager::SaveAllMaterials(const std::string &folderPath) {
    for (const auto &pair : sMaterials) {
        const MaterialHandle handle = pair.first;
        const MaterialEntry &entry = pair.second;
        std::string savePath;
        if (!folderPath.empty()) {
            savePath = NormalizePathSlashes(folderPath + "/" + entry.fileName);
        } else {
            savePath = entry.fullPath;
        }
        if (!SaveMaterial(handle, savePath)) {
            Log(Translation("engine.material.saving.failed") + savePath, LogSeverity::Warning);
            return false;
        }
    }
    return true;
}

MaterialManager::MaterialHandle MaterialManager::GetMaterialHandleFromFileName(const std::string& fileName) {
    LogScope scope;
    auto it = sFileNameToHandle.find(fileName);
    if (it == sFileNameToHandle.end()) return kInvalidHandle;
    return it->second;
}

MaterialManager::MaterialHandle MaterialManager::GetMaterialHandleFromName(const std::string& name) {
    LogScope scope;
    auto it = sNameToHandle.find(name);
    if (it == sNameToHandle.end()) return kInvalidHandle;
    return it->second;
}

bool MaterialManager::RenameMaterialFile(const std::string &oldAssetPath, const std::string &newAssetPath) {
    LogScope scope;
    const std::string normalizedOld = NormalizePathSlashes(oldAssetPath);
    // assetPath -> handle の直接マップは存在しないため走査して探す
    // （Material::name とは独立した概念のため sNameToHandle は使わない）
    auto entryIt = std::find_if(sMaterials.begin(), sMaterials.end(),
        [&normalizedOld](const auto &kv) { return kv.second.assetPath == normalizedOld; });
    if (entryIt == sMaterials.end()) return false;

    MaterialEntry &entry = entryIt->second;
    sFileNameToHandle.erase(entry.fileName);

    const std::string normalizedNew = NormalizePathSlashes(newAssetPath);
    entry.assetPath = normalizedNew;
    entry.fileName = PathToUtf8String(Utf8StringToPath(normalizedNew).filename());
    entry.fullPath = sAssetsRootPath + "/" + normalizedNew;

    sFileNameToHandle[entry.fileName] = entryIt->first;
    return true;
}

MaterialManager::Material* MaterialManager::GetMaterial(MaterialHandle handle) {
    if (handle == kInvalidHandle) return nullptr;
    auto it = sMaterials.find(handle);
    if (it == sMaterials.end()) return nullptr;
    return &it->second.material;
}

MaterialManager::Material* MaterialManager::GetMaterial(const std::string& name) {
    MaterialHandle handle = GetMaterialHandleFromName(name);
    return GetMaterial(handle);
}

MaterialHandle MaterialManager::RegisterMaterial(const std::string &name, const Material &material, const std::string &filePath) {
    (void)name;
    MaterialEntry entry{};
    entry.fullPath = NormalizePathSlashes(filePath);
    entry.assetPath = MakeAssetRelativePath(sAssetsRootPath, entry.fullPath);
    entry.fileName = PathToUtf8String(Utf8StringToPath(filePath).filename());
    entry.material = material;
    return RegisterEntry(std::move(entry));
}

bool MaterialManager::RemoveMaterial(const std::string &name) {
    LogScope scope;
    auto it = sNameToHandle.find(name);
    if (it == sNameToHandle.end() || it->second == kInvalidHandle) return false;

    MaterialHandle handle = it->second;
    sNameToHandle.erase(it);

    // sMaterials から削除
    auto mat_it = sMaterials.find(handle);
    if (mat_it != sMaterials.end()) {
        const std::string fileName = mat_it->second.fileName;
        sMaterials.erase(mat_it);
        sFileNameToHandle.erase(fileName);
    }

    return true;
}

std::vector<MaterialEntry> MaterialManager::GetLoadedMaterialListEntries() {
    LogScope scope;
    std::vector<MaterialEntry> out;
    out.reserve(sMaterials.size());
    for (const auto& pair : sMaterials) {
        out.push_back(pair.second);
    }
    std::sort(out.begin(), out.end(), [](const MaterialEntry &a, const MaterialEntry &b) {
        return a.material.name < b.material.name;
        });
    return out;
}

const std::string &MaterialManager::GetAssetsRootPath() const noexcept {
    return sAssetsRootPath;
}

#if defined(USE_IMGUI)
void MaterialManager::ShowImGuiMaterialManagerWindow() {
    if (!ImGui::Begin(TranslationLabel("editor.materialmanager.window"))) {
        ImGui::End();
        return;
    }

    ImGui::Text(TranslationC("editor.materialmanager.materials_d"), static_cast<int>(sMaterials.size()));

    //--------- 新規マテリアルの追加 ---------//
    static char sNewMaterialName[128] = "";
    ImGui::InputText("##NewMaterialName", sNewMaterialName, sizeof(sNewMaterialName));
    ImGui::SameLine();
    if (ImGui::Button(TranslationLabel("editor.materialmanager.add_material"))) {
        const std::string name = sNewMaterialName;
        if (!name.empty() && GetMaterialHandleFromName(name) == kInvalidHandle) {
            Material material{};
            material.name = name;
            const std::string filePath = sAssetsRootPath + "/Materials/" + name + ".mat";
            const auto handle = RegisterMaterial(name, material, filePath);
            if (handle != kInvalidHandle) {
                SaveMaterial(handle);
                sNewMaterialName[0] = '\0';
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(TranslationLabel("editor.materialmanager.save_all"))) {
        SaveAllMaterials();
    }

    ImGui::Separator();

    //--------- マテリアル一覧と編集 ---------//
    static MaterialHandle sSelectedHandle = kInvalidHandle;
    std::string pendingRemoveName;

    if (ImGui::BeginTable("##MaterialList", 3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
        ImVec2(0, 180))) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();

        for (const auto &entry : GetLoadedMaterialListEntries()) {
            const auto handle = GetMaterialHandleFromName(entry.material.name);
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(handle));

            ImGui::TableSetColumnIndex(0);
            const bool isSelected = (sSelectedHandle == handle);
            if (ImGui::Selectable(entry.material.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                sSelectedHandle = handle;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.assetPath.c_str());

            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton(TranslationLabel("editor.materialmanager.remove"))) {
                pendingRemoveName = entry.material.name;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!pendingRemoveName.empty()) {
        const auto handle = GetMaterialHandleFromName(pendingRemoveName);
        if (handle == sSelectedHandle) sSelectedHandle = kInvalidHandle;
        RemoveMaterial(pendingRemoveName);
    }

    ImGui::Separator();

    //--------- 選択中マテリアルの編集 ---------//
    auto *material = GetMaterial(sSelectedHandle);
    if (!material) {
        ImGui::TextUnformatted(TranslationC("editor.materialmanager.no_material_selected"));
        ImGui::End();
        return;
    }

    // 名前の変更（Enter確定。名前マップも更新する）
    static char sRenameBuffer[128] = "";
    static MaterialHandle sRenameTarget = kInvalidHandle;
    if (sRenameTarget != sSelectedHandle) {
        std::snprintf(sRenameBuffer, sizeof(sRenameBuffer), "%s", material->name.c_str());
        sRenameTarget = sSelectedHandle;
    }
    if (ImGui::InputText(TranslationLabel("editor.materialmanager.name"), sRenameBuffer, sizeof(sRenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        const std::string newName = sRenameBuffer;
        if (!newName.empty() && newName != material->name && GetMaterialHandleFromName(newName) == kInvalidHandle) {
            sNameToHandle.erase(material->name);
            material->name = newName;
            sNameToHandle[newName] = sSelectedHandle;
        }
    }

    ShowMaterialEditorFields(*material);

    if (ImGui::Button(TranslationLabel("editor.materialmanager.save"))) {
        SaveMaterial(sSelectedHandle);
    }

    ImGui::End();
}

void MaterialManager::ShowMaterialEditorFields(Material &material) {
    ImGui::ColorEdit4(TranslationLabel("editor.materialmanager.color"), &material.color.x);

    // テクスチャは読み込み済みのものから選択する
    // ファイル名単体だと同名ファイルが複数フォルダにある場合にImGuiのID重複警告が出るため、
    // Assetsからの相対パスを表示・選択キーとして使う（選択候補は常に解決済みなのでハンドルは即座に得られる）
    std::vector<std::string> texturePaths;
    for (const auto &entry : TextureManager::GetLoadedTextureListEntries()) {
        texturePaths.push_back(entry.assetPath);
    }
    std::string texturePath = TextureManager::GetTextureAssetPath(material.textureHandle);
    if (texturePath.empty()) texturePath = material.textureFileName; // 未解決の場合は保留中のファイル名を表示
    if (ImGuiCustom::SelectString(TranslationLabel("editor.materialmanager.texture"), texturePath, texturePaths, true)) {
        material.textureHandle = texturePath.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(texturePath);
        material.textureFileName = TextureManager::GetTextureFileName(material.textureHandle);
    }
    // Assetsウィンドウからのテクスチャファイルドラッグ&ドロップも受け付ける
    if (std::string droppedPath; AcceptAssetDragDropTarget(kTextureAssetDragDropType, droppedPath)) {
        material.textureHandle = TextureManager::GetTextureFromAssetPath(droppedPath);
        material.textureFileName = TextureManager::GetTextureFileName(material.textureHandle);
    }
    std::string environmentPath = TextureManager::GetTextureAssetPath(material.environmentHandle);
    if (environmentPath.empty()) environmentPath = material.environmentFileName;
    if (ImGuiCustom::SelectString(TranslationLabel("editor.materialmanager.environment"), environmentPath, texturePaths, true)) {
        material.environmentHandle = environmentPath.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(environmentPath);
        material.environmentFileName = TextureManager::GetTextureFileName(material.environmentHandle);
    }
    if (std::string droppedPath; AcceptAssetDragDropTarget(kTextureAssetDragDropType, droppedPath)) {
        material.environmentHandle = TextureManager::GetTextureFromAssetPath(droppedPath);
        material.environmentFileName = TextureManager::GetTextureFileName(material.environmentHandle);
    }
    std::string normalMapPath = TextureManager::GetTextureAssetPath(material.normalMapHandle);
    if (normalMapPath.empty()) normalMapPath = material.normalMapFileName;
    if (ImGuiCustom::SelectString(TranslationLabel("editor.materialmanager.normal_map"), normalMapPath, texturePaths, true)) {
        material.normalMapHandle = normalMapPath.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(normalMapPath);
        material.normalMapFileName = TextureManager::GetTextureFileName(material.normalMapHandle);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TranslationC("editor.materialmanager.rgb_0_1_opengl_y"));
    }
    if (std::string droppedPath; AcceptAssetDragDropTarget(kTextureAssetDragDropType, droppedPath)) {
        material.normalMapHandle = TextureManager::GetTextureFromAssetPath(droppedPath);
        material.normalMapFileName = TextureManager::GetTextureFileName(material.normalMapHandle);
    }

    ImGui::DragFloat(TranslationLabel("editor.materialmanager.shininess"), &material.shininess, 0.1f, 0.0f, 1024.0f);
    ImGui::ColorEdit4(TranslationLabel("editor.materialmanager.specular_color"), &material.specularColor.x);
    ImGui::DragFloat(TranslationLabel("editor.materialmanager.environment_coefficient"), &material.environmentCoefficient, 0.01f, 0.0f, 1.0f);
    ImGui::ColorEdit4(TranslationLabel("editor.materialmanager.rim_color"), &material.rimColor.x);
    ImGui::DragFloat(TranslationLabel("editor.materialmanager.rim_power"), &material.rimPower, 0.05f, 0.1f, 16.0f);
    ImGui::DragFloat(TranslationLabel("editor.materialmanager.rim_intensity"), &material.rimIntensity, 0.01f, 0.0f, 10.0f);
    ImGui::Checkbox(TranslationLabel("editor.materialmanager.enable_lighting"), &material.enableLighting);
    ImGui::Checkbox(TranslationLabel("editor.materialmanager.enable_shadowmap_projection"), &material.enableShadowMapProjection);
    ImGuiCustom::EditValue(TranslationLabel("editor.materialmanager.uv_transform"), material.uvTransform);
}
#endif

} // namespace KashipanEngine