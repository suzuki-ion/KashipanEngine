#include "AssetsWindow.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

#include "Assets/TextureManager.h"

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
} // namespace

AssetsWindow::AssetsWindow(Passkey<SceneEditor>) {
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

    ImGui::End();
}

bool AssetsWindow::IsSupportedExtension(const std::string &ext) {
    static const std::array<const char *, 36> kSupported = {
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

        // フォルダはダブルクリックで移動する（Unityと同じ操作）
        if (file.isFolder && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            currentFolder_ = file.path;
            RefreshFileList();
            ImGui::EndGroup();
            ImGui::PopID();
            break;
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
}

} // namespace KashipanEngine

#endif // USE_IMGUI
