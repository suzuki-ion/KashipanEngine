#include "AssetsWindow.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

#include "Assets/AudioManager.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Assets/TextureManager.h"
#include "Objects/EmptyObject.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Scene/Editor/SceneObjectPayload.h"
#include "Scene/SceneEditorContext.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

namespace {
/// @brief 実行ディレクトリ("."）からの相対パスを、TextureManagerが管理する
///        Assetsルートからの相対パスへ変換する（先頭の "Assets/" を取り除く）
std::string ToAssetsRelativePath(const std::string &cwdRelativePath) {
    constexpr std::string_view kAssetsPrefix = "Assets/";
    if (cwdRelativePath.rfind(kAssetsPrefix, 0) == 0) {
        return cwdRelativePath.substr(kAssetsPrefix.size());
    }
    return cwdRelativePath;
}

bool IsTextureExtension(const std::string &ext) {
    static const std::array<const char *, 11> kExts = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".hdr", ".tif", ".tiff", ".gif", ".webp",
    };
    return std::find_if(kExts.begin(), kExts.end(), [&ext](const char *s) { return ext == s; }) != kExts.end();
}

bool IsAudioExtension(const std::string &ext) {
    static const std::array<const char *, 7> kExts = {
        ".wav", ".mp3", ".ogg", ".flac", ".aac", ".m4a", ".wma",
    };
    return std::find_if(kExts.begin(), kExts.end(), [&ext](const char *s) { return ext == s; }) != kExts.end();
}

bool IsModelExtension(const std::string &ext) {
    static const std::array<const char *, 10> kExts = {
        ".fbx", ".obj", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply", ".stl", ".x",
    };
    return std::find_if(kExts.begin(), kExts.end(), [&ext](const char *s) { return ext == s; }) != kExts.end();
}
} // namespace

AssetsWindow::AssetsWindow(Passkey<SceneEditor>, SceneEditorContext *editorContext)
    : editorContext_(editorContext) {
    RefreshFolderTree();
}

void AssetsWindow::ShowImGui() {
    if (!ImGui::Begin("Assets")) {
        ImGui::End();
        return;
    }

    //--------- ツールバー ---------//
    if (ImGui::Button("Refresh")) {
        RefreshFolderTree();
        RefreshFileList();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(currentFolder_.empty() ? "(root)" : currentFolder_.c_str());
    ImGui::Separator();

    //--------- 左：フォルダツリー ---------//
    ImGui::BeginChild("AssetsFolderTree", ImVec2(220.0f, 0.0f), true);
    ShowFolderNode(rootFolder_);
    ImGui::EndChild();

    ImGui::SameLine();

    //--------- 右：ファイルグリッド ---------//
    ImGui::BeginChild("AssetsFileGrid", ImVec2(0.0f, 0.0f), true);
    ShowFileGrid();
    ImGui::EndChild();

    // ヒエラルキーからオブジェクトをD&Dすると、現在開いているフォルダへ.prefabファイルを生成する
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kSceneObjectDragDropType)) {
            IM_ASSERT(payload->DataSize == sizeof(SceneObjectDragDropPayload));
            const auto *dndPayload = static_cast<const SceneObjectDragDropPayload *>(payload->Data);
            CreatePrefabFromObject(dndPayload->object);
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();

    //--------- 新規作成・リネーム・削除確認モーダル ---------//
    ShowCreateFileModal();
    ShowRenameModal();
    ShowDeleteConfirmModal();

    //--------- ダブルクリックで開いた編集/プレビューウィンドウ ---------//
    ShowOpenEditors();
}

bool AssetsWindow::IsSupportedExtension(const std::string &ext) {
    static const std::array<const char *, 38> kSupported = {
        // テクスチャ（TextureManager対応形式）
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".hdr", ".tif", ".tiff", ".gif", ".webp",
        // モデル（ModelManager対応形式）
        ".fbx", ".obj", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply", ".stl", ".x",
        // サウンド（AudioManager対応形式）
        ".wav", ".mp3", ".ogg", ".flac", ".aac", ".m4a", ".wma",
        // シーン・パイプライン等の定義ファイル
        ".json",
        // マテリアル
        ".mat",
        // プレハブ
        ".prefab",
        // スクリプト（AngelScript）
        ".as",
        // シェーダー
        ".hlsl", ".hlsli",
        // フォント・翻訳・テキスト
        ".ttf", ".otf", ".csv", ".txt",
    };
    return std::find_if(kSupported.begin(), kSupported.end(),
        [&ext](const char *s) { return ext == s; }) != kSupported.end();
}

unsigned int AssetsWindow::ExtensionColor(const std::string &ext) {
    auto in = [&ext](std::initializer_list<const char *> list) {
        return std::find_if(list.begin(), list.end(), [&ext](const char *s) { return ext == s; }) != list.end();
    };
    if (in({ ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".hdr", ".tif", ".tiff", ".gif", ".webp" })) return IM_COL32(96, 168, 96, 255);   // テクスチャ: 緑
    if (in({ ".fbx", ".obj", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply", ".stl", ".x" })) return IM_COL32(96, 128, 192, 255);            // モデル: 青
    if (in({ ".wav", ".mp3", ".ogg", ".flac", ".aac", ".m4a", ".wma" })) return IM_COL32(192, 144, 64, 255);                                    // サウンド: 橙
    if (in({ ".json" })) return IM_COL32(176, 176, 96, 255);                                                                                    // JSON: 黄
    if (in({ ".mat" })) return IM_COL32(96, 176, 176, 255);                                                                                     // マテリアル: 水色
    if (in({ ".prefab" })) return IM_COL32(112, 144, 224, 255);                                                                                 // プレハブ: 青紫
    if (in({ ".as" })) return IM_COL32(176, 112, 112, 255);                                                                                     // スクリプト: 赤

    if (in({ ".hlsl", ".hlsli" })) return IM_COL32(160, 96, 176, 255);                                                                          // シェーダー: 紫
    return IM_COL32(128, 128, 128, 255);
}

std::string AssetsWindow::ToLowerExtension(const std::filesystem::path &path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

void AssetsWindow::RefreshFolderTree() {
    rootFolder_ = FolderNode{};
    rootFolder_.name = "(root)";
    rootFolder_.path = "";
    BuildFolderNode(rootFolder_);
    RefreshFileList();
}

void AssetsWindow::BuildFolderNode(FolderNode &node) {
    std::error_code ec;
    const std::filesystem::path base = node.path.empty() ? "." : node.path;
    for (const auto &entry : std::filesystem::directory_iterator(base, ec)) {
        if (!entry.is_directory()) continue;
        const std::string name = entry.path().filename().string();
        // 隠しフォルダは表示しない
        if (!name.empty() && name.front() == '.') continue;
        auto child = std::make_unique<FolderNode>();
        child->name = name;
        child->path = entry.path().lexically_relative(".").generic_string();
        BuildFolderNode(*child);
        node.children.push_back(std::move(child));
    }
    std::sort(node.children.begin(), node.children.end(),
        [](const auto &a, const auto &b) { return a->name < b->name; });
}

void AssetsWindow::RefreshFileList() {
    files_.clear();
    std::error_code ec;
    const std::filesystem::path base = currentFolder_.empty() ? "." : currentFolder_;
    for (const auto &entry : std::filesystem::directory_iterator(base, ec)) {
        FileEntry file;
        file.name = entry.path().filename().string();
        if (!file.name.empty() && file.name.front() == '.') continue;
        file.path = entry.path().lexically_relative(".").generic_string();
        if (entry.is_directory()) {
            file.isFolder = true;
        } else {
            file.extension = ToLowerExtension(entry.path());
            if (!IsSupportedExtension(file.extension)) continue;
        }
        files_.push_back(std::move(file));
    }
    // フォルダを先、その後は名前順（Unityと同じ並び）
    std::sort(files_.begin(), files_.end(), [](const FileEntry &a, const FileEntry &b) {
        if (a.isFolder != b.isFolder) return a.isFolder;
        return a.name < b.name;
    });
}

void AssetsWindow::ShowFolderNode(FolderNode &node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (node.path == currentFolder_) flags |= ImGuiTreeNodeFlags_Selected;
    if (node.path.empty()) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    const bool isOpen = ImGui::TreeNodeEx(node.name.c_str(), flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        currentFolder_ = node.path;
        RefreshFileList();
    }
    if (isOpen) {
        for (auto &child : node.children) {
            ShowFolderNode(*child);
        }
        ImGui::TreePop();
    }
}

void AssetsWindow::ShowFileGrid() {
    constexpr float kCellSize = 96.0f;
    constexpr float kThumbnailSize = 64.0f;
    const float availWidth = ImGui::GetContentRegionAvail().x;
    const int columns = std::max(1, static_cast<int>(availWidth / kCellSize));

    int index = 0;
    for (const auto &file : files_) {
        ImGui::PushID(file.path.c_str());
        if (index % columns != 0) ImGui::SameLine();
        ImGui::BeginGroup();

        const ImVec2 thumbnailSize(kThumbnailSize, kThumbnailSize);
        const float offsetX = (kCellSize - kThumbnailSize) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

        bool activated = false;
        if (file.isFolder) {
            // フォルダアイコン（黄色いブロック）
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 176, 96, 255));
            activated = ImGui::Button("##folder", thumbnailSize);
            ImGui::PopStyleColor();
        } else {
            // 読み込み済みテクスチャの場合はサムネイルを表示する
            // file.path は実行ディレクトリ（"."）からの相対パス（例: "Assets/Materials/White.png"）だが、
            // TextureManager 側は Assets ルートからの相対パス（例: "Materials/White.png"）で管理しているため、
            // 先頭の "Assets/" を取り除いてから問い合わせる必要がある
            D3D12_GPU_DESCRIPTOR_HANDLE srvHandle{};
            const auto textureHandle = TextureManager::GetTextureFromAssetPath(ToAssetsRelativePath(file.path));
            if (textureHandle != TextureManager::kInvalidHandle) {
                srvHandle = TextureManager::GetTextureView(textureHandle).GetSrvHandle();
            }
            if (srvHandle.ptr != 0) {
                activated = ImGui::ImageButton("##thumb",
                    static_cast<ImTextureID>(srvHandle.ptr), thumbnailSize);
            } else {
                // 分類色のブロックに拡張子を表示する
                ImGui::PushStyleColor(ImGuiCol_Button, ExtensionColor(file.extension));
                activated = ImGui::Button(file.extension.c_str(), thumbnailSize);
                ImGui::PopStyleColor();
            }
        }
        (void)activated;

        // テクスチャ/マテリアル/スクリプト/プレハブファイルはD&Dでコンポーネントのフィールド指定や
        // シーンへの配置ができるようにする
        if (!file.isFolder && (IsTextureExtension(file.extension) || file.extension == ".mat" || file.extension == ".as" || file.extension == ".prefab")) {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                if (IsTextureExtension(file.extension)) {
                    SetAssetDragDropPayload(kTextureAssetDragDropType, ToAssetsRelativePath(file.path));
                } else if (file.extension == ".mat") {
                    SetAssetDragDropPayload(kMaterialAssetDragDropType, ToAssetsRelativePath(file.path));
                } else if (file.extension == ".prefab") {
                    // プレハブは読み込みに使う実行ディレクトリからの相対パスで渡す
                    SetAssetDragDropPayload(kPrefabAssetDragDropType, file.path);
                } else {
                    // スクリプトはScriptComponentのScript Pathと同じ形式（"Assets/"プレフィックス付き）で渡す
                    SetAssetDragDropPayload(kScriptAssetDragDropType, file.path);
                }
                ImGui::Text("%s", file.name.c_str());
                ImGui::EndDragDropSource();
            }
        }

        // フォルダはダブルクリックで移動する（Unityと同じ操作）
        if (file.isFolder && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            currentFolder_ = file.path;
            RefreshFileList();
            ImGui::EndGroup();
            ImGui::PopID();
            break;
        }
        // ファイルはダブルクリックで拡張子に応じた編集/プレビューウィンドウを開く
        if (!file.isFolder && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            OpenFileEditor(file);
        }
        if (!file.isFolder) {
            ShowFileContextMenu(file);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", file.path.c_str());
        }

        // ファイル名（セル幅で折り返し）
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kCellSize);
        ImGui::TextWrapped("%s", file.name.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
        ImGui::PopID();
        ++index;
    }

    if (files_.empty()) {
        ImGui::TextUnformatted("No supported files in this folder.");
    }

    ShowGridBackgroundContextMenu();
}

void AssetsWindow::ShowFileContextMenu(const FileEntry &file) {
    if (ImGui::BeginPopupContextItem("FileContextMenu")) {
        if (ImGui::MenuItem("Open")) {
            OpenFileEditor(file);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Rename")) {
            contextMenuTargetPath_ = file.path;
            renameBuffer_ = file.name;
            isRenameRequested_ = true;
        }
        if (ImGui::MenuItem("Delete")) {
            contextMenuTargetPath_ = file.path;
            isDeleteRequested_ = true;
        }
        ImGui::EndPopup();
    }
}

void AssetsWindow::ShowGridBackgroundContextMenu() {
    // ImGuiPopupFlags_NoOpenOverItems を指定しないと、ファイル項目上での右クリックでも
    // この window レベルのメニューが同一フレームで開いてしまい、
    // ファイル自体の FileContextMenu を閉じてしまう（表示されないように見える）ため必須。
    if (ImGui::BeginPopupContextWindow("AssetsGridContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Create JSON File")) {
            createFileExtension_ = ".json";
            newFileName_ = "New File";
            isCreateFileRequested_ = true;
        }
        if (ImGui::MenuItem("Create Material File")) {
            createFileExtension_ = ".mat";
            newFileName_ = "New Material";
            isCreateFileRequested_ = true;
        }
        ImGui::EndPopup();
    }
}

void AssetsWindow::OpenFileEditor(const FileEntry &file) {
    if (file.isFolder) return;
    if (file.extension == ".json" || file.extension == ".prefab") {
        // .prefabの中身はただのJSONのため、JSONエディターでそのまま開ける
        for (auto &editor : jsonEditors_) {
            if (editor && editor->GetFilePath() == file.path) return;
        }
        jsonEditors_.push_back(std::make_unique<JSONFileEditorWindow>(file.path));
    } else if (file.extension == ".mat") {
        const std::string assetPath = ToAssetsRelativePath(file.path);
        for (auto &editor : materialEditors_) {
            if (editor && editor->GetAssetPath() == assetPath) return;
        }
        materialEditors_.push_back(std::make_unique<MaterialFileEditorWindow>(assetPath));
    } else if (IsTextureExtension(file.extension)) {
        const std::string assetPath = ToAssetsRelativePath(file.path);
        for (auto &editor : imagePreviews_) {
            if (editor && editor->GetAssetPath() == assetPath) return;
        }
        imagePreviews_.push_back(std::make_unique<ImagePreviewWindow>(assetPath));
    } else if (IsAudioExtension(file.extension)) {
        const std::string assetPath = ToAssetsRelativePath(file.path);
        for (auto &editor : audioPreviews_) {
            if (editor && editor->GetAssetPath() == assetPath) return;
        }
        audioPreviews_.push_back(std::make_unique<AudioPreviewWindow>(assetPath));
    }
}

void AssetsWindow::ShowOpenEditors() {
    auto prune = [](auto &container) {
        for (size_t i = 0; i < container.size();) {
            if (container[i] && container[i]->ShowImGui()) {
                ++i;
            } else {
                container.erase(container.begin() + i);
            }
        }
    };
    prune(jsonEditors_);
    prune(materialEditors_);
    prune(imagePreviews_);
    prune(audioPreviews_);
}

void AssetsWindow::CloseEditorsForPath(const std::string &cwdRelativePath) {
    jsonEditors_.erase(std::remove_if(jsonEditors_.begin(), jsonEditors_.end(),
        [&](const auto &editor) { return editor && editor->GetFilePath() == cwdRelativePath; }), jsonEditors_.end());

    const std::string assetPath = ToAssetsRelativePath(cwdRelativePath);
    materialEditors_.erase(std::remove_if(materialEditors_.begin(), materialEditors_.end(),
        [&](const auto &editor) { return editor && editor->GetAssetPath() == assetPath; }), materialEditors_.end());
    imagePreviews_.erase(std::remove_if(imagePreviews_.begin(), imagePreviews_.end(),
        [&](const auto &editor) { return editor && editor->GetAssetPath() == assetPath; }), imagePreviews_.end());
    audioPreviews_.erase(std::remove_if(audioPreviews_.begin(), audioPreviews_.end(),
        [&](const auto &editor) { return editor && editor->GetAssetPath() == assetPath; }), audioPreviews_.end());
}

void AssetsWindow::CreatePrefabFromObject(EmptyObject *obj) {
    if (!obj || !editorContext_) return;

    const JSON prefabJson = PrefabUtility::BuildPrefabJson(editorContext_, obj);
    if (prefabJson.empty()) return;

    // ファイル名に使えない文字をオブジェクト名から取り除く
    std::string baseName = obj->GetName();
    constexpr std::string_view kInvalidChars = "\\/:*?\"<>|";
    for (auto &c : baseName) {
        if (kInvalidChars.find(c) != std::string_view::npos) c = '_';
    }
    if (baseName.empty()) baseName = "Prefab";

    // 既存ファイルと重複しない名前を付ける（Unityの複製と同様に連番を付与する）
    const std::string folder = currentFolder_.empty() ? "" : (currentFolder_ + "/");
    std::string filePath = folder + baseName + PrefabUtility::kPrefabExtension;
    for (int suffix = 1; std::filesystem::exists(filePath); ++suffix) {
        filePath = folder + baseName + "_" + std::to_string(suffix) + PrefabUtility::kPrefabExtension;
    }

    if (SaveJSON(prefabJson, filePath)) {
        RefreshFileList();
    }
}

bool AssetsWindow::ShowCreateFileModal() {
    bool created = false;
    if (isCreateFileRequested_) {
        ImGui::OpenPopup("Create Asset File");
        isCreateFileRequested_ = false;
    }
    if (ImGui::BeginPopupModal("Create Asset File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Extension: %s", createFileExtension_.c_str());
        ImGui::InputText("File Name", &newFileName_);
        const std::string folder = currentFolder_.empty() ? "" : (currentFolder_ + "/");
        const std::string fullPath = folder + newFileName_ + createFileExtension_;
        const bool alreadyExists = !newFileName_.empty() && std::filesystem::exists(fullPath);
        if (alreadyExists) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "A file with this name already exists.");
        }
        ImGui::BeginDisabled(newFileName_.empty() || alreadyExists);
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            bool succeeded = false;
            if (createFileExtension_ == ".mat") {
                MaterialManager::Material material{};
                material.name = newFileName_;
                const auto handle = MaterialManager::RegisterMaterial(newFileName_, material, fullPath);
                succeeded = (handle != MaterialManager::kInvalidHandle) && MaterialManager::SaveMaterial(handle);
            } else {
                succeeded = SaveJSON(JSON::object(), fullPath);
            }
            if (succeeded) {
                RefreshFileList();
                created = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return created;
}

void AssetsWindow::ShowRenameModal() {
    if (isRenameRequested_) {
        ImGui::OpenPopup("Rename File");
        isRenameRequested_ = false;
    }
    if (ImGui::BeginPopupModal("Rename File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(contextMenuTargetPath_.c_str());
        ImGui::InputText("New Name", &renameBuffer_);
        ImGui::BeginDisabled(renameBuffer_.empty());
        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            const std::filesystem::path oldPath = contextMenuTargetPath_;
            const std::filesystem::path newPath = oldPath.parent_path() / renameBuffer_;
            std::error_code ec;
            std::filesystem::rename(oldPath, newPath, ec);
            if (!ec) {
                // 実ファイルのリネームに成功したら、対応するマネージャーの登録名/パスも追従させる
                // （リネーム前に既に読み込まれていなかった場合は各RenameXxxは何もせずfalseを返すだけ）
                const std::string oldAssetPath = ToAssetsRelativePath(oldPath.lexically_relative(".").generic_string());
                const std::string newAssetPath = ToAssetsRelativePath(newPath.lexically_relative(".").generic_string());
                const std::string ext = ToLowerExtension(newPath);
                if (IsTextureExtension(ext)) {
                    TextureManager::RenameTexture(oldAssetPath, newAssetPath);
                } else if (ext == ".mat") {
                    MaterialManager::RenameMaterialFile(oldAssetPath, newAssetPath);
                } else if (IsAudioExtension(ext)) {
                    AudioManager::RenameSound(oldAssetPath, newAssetPath);
                } else if (IsModelExtension(ext)) {
                    ModelManager::RenameModel(oldAssetPath, newAssetPath);
                }
                // 古いパスを指している開いている編集/プレビューウィンドウは無効になるため閉じる
                CloseEditorsForPath(contextMenuTargetPath_);
                RefreshFileList();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AssetsWindow::ShowDeleteConfirmModal() {
    if (isDeleteRequested_) {
        ImGui::OpenPopup("Delete File");
        isDeleteRequested_ = false;
    }
    if (ImGui::BeginPopupModal("Delete File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "This will permanently delete the actual file on disk:");
        ImGui::TextUnformatted(contextMenuTargetPath_.c_str());
        ImGui::TextUnformatted("This action cannot be undone.");
        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            std::error_code ec;
            std::filesystem::remove(contextMenuTargetPath_, ec);
            RefreshFileList();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
