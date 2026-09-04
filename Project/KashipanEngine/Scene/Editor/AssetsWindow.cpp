#include "AssetsWindow.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")

#include "Assets/AudioManager.h"
#include "Assets/GifManager.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Assets/TextureManager.h"
#include "Assets/VideoManager.h"
#include "ComponentSerialize/ComponentRegistry.h"
#include "Core/ProjectPaths.h"
#include "Debug/Logger.h"
#include "Objects/Components/PrefabInstanceComponent.h"
#include "Objects/EmptyObject.h"
#include "Scene/Editor/EditorWindowChrome.h"
#include "Scene/Editor/PrefabAssetManager.h"
#include "Scene/Editor/PrefabUtility.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/SceneObjectPayload.h"
#include "Scene/SceneEditorContext.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/FileIO/TextFile.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

namespace {
constexpr const char *kAssetIconAtlasPath = "KashipanEngine/EditorIcons/AssetIcons.png";
constexpr float kAssetIconAtlasColumns = 4.0f;
constexpr float kAssetIconAtlasRows = 3.0f;

enum class AssetIcon : unsigned int {
    Folder,
    File,
    Image,
    Model,
    Audio,
    Video,
    Json,
    Material,
    Prefab,
    Script,
    Shader,
    Font,
};

AssetIcon GetAssetIcon(const std::string &extension) {
    LogScope scope;
    auto in = [&extension](std::initializer_list<const char *> extensions) {
        return std::find_if(extensions.begin(), extensions.end(), [&extension](const char *candidate) {
            return extension == candidate;
        }) != extensions.end();
    };

    if (in({ ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".hdr", ".tif", ".tiff", ".gif", ".webp" })) return AssetIcon::Image;
    if (in({ ".fbx", ".obj", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply", ".stl", ".x" })) return AssetIcon::Model;
    if (in({ ".wav", ".mp3", ".ogg", ".flac", ".aac", ".m4a", ".wma" })) return AssetIcon::Audio;
    if (in({ ".mp4", ".wmv", ".mov", ".avi" })) return AssetIcon::Video;
    if (extension == ".json") return AssetIcon::Json;
    if (extension == ".mat") return AssetIcon::Material;
    if (extension == ".prefab") return AssetIcon::Prefab;
    if (extension == ".as") return AssetIcon::Script;
    if (in({ ".hlsl", ".hlsli" })) return AssetIcon::Shader;
    if (in({ ".ttf", ".otf", ".fnt" })) return AssetIcon::Font;
    return AssetIcon::File;
}

void GetAssetIconUV(AssetIcon icon, ImVec2 &uv0, ImVec2 &uv1) {
    LogScope scope;
    const unsigned int index = static_cast<unsigned int>(icon);
    const float column = static_cast<float>(index % static_cast<unsigned int>(kAssetIconAtlasColumns));
    const float row = static_cast<float>(index / static_cast<unsigned int>(kAssetIconAtlasColumns));
    uv0 = ImVec2(column / kAssetIconAtlasColumns, row / kAssetIconAtlasRows);
    uv1 = ImVec2((column + 1.0f) / kAssetIconAtlasColumns, (row + 1.0f) / kAssetIconAtlasRows);
}

/// @brief 論理パス（"Assets/..." 形式）を、TextureManagerなどが管理する
///        Assetsルートからの相対パスへ変換する（先頭の "Assets/" を取り除く）
std::string ToAssetsRelativePath(const std::string &logicalPath) {
    LogScope scope;
    constexpr std::string_view kAssetsPrefix = "Assets/";
    if (logicalPath.rfind(kAssetsPrefix, 0) == 0) {
        return logicalPath.substr(kAssetsPrefix.size());
    }
    return logicalPath;
}

bool IsTextureExtension(const std::string &ext) {
    LogScope scope;
    static const std::array<const char *, 11> kExts = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".hdr", ".tif", ".tiff", ".gif", ".webp",
    };
    return std::find_if(kExts.begin(), kExts.end(), [&ext](const char *s) { return ext == s; }) != kExts.end();
}

bool IsAudioExtension(const std::string &ext) {
    LogScope scope;
    static const std::array<const char *, 7> kExts = {
        ".wav", ".mp3", ".ogg", ".flac", ".aac", ".m4a", ".wma",
    };
    return std::find_if(kExts.begin(), kExts.end(), [&ext](const char *s) { return ext == s; }) != kExts.end();
}

bool IsModelExtension(const std::string &ext) {
    LogScope scope;
    static const std::array<const char *, 10> kExts = {
        ".fbx", ".obj", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply", ".stl", ".x",
    };
    return std::find_if(kExts.begin(), kExts.end(), [&ext](const char *s) { return ext == s; }) != kExts.end();
}

bool IsVideoExtension(const std::string &ext) {
    LogScope scope;
    static const std::array<const char *, 4> kExts = {
        ".mp4", ".wmv", ".mov", ".avi",
    };
    return std::find_if(kExts.begin(), kExts.end(), [&ext](const char *s) { return ext == s; }) != kExts.end();
}

bool PathExistsNoThrow(const std::filesystem::path &path) {
    LogScope scope;
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

/// @brief このウィンドウが扱うパス（プロジェクトルート基準の論理パス）を、
///        実際にファイル操作を行える物理パスへ変換する
/// @details 空文字はプロジェクトルート自身を指す
std::filesystem::path ToPhysicalPath(const std::string &projectRelativePath) {
    LogScope scope;
    return Utf8StringToPath(projectRelativePath.empty()
        ? ProjectPaths::ProjectRoot()
        : ProjectPaths::ToPhysical(projectRelativePath));
}

/// @brief スクリプトファイルを外部エディター（VSCodeのcodeコマンド）で開く
/// @details codeコマンドはVSCodeインストール時に「PATHへ追加」が有効な場合のみ使える。
///          見つからない/起動に失敗した場合はエラーログのみ出し、例外は投げない
/// @details ファイル単体ではなくプロジェクトルートをワークスペースとして開く（-gでファイルへジャンプ）。
///          こうすることで、プロジェクトルート直下に生成されているas.predefined（コード補完用の型定義）と
///          launch.json（AngelScriptアタッチデバッグ設定、SceneScriptEngine::Initializeが生成）を
///          VSCodeが自動で読み込む状態にできる
void OpenScriptInExternalEditor(const std::string &projectRelativePath) {
    LogScope scope;
    const std::filesystem::path physicalPath = ToPhysicalPath(projectRelativePath);
    const std::filesystem::path projectRootPath = Utf8StringToPath(ProjectPaths::ProjectRoot());
    const std::wstring parameters =
        L"\"" + projectRootPath.wstring() + L"\" -g \"" + physicalPath.wstring() + L"\"";
    // ShellExecuteWは成功時に32より大きい値を返す（ProjectManager.cppのフォルダを開く処理と同じ規約）
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", L"code", parameters.c_str(), nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        Log(Translation("editor.assets.openscript.failed") + projectRelativePath, LogSeverity::Error);
    }
}

/// @brief スクリプトのファイル名（拡張子無し）から、AngelScriptのクラス名として使える識別子を作る
/// @details 英数字とアンダースコア以外はアンダースコアへ置換し、先頭が数字または空になった場合は
///          "Script" を前置する（AngelScriptの識別子規則はC系言語と同様、数字始まり不可のため）
std::string SanitizeScriptClassName(const std::string &fileName) {
    LogScope scope;
    std::string result;
    result.reserve(fileName.size());
    for (const char c : fileName) {
        result += (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ? c : '_';
    }
    if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front()))) {
        result = "Script" + result;
    }
    return result;
}

/// @brief Start/Update/Endのみを定義したデフォルトのスクリプトテンプレート
std::vector<std::string> BuildDefaultScriptTemplate(const std::string &className) {
    LogScope scope;
    return {
        "class " + className + " : ScriptComponentBehavior {",
        "    void Start() {",
        "    }",
        "",
        "    void Update() {",
        "    }",
        "",
        "    void End() {",
        "    }",
        "}",
    };
}

/// @brief デフォルト一式に加え、当たり判定イベント（OnCollisionEnter/Stay/Exit）も定義したテンプレート
/// @details Trigger（IsTrigger）扱いのコライダーも同じOnCollisionEnter/Stay/Exitで通知される
///          （このエンジンにOnTriggerEnter等の別イベントは存在しない）
std::vector<std::string> BuildCollisionScriptTemplate(const std::string &className) {
    LogScope scope;
    return {
        "class " + className + " : ScriptComponentBehavior {",
        "    void Start() {",
        "    }",
        "",
        "    void Update() {",
        "    }",
        "",
        "    void End() {",
        "    }",
        "",
        "    void OnCollisionEnter(const HitInfo &in hit) {",
        "    }",
        "",
        "    void OnCollisionStay(const HitInfo &in hit) {",
        "    }",
        "",
        "    void OnCollisionExit(const HitInfo &in hit) {",
        "    }",
        "}",
    };
}

/// @brief 選択されたテンプレート種別に応じたスクリプト本文を組み立てる
std::vector<std::string> BuildScriptTemplate(AssetsWindow::ScriptTemplate scriptTemplate, const std::string &className) {
    LogScope scope;
    switch (scriptTemplate) {
    case AssetsWindow::ScriptTemplate::Collision:
        return BuildCollisionScriptTemplate(className);
    case AssetsWindow::ScriptTemplate::Default:
    default:
        return BuildDefaultScriptTemplate(className);
    }
}
} // namespace

AssetsWindow::AssetsWindow(Passkey<SceneEditor>, SceneEditorContext *editorContext)
    : editorContext_(editorContext) {
    LogScope scope;
    sActiveInstance_ = this;
    RefreshFolderTree();
}

AssetsWindow::~AssetsWindow() {
    LogScope scope;
    if (sActiveInstance_ == this) sActiveInstance_ = nullptr;
}

void AssetsWindow::HandleDroppedFiles(const std::vector<std::string> &physicalPaths) {
    LogScope scope;
    if (sActiveInstance_) sActiveInstance_->ImportDroppedFiles(physicalPaths);
}

void AssetsWindow::ShowImGui() {
    LogScope scope;
    if (!ImGui::Begin(TranslationLabel("editor.assets.window"))) {
        ImGui::End();
        return;
    }
    DrawFloatingWindowChromeButtons();

    //--------- ツールバー ---------//
    ImGui::BeginDisabled(currentFolder_.empty());
    if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
        NavigateToFolder(GetParentFolder(currentFolder_));
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TranslationC("editor.assets.up"));
    }
    ImGui::SameLine();
    if (ImGui::Button(TranslationLabel("editor.common.refresh"))) {
        RefreshFolderTree();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(currentFolder_.empty() ? TranslationC("editor.assets.folder.root") : currentFolder_.c_str());
    ImGui::Separator();

    //--------- 左：フォルダツリー ---------//
    ImGui::BeginChild("AssetsFolderTree", ImVec2(220.0f, 0.0f), true);
    ShowFolderNode(rootFolder_);
    ImGui::EndChild();
    // 移動先までの強制展開・スクロールは1フレームだけ適用すればよいので、描画後にクリアする
    forceOpenFolders_.clear();
    pendingScrollToFolder_.clear();

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
    ShowCreateScriptModal();
    ShowCreateFolderModal();
    ShowRenameModal();
    ShowDeleteConfirmModal();

    //--------- ダブルクリックで開いた編集/プレビューウィンドウ ---------//
    ShowOpenEditors();
}

bool AssetsWindow::IsSupportedExtension(const std::string &ext) {
    LogScope scope;
    static const std::array<const char *, 43> kSupported = {
        // テクスチャ（TextureManager対応形式）
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds", ".hdr", ".tif", ".tiff", ".gif", ".webp",
        // モデル（ModelManager対応形式）
        ".fbx", ".obj", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply", ".stl", ".x",
        // サウンド（AudioManager対応形式）
        ".wav", ".mp3", ".ogg", ".flac", ".aac", ".m4a", ".wma",
        // 動画（VideoManager対応形式）
        ".mp4", ".wmv", ".mov", ".avi",
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
        ".ttf", ".otf", ".fnt", ".csv", ".txt",
    };
    return std::find_if(kSupported.begin(), kSupported.end(),
        [&ext](const char *s) { return ext == s; }) != kSupported.end();
}

std::string AssetsWindow::ToLowerExtension(const std::filesystem::path &path) {
    LogScope scope;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

void AssetsWindow::RefreshFolderTree() {
    LogScope scope;
    rootFolder_ = FolderNode{};
    rootFolder_.name = Translation("editor.assets.folder.root");
    rootFolder_.path = "";
    BuildFolderNode(rootFolder_);
    RefreshFileList();
}

void AssetsWindow::BuildFolderNode(FolderNode &node) {
    LogScope scope;
    std::error_code ec;
    const std::filesystem::path base = ToPhysicalPath(node.path);
    for (const auto &entry : std::filesystem::directory_iterator(
            base, std::filesystem::directory_options::skip_permission_denied, ec)) {
        std::error_code statusError;
        const auto status = entry.symlink_status(statusError);
        // シンボリックリンク／ジャンクションを再帰すると循環ディレクトリで無限再帰し得るため除外する。
        if (statusError || !std::filesystem::is_directory(status) || std::filesystem::is_symlink(status)) continue;
        const std::string name = PathToUtf8String(entry.path().filename());
        // 隠しフォルダは表示しない
        if (!name.empty() && name.front() == '.') continue;
        auto child = std::make_unique<FolderNode>();
        child->name = name;
        child->path = ProjectPaths::ToLogical(PathToUtf8String(entry.path()));
        BuildFolderNode(*child);
        node.children.push_back(std::move(child));
    }
    std::sort(node.children.begin(), node.children.end(),
        [](const auto &a, const auto &b) { return a->name < b->name; });
}

void AssetsWindow::RefreshFileList() {
    LogScope scope;
    files_.clear();
    std::error_code ec;
    const std::filesystem::path base = ToPhysicalPath(currentFolder_);
    for (const auto &entry : std::filesystem::directory_iterator(
            base, std::filesystem::directory_options::skip_permission_denied, ec)) {
        FileEntry file;
        file.name = PathToUtf8String(entry.path().filename());
        if (!file.name.empty() && file.name.front() == '.') continue;
        file.path = ProjectPaths::ToLogical(PathToUtf8String(entry.path()));
        std::error_code statusError;
        const auto status = entry.status(statusError);
        if (statusError) continue;
        if (std::filesystem::is_directory(status)) {
            file.isFolder = true;
        } else if (std::filesystem::is_regular_file(status)) {
            file.extension = ToLowerExtension(entry.path());
            if (!IsSupportedExtension(file.extension)) continue;
        } else {
            continue;
        }
        files_.push_back(std::move(file));
    }
    // フォルダを先、その後は名前順（Unityと同じ並び）
    std::sort(files_.begin(), files_.end(), [](const FileEntry &a, const FileEntry &b) {
        if (a.isFolder != b.isFolder) return a.isFolder;
        return a.name < b.name;
    });
}

void AssetsWindow::NavigateToFolder(const std::string &path) {
    LogScope scope;
    currentFolder_ = path;
    RefreshFileList();

    // 左のツリーを現在のフォルダに同期させる: 移動先までの経路（祖先すべて）を
    // 強制的に開き、移動先の項目までスクロールする（ヒエラルキーの選択同期と同じ考え方）
    forceOpenFolders_.clear();
    size_t start = 0;
    while (start < path.size()) {
        const size_t slash = path.find('/', start);
        const size_t end = (slash == std::string::npos) ? path.size() : slash;
        forceOpenFolders_.insert(path.substr(0, end));
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    pendingScrollToFolder_ = path;
}

std::string AssetsWindow::GetParentFolder(const std::string &path) {
    LogScope scope;
    const size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return "";
    return path.substr(0, pos);
}

void AssetsWindow::ShowFolderNode(FolderNode &node) {
    LogScope scope;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (node.path == currentFolder_) flags |= ImGuiTreeNodeFlags_Selected;
    if (node.path.empty()) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    if (!node.path.empty() && forceOpenFolders_.contains(node.path)) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    const bool isOpen = ImGui::TreeNodeEx(node.name.c_str(), flags);
    if (!node.path.empty() && node.path == pendingScrollToFolder_) {
        ImGui::SetScrollHereY(0.5f);
    }
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        NavigateToFolder(node.path);
    }
    if (isOpen) {
        for (auto &child : node.children) {
            ShowFolderNode(*child);
        }
        ImGui::TreePop();
    }
}

void AssetsWindow::ShowFileGrid() {
    LogScope scope;
    constexpr float kCellSize = 96.0f;
    constexpr float kThumbnailSize = 64.0f;
    const float availWidth = ImGui::GetContentRegionAvail().x;
    const int columns = std::max(1, static_cast<int>(availWidth / kCellSize));

    D3D12_GPU_DESCRIPTOR_HANDLE iconAtlasSrvHandle{};
    const auto iconAtlasTextureHandle = TextureManager::GetTextureFromAssetPath(kAssetIconAtlasPath);
    if (iconAtlasTextureHandle != TextureManager::kInvalidHandle) {
        iconAtlasSrvHandle = TextureManager::GetTextureView(iconAtlasTextureHandle).GetSrvHandle();
    }

    int index = 0;
    for (const auto &file : files_) {
        ImGui::PushID(file.path.c_str());
        if (index % columns != 0) ImGui::SameLine();
        ImGui::BeginGroup();

        const ImVec2 thumbnailSize(kThumbnailSize, kThumbnailSize);
        const float offsetX = (kCellSize - kThumbnailSize) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

        bool activated = false;
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle{};
        ImVec2 uv0(0.0f, 0.0f);
        ImVec2 uv1(1.0f, 1.0f);
        if (!file.isFolder) {
            // 読み込み済みテクスチャの場合はサムネイルを表示する
            // file.path はプロジェクトルートからの論理パス（例: "Assets/Materials/White.png"）だが、
            // TextureManager 側は Assets ルートからの相対パス（例: "Materials/White.png"）で管理しているため、
            // 先頭の "Assets/" を取り除いてから問い合わせる必要がある
            const auto textureHandle = TextureManager::GetTextureFromAssetPath(ToAssetsRelativePath(file.path));
            if (textureHandle != TextureManager::kInvalidHandle) {
                srvHandle = TextureManager::GetTextureView(textureHandle).GetSrvHandle();
            }
        }

        // テクスチャ以外は、ファイル種別に対応した生成済みアイコンをアトラスから表示する。
        // フォルダも同じアトラスを使うことで、従来の単色矩形を描画しない。
        if (srvHandle.ptr == 0 && iconAtlasSrvHandle.ptr != 0) {
            srvHandle = iconAtlasSrvHandle;
            GetAssetIconUV(file.isFolder ? AssetIcon::Folder : GetAssetIcon(file.extension), uv0, uv1);
        }

        if (srvHandle.ptr != 0) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 24));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 48));
            activated = ImGui::ImageButton("##thumb", static_cast<ImTextureID>(srvHandle.ptr),
                thumbnailSize, uv0, uv1);
            ImGui::PopStyleColor(3);
        } else {
            // アイコンアセットの読み込みに失敗しても操作領域は維持し、色付き矩形へは戻さない。
            activated = ImGui::InvisibleButton("##missingIcon", thumbnailSize);
        }
        (void)activated;

        // テクスチャ/動画/マテリアル/スクリプト/プレハブファイルはD&Dでコンポーネントのフィールド指定や
        // シーンへの配置ができるようにする
        if (!file.isFolder && (IsTextureExtension(file.extension) || IsVideoExtension(file.extension) ||
            file.extension == ".mat" || file.extension == ".as" || file.extension == ".prefab")) {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                if (IsTextureExtension(file.extension)) {
                    SetAssetDragDropPayload(kTextureAssetDragDropType, ToAssetsRelativePath(file.path));
                } else if (IsVideoExtension(file.extension)) {
                    SetAssetDragDropPayload(kVideoAssetDragDropType, ToAssetsRelativePath(file.path));
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
            NavigateToFolder(file.path);
            ImGui::EndGroup();
            ImGui::PopID();
            break;
        }
        // ファイルはダブルクリックで拡張子に応じた編集/プレビューウィンドウを開く
        if (!file.isFolder && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            OpenFileEditor(file);
        }
        ShowFileContextMenu(file);
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
        ImGui::TextUnformatted(TranslationC("editor.assets.nofiles"));
    }

    ShowGridBackgroundContextMenu();
}

void AssetsWindow::ShowFileContextMenu(const FileEntry &file) {
    LogScope scope;
    if (ImGui::BeginPopupContextItem("FileContextMenu")) {
        if (!file.isFolder) {
            if (ImGui::MenuItem(TranslationLabel("editor.common.open"))) {
                OpenFileEditor(file);
            }
            ImGui::Separator();
        }
        if (ImGui::MenuItem(TranslationLabel("editor.common.rename"))) {
            contextMenuTargetPath_ = file.path;
            contextMenuTargetIsFolder_ = file.isFolder;
            renameBuffer_ = file.name;
            isRenameRequested_ = true;
        }
        if (ImGui::MenuItem(TranslationLabel("editor.common.delete"))) {
            contextMenuTargetPath_ = file.path;
            contextMenuTargetIsFolder_ = file.isFolder;
            isDeleteRequested_ = true;
        }
        ImGui::EndPopup();
    }
}

void AssetsWindow::ShowGridBackgroundContextMenu() {
    LogScope scope;
    // ImGuiPopupFlags_NoOpenOverItems を指定しないと、ファイル項目上での右クリックでも
    // この window レベルのメニューが同一フレームで開いてしまい、
    // ファイル自体の FileContextMenu を閉じてしまう（表示されないように見える）ため必須。
    if (ImGui::BeginPopupContextWindow("AssetsGridContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem(TranslationLabel("editor.assets.createfolder"))) {
            newFolderName_ = "New Folder";
            isCreateFolderRequested_ = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(TranslationLabel("editor.assets.createjson"))) {
            createFileExtension_ = ".json";
            newFileName_ = "New File";
            isCreateFileRequested_ = true;
        }
        if (ImGui::MenuItem(TranslationLabel("editor.assets.creatematerial"))) {
            createFileExtension_ = ".mat";
            newFileName_ = "New Material";
            isCreateFileRequested_ = true;
        }
        if (ImGui::BeginMenu(TranslationLabel("editor.assets.createscript"))) {
            if (ImGui::MenuItem(TranslationLabel("editor.assets.createscript.default"))) {
                pendingScriptTemplate_ = ScriptTemplate::Default;
                newScriptName_ = "NewScript";
                isCreateScriptRequested_ = true;
            }
            if (ImGui::MenuItem(TranslationLabel("editor.assets.createscript.collision"))) {
                pendingScriptTemplate_ = ScriptTemplate::Collision;
                newScriptName_ = "NewScript";
                isCreateScriptRequested_ = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
}

void AssetsWindow::OpenFileEditor(const FileEntry &file) {
    LogScope scope;
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
    } else if (file.extension == ".as") {
        OpenScriptInExternalEditor(file.path);
    } else if (file.extension == ".gif") {
        const std::string assetPath = ToAssetsRelativePath(file.path);
        for (auto &editor : gifPreviews_) {
            if (editor && editor->GetAssetPath() == assetPath) return;
        }
        gifPreviews_.push_back(std::make_unique<GifPreviewWindow>(assetPath));
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
    } else if (IsVideoExtension(file.extension)) {
        const std::string assetPath = ToAssetsRelativePath(file.path);
        for (auto &editor : videoPreviews_) {
            if (editor && editor->GetAssetPath() == assetPath) return;
        }
        videoPreviews_.push_back(std::make_unique<VideoPreviewWindow>(assetPath));
    } else if (IsModelExtension(file.extension)) {
        const std::string assetPath = ToAssetsRelativePath(file.path);
        for (auto &editor : modelPreviews_) {
            if (editor && editor->GetAssetPath() == assetPath) return;
        }
        modelPreviews_.push_back(std::make_unique<ModelPreviewWindow>(assetPath));
    }
}

void AssetsWindow::ShowOpenEditors() {
    LogScope scope;
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
    prune(gifPreviews_);
    prune(audioPreviews_);
    prune(videoPreviews_);

    // ModelPreviewWindowはプレビュー用オブジェクトの生成先（現在編集中のシーン）が必要なため、
    // 他の（アセットファイル単体で完結する）プレビューウィンドウとは別に扱う
    SceneContext *sceneContext = editorContext_ ? editorContext_->GetSceneContext() : nullptr;
    for (size_t i = 0; i < modelPreviews_.size();) {
        if (modelPreviews_[i] && modelPreviews_[i]->ShowImGui(sceneContext)) {
            ++i;
        } else {
            modelPreviews_.erase(modelPreviews_.begin() + i);
        }
    }
}

void AssetsWindow::CloseEditorsForPath(const std::string &cwdRelativePath) {
    LogScope scope;
    jsonEditors_.erase(std::remove_if(jsonEditors_.begin(), jsonEditors_.end(),
        [&](const auto &editor) { return editor && editor->GetFilePath() == cwdRelativePath; }), jsonEditors_.end());

    const std::string assetPath = ToAssetsRelativePath(cwdRelativePath);
    materialEditors_.erase(std::remove_if(materialEditors_.begin(), materialEditors_.end(),
        [&](const auto &editor) { return editor && editor->GetAssetPath() == assetPath; }), materialEditors_.end());
    imagePreviews_.erase(std::remove_if(imagePreviews_.begin(), imagePreviews_.end(),
        [&](const auto &editor) { return editor && editor->GetAssetPath() == assetPath; }), imagePreviews_.end());
    audioPreviews_.erase(std::remove_if(audioPreviews_.begin(), audioPreviews_.end(),
        [&](const auto &editor) { return editor && editor->GetAssetPath() == assetPath; }), audioPreviews_.end());
    videoPreviews_.erase(std::remove_if(videoPreviews_.begin(), videoPreviews_.end(),
        [&](const auto &editor) { return editor && editor->GetAssetPath() == assetPath; }), videoPreviews_.end());
    modelPreviews_.erase(std::remove_if(modelPreviews_.begin(), modelPreviews_.end(),
        [&](const auto &editor) { return editor && editor->GetAssetPath() == assetPath; }), modelPreviews_.end());
}

void AssetsWindow::CreatePrefabFromObject(EmptyObject *obj) {
    LogScope scope;
    if (!obj || !editorContext_) return;

    // このPrefab資産の固有ID。対象が既に別Prefabのインスタンスだったとしても、
    // 新しく作るPrefabは元のリンクと無関係な独立した資産になる（BuildPrefabJson側でリンク情報を除去する）
    const UUID128 prefabID(true);
    const JSON prefabJson = PrefabUtility::BuildPrefabJson(editorContext_, obj, prefabID);
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
    for (int suffix = 1; PathExistsNoThrow(ToPhysicalPath(filePath)); ++suffix) {
        filePath = folder + baseName + "_" + std::to_string(suffix) + PrefabUtility::kPrefabExtension;
    }

    if (!PrefabAssetManager::CreatePrefabFile(prefabID, prefabJson, filePath)) return;

    // 対象オブジェクトを、今作成したPrefabのインスタンスとしてリンクする
    if (commands_) {
        commands_->Execute(std::make_unique<AddComponentCommand>(obj, "PrefabInstanceComponent"));
    } else {
        obj->AddComponent(CreateObjectComponentByType("PrefabInstanceComponent"));
    }
    if (auto *comp = obj->GetComponent<PrefabInstanceComponent>()) {
        comp->SetPrefabID(prefabID);
    }

    RefreshFileList();
}

void AssetsWindow::ImportDroppedFiles(const std::vector<std::string> &physicalPaths) {
    LogScope scope;
    bool anyImported = false;
    const std::string folder = currentFolder_.empty() ? "" : (currentFolder_ + "/");

    for (const auto &srcPathStr : physicalPaths) {
        const std::filesystem::path srcPath = Utf8StringToPath(srcPathStr);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(srcPath, ec) || ec) continue; // フォルダ等は無視

        const std::string fileName = PathToUtf8String(srcPath.filename());
        const std::string stem = PathToUtf8String(srcPath.stem());
        const std::string extWithDot = PathToUtf8String(srcPath.extension());

        // 既存ファイルと重複しない名前を付ける（Prefab書き出しと同様に連番を付与する）
        std::string destLogicalPath = folder + fileName;
        for (int suffix = 1; PathExistsNoThrow(ToPhysicalPath(destLogicalPath)); ++suffix) {
            destLogicalPath = folder + stem + "_" + std::to_string(suffix) + extWithDot;
        }

        const std::filesystem::path destPhysicalPath = ToPhysicalPath(destLogicalPath);
        std::filesystem::copy_file(srcPath, destPhysicalPath, std::filesystem::copy_options::none, ec);
        if (ec) continue;
        anyImported = true;

        // Assets以下に取り込まれた場合のみ、拡張子に応じてAssetManagerへ動的登録する
        // （エンジン起動時にAssetsフォルダをスキャンして読み込む処理と同じ経路を通す）
        // TextureManager/AudioManager/ModelManagerのLoad系は、AssetsRoot()を基準にした
        // 物理パス（例: "C:/.../Projects/MyGame/Assets/Foo/bar.png"）をそのまま受け取る設計のため、
        // 論理パス側で剥がした "Assets/" 以降の断片ではなく物理パスを渡す必要がある
        if (destLogicalPath.rfind("Assets/", 0) == 0) {
            const std::string physicalPathStr = PathToUtf8String(destPhysicalPath);
            const std::string ext = ToLowerExtension(destPhysicalPath);
            if (IsTextureExtension(ext)) {
                TextureManager::LoadTextureDynamic(physicalPathStr);
                if (ext == ".gif") {
                    GifManager::LoadDynamic(physicalPathStr);
                }
            } else if (IsAudioExtension(ext)) {
                AudioManager::LoadDynamic(physicalPathStr);
            } else if (IsModelExtension(ext)) {
                ModelManager::LoadModelDynamic(physicalPathStr);
            } else if (IsVideoExtension(ext)) {
                VideoManager::LoadDynamic(physicalPathStr);
            }
        }
    }

    if (anyImported) RefreshFileList();
}

bool AssetsWindow::ShowCreateFileModal() {
    LogScope scope;
    bool created = false;
    if (isCreateFileRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.assets.createfile.title"));
        isCreateFileRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.assets.createfile.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s%s", TranslationC("editor.assets.createfile.extension"), createFileExtension_.c_str());
        ImGui::InputText(TranslationLabel("editor.assets.createfile.name"), &newFileName_);
        const std::string folder = currentFolder_.empty() ? "" : (currentFolder_ + "/");
        const std::string fullPath = folder + newFileName_ + createFileExtension_;
        const bool alreadyExists = !newFileName_.empty() && PathExistsNoThrow(ToPhysicalPath(fullPath));
        if (alreadyExists) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", TranslationC("editor.assets.createfile.alreadyexists"));
        }
        ImGui::BeginDisabled(newFileName_.empty() || alreadyExists);
        if (ImGui::Button(TranslationLabel("editor.common.create"), ImVec2(120, 0))) {
            bool succeeded = false;
            if (createFileExtension_ == ".mat") {
                MaterialManager::Material material{};
                material.name = newFileName_;
                const auto handle = MaterialManager::RegisterMaterial(newFileName_, material, ProjectPaths::ToPhysical(fullPath));
                succeeded = (handle != MaterialManager::kInvalidHandle) && MaterialManager::SaveMaterial(handle);
            } else {
                succeeded = SaveJSON(JSON::object(), ProjectPaths::ToPhysical(fullPath));
            }
            if (succeeded) {
                RefreshFileList();
                created = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.common.cancel"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return created;
}

bool AssetsWindow::ShowCreateScriptModal() {
    LogScope scope;
    bool created = false;
    if (isCreateScriptRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.assets.createscript.title"));
        isCreateScriptRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.assets.createscript.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText(TranslationLabel("editor.assets.createscript.name"), &newScriptName_);
        const std::string folder = currentFolder_.empty() ? "" : (currentFolder_ + "/");
        const std::string fullPath = folder + newScriptName_ + ".as";
        const bool alreadyExists = !newScriptName_.empty() && PathExistsNoThrow(ToPhysicalPath(fullPath));
        if (alreadyExists) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", TranslationC("editor.assets.createfile.alreadyexists"));
        }
        ImGui::BeginDisabled(newScriptName_.empty() || alreadyExists);
        if (ImGui::Button(TranslationLabel("editor.common.create"), ImVec2(120, 0))) {
            TextFileData textFile{};
            textFile.filePath = ProjectPaths::ToPhysical(fullPath);
            textFile.lines = BuildScriptTemplate(pendingScriptTemplate_, SanitizeScriptClassName(newScriptName_));
            SaveTextFile(textFile);
            if (PathExistsNoThrow(ToPhysicalPath(fullPath))) {
                RefreshFileList();
                created = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.common.cancel"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return created;
}

void AssetsWindow::ShowCreateFolderModal() {
    LogScope scope;
    if (isCreateFolderRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.assets.createfolder.title"));
        isCreateFolderRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.assets.createfolder.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText(TranslationLabel("editor.assets.createfolder.name"), &newFolderName_);
        const std::string folder = currentFolder_.empty() ? "" : (currentFolder_ + "/");
        const std::string fullPath = folder + newFolderName_;
        const bool alreadyExists = !newFolderName_.empty() && PathExistsNoThrow(ToPhysicalPath(fullPath));
        if (alreadyExists) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", TranslationC("editor.assets.createfolder.alreadyexists"));
        }
        ImGui::BeginDisabled(newFolderName_.empty() || alreadyExists);
        if (ImGui::Button(TranslationLabel("editor.common.create"), ImVec2(120, 0))) {
            std::error_code ec;
            if (std::filesystem::create_directory(ToPhysicalPath(fullPath), ec)) {
                RefreshFolderTree();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.common.cancel"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AssetsWindow::ShowRenameModal() {
    LogScope scope;
    if (isRenameRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.assets.rename.title"));
        isRenameRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.assets.rename.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(contextMenuTargetPath_.c_str());
        ImGui::InputText(TranslationLabel("editor.assets.rename.newname"), &renameBuffer_);
        ImGui::BeginDisabled(renameBuffer_.empty());
        if (ImGui::Button(TranslationLabel("editor.common.rename"), ImVec2(120, 0))) {
            const std::filesystem::path oldPath = ToPhysicalPath(contextMenuTargetPath_);
            const std::filesystem::path newPath = oldPath.parent_path() / Utf8StringToPath(renameBuffer_);
            std::error_code ec;
            std::filesystem::rename(oldPath, newPath, ec);
            if (!ec) {
                if (contextMenuTargetIsFolder_) {
                    // フォルダの場合は中身のファイルパスをアセットマネージャーへ個別に
                    // 追従させる仕組みがないため、ツリー自体を再構築するだけに留める
                    if (currentFolder_ == contextMenuTargetPath_) {
                        currentFolder_ = ProjectPaths::ToLogical(PathToUtf8String(newPath));
                    }
                    RefreshFolderTree();
                } else {
                    // 実ファイルのリネームに成功したら、対応するマネージャーの登録名/パスも追従させる
                    // （リネーム前に既に読み込まれていなかった場合は各RenameXxxは何もせずfalseを返すだけ）
                    const std::string oldLogicalPath = ProjectPaths::ToLogical(PathToUtf8String(oldPath));
                    const std::string newLogicalPath = ProjectPaths::ToLogical(PathToUtf8String(newPath));
                    const std::string oldAssetPath = ToAssetsRelativePath(oldLogicalPath);
                    const std::string newAssetPath = ToAssetsRelativePath(newLogicalPath);
                    const std::string ext = ToLowerExtension(newPath);
                    if (IsTextureExtension(ext)) {
                        TextureManager::RenameTexture(oldAssetPath, newAssetPath);
                        if (ext == ".gif") {
                            GifManager::RenameGif(oldAssetPath, newAssetPath);
                        }
                    } else if (ext == ".mat") {
                        MaterialManager::RenameMaterialFile(oldAssetPath, newAssetPath);
                    } else if (IsAudioExtension(ext)) {
                        AudioManager::RenameSound(oldAssetPath, newAssetPath);
                    } else if (IsModelExtension(ext)) {
                        ModelManager::RenameModel(oldAssetPath, newAssetPath);
                    } else if (IsVideoExtension(ext)) {
                        VideoManager::RenameVideo(oldAssetPath, newAssetPath);
                    } else if (ext == PrefabUtility::kPrefabExtension) {
                        PrefabAssetManager::RenamePrefabFile(oldLogicalPath, newLogicalPath);
                    }
                    // 古いパスを指している開いている編集/プレビューウィンドウは無効になるため閉じる
                    CloseEditorsForPath(contextMenuTargetPath_);
                    RefreshFileList();
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.common.cancel"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AssetsWindow::ShowDeleteConfirmModal() {
    LogScope scope;
    if (isDeleteRequested_) {
        ImGui::OpenPopup(TranslationLabel("editor.assets.delete.title"));
        isDeleteRequested_ = false;
    }
    if (ImGui::BeginPopupModal(TranslationLabel("editor.assets.delete.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s",
            contextMenuTargetIsFolder_ ? TranslationC("editor.assets.delete.confirm.folder") : TranslationC("editor.assets.delete.confirm.file"));
        ImGui::TextUnformatted(contextMenuTargetPath_.c_str());
        ImGui::TextUnformatted(TranslationC("editor.assets.delete.cannotundo"));
        if (ImGui::Button(TranslationLabel("editor.common.delete"), ImVec2(120, 0))) {
            std::error_code ec;
            if (contextMenuTargetIsFolder_) {
                std::filesystem::remove_all(ToPhysicalPath(contextMenuTargetPath_), ec);
                RefreshFolderTree();
            } else {
                std::filesystem::remove(ToPhysicalPath(contextMenuTargetPath_), ec);
                RefreshFileList();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("editor.common.cancel"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
