#pragma once
#ifdef USE_IMGUI
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief Unity の Assets ウィンドウ風のファイルブラウザ
/// @details 実行中のディレクトリ以下から、エンジンが対応している形式のファイルを表示する。
///          左側にフォルダツリー、右側に選択中フォルダ内のファイルをグリッド表示する。
class AssetsWindow final {
public:
    AssetsWindow(Passkey<SceneEditor>);
    ~AssetsWindow() = default;

    void ShowImGui();

private:
    struct FolderNode {
        std::string name;
        std::string path; // 実行ディレクトリからの相対パス（ルートは空文字）
        std::vector<std::unique_ptr<FolderNode>> children;
    };

    struct FileEntry {
        std::string name;
        std::string path;
        std::string extension; // 小文字
        bool isFolder = false;
    };

    /// @brief エンジンが対応している形式か（拡張子は小文字で渡す）
    static bool IsSupportedExtension(const std::string &ext);
    /// @brief 拡張子の分類色（Unityのアイコンの代わり）
    static unsigned int ExtensionColor(const std::string &ext);
    static std::string ToLowerExtension(const std::filesystem::path &path);

    /// @brief フォルダツリーを再構築する
    void RefreshFolderTree();
    void BuildFolderNode(FolderNode &node);
    /// @brief 選択中フォルダのファイル一覧を再取得する
    void RefreshFileList();
    void ShowFolderNode(FolderNode &node);
    void ShowFileGrid();

    FolderNode rootFolder_;
    std::string currentFolder_;
    std::vector<FileEntry> files_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
