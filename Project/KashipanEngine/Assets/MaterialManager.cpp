#include "MaterialManager.h"
#include "Assets/CaseInsensitive.h"

#include "Debug/Logger.h"
#include "Utilities/Translation.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/FileIO/JSON.h"

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
    std::filesystem::path root(assetsRoot);
    std::filesystem::path full(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(full.filename().string());
    }
    return NormalizePathSlashes(rel.string());
}

bool LoadMaterialFromJSON(const std::string& filePath, MaterialManager::Material& outMaterial) {
    try {
        JSON json = LoadJSON(filePath);
        if (!json.is_object()) return false;

        // マテリアル名
        if (json.contains("name") && json["name"].is_string()) {
            outMaterial.name = FromJSON<std::string>(json["name"]);
        } else {
            outMaterial.name = std::filesystem::path(filePath).stem().string();
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

        return true;
    } catch (const std::exception& e) {
        Log(std::string("Failed to parse material JSON: ") + e.what(), LogSeverity::Warning);
        return false;
    }
}

MaterialHandle RegisterEntry(MaterialEntry&& entry) {
    const MaterialHandle handle = static_cast<MaterialHandle>(sMaterials.size() + 1u);
    if (handle == MaterialManager::kInvalidHandle) return MaterialManager::kInvalidHandle;
    if (sMaterials.find(handle) != sMaterials.end()) return MaterialManager::kInvalidHandle;

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
}

MaterialManager::~MaterialManager() {
    LogScope scope;
    FinalizeMaterialManager();
}

void MaterialManager::InitializeMaterialManager() {
    sMaterials.clear();
    sNameToHandle.clear();
    sFileNameToHandle.clear();
}

void MaterialManager::FinalizeMaterialManager() {
    sMaterials.clear();
    sNameToHandle.clear();
    sFileNameToHandle.clear();
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

    const std::filesystem::path p(filePath);
    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.material.loading.failed.notfound") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    if (!HasSupportedMaterialExtension(p)) {
        Log(Translation("engine.material.loading.failed.unsupported") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    const std::string full = NormalizePathSlashes(p.string());
    const std::string asset = MakeAssetRelativePath(sAssetsRootPath, full);

    // 既に読み込み済みかチェック
    {
        auto it = sNameToHandle.find(asset);
        if (it != sNameToHandle.end()) return it->second;
    }

    MaterialEntry entry{};
    entry.fullPath = full;
    entry.assetPath = asset;
    entry.fileName = p.filename().string();
    entry.material = Material{};

    if (!LoadMaterialFromJSON(full, entry.material)) {
        Log(Translation("engine.material.loading.failed.parse") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.material.loading.failed.register") + p.string(), LogSeverity::Error);
        return kInvalidHandle;
    }

    Log(Translation("engine.material.loading.succeeded") + p.string(), LogSeverity::Info);
    return handle;
}

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
    json["textureFile"] = textureFile;
    json["environmentFile"] = environmentFile;
    json["samplerHandle"] = material.samplerHandle;
    json["shininess"] = material.shininess;
    json["specularColor"] = ToJSON(material.specularColor);
    json["environmentCoefficient"] = material.environmentCoefficient;
    json["enableLighting"] = material.enableLighting;
    json["enableShadowMapProjection"] = material.enableShadowMapProjection;

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(savePath).parent_path(), ec);
    SaveJSON(json, savePath);

    // 保存先が変わった場合はエントリ情報を更新する
    if (it->second.fullPath != savePath) {
        it->second.fullPath = savePath;
        it->second.assetPath = MakeAssetRelativePath(sAssetsRootPath, savePath);
        it->second.fileName = std::filesystem::path(savePath).filename().string();
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
    entry.fileName = std::filesystem::path(filePath).filename().string();
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
    if (!ImGui::Begin("MaterialManager - Materials")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Materials: %d", static_cast<int>(sMaterials.size()));

    //--------- 新規マテリアルの追加 ---------//
    static char sNewMaterialName[128] = "";
    ImGui::InputText("##NewMaterialName", sNewMaterialName, sizeof(sNewMaterialName));
    ImGui::SameLine();
    if (ImGui::Button("Add Material")) {
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
    if (ImGui::Button("Save All")) {
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
            if (ImGui::SmallButton("Remove")) {
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
        ImGui::TextUnformatted("No material selected.");
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
    if (ImGui::InputText("Name", sRenameBuffer, sizeof(sRenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        const std::string newName = sRenameBuffer;
        if (!newName.empty() && newName != material->name && GetMaterialHandleFromName(newName) == kInvalidHandle) {
            sNameToHandle.erase(material->name);
            material->name = newName;
            sNameToHandle[newName] = sSelectedHandle;
        }
    }

    ImGui::ColorEdit4("Color", &material->color.x);

    // テクスチャは読み込み済みのものから選択する
    // ファイル名単体だと同名ファイルが複数フォルダにある場合にImGuiのID重複警告が出るため、
    // Assetsからの相対パスを表示・選択キーとして使う（選択候補は常に解決済みなのでハンドルは即座に得られる）
    std::vector<std::string> texturePaths;
    for (const auto &entry : TextureManager::GetLoadedTextureListEntries()) {
        texturePaths.push_back(entry.assetPath);
    }
    std::string texturePath = TextureManager::GetTextureAssetPath(material->textureHandle);
    if (texturePath.empty()) texturePath = material->textureFileName; // 未解決の場合は保留中のファイル名を表示
    if (ImGuiCustom::SelectString("Texture", texturePath, texturePaths, true)) {
        material->textureHandle = texturePath.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(texturePath);
        material->textureFileName = TextureManager::GetTextureFileName(material->textureHandle);
    }
    std::string environmentPath = TextureManager::GetTextureAssetPath(material->environmentHandle);
    if (environmentPath.empty()) environmentPath = material->environmentFileName;
    if (ImGuiCustom::SelectString("Environment", environmentPath, texturePaths, true)) {
        material->environmentHandle = environmentPath.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(environmentPath);
        material->environmentFileName = TextureManager::GetTextureFileName(material->environmentHandle);
    }

    ImGui::DragFloat("Shininess", &material->shininess, 0.1f, 0.0f, 1024.0f);
    ImGui::ColorEdit4("Specular Color", &material->specularColor.x);
    ImGui::DragFloat("Environment Coefficient", &material->environmentCoefficient, 0.01f, 0.0f, 1.0f);
    ImGui::Checkbox("Enable Lighting", &material->enableLighting);
    ImGui::Checkbox("Enable ShadowMap Projection", &material->enableShadowMapProjection);
    ImGuiCustom::EditValue("UV Transform", material->uvTransform);

    if (ImGui::Button("Save")) {
        SaveMaterial(sSelectedHandle);
    }

    ImGui::End();
}
#endif

} // namespace KashipanEngine