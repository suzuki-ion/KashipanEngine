#pragma once
#ifdef USE_IMGUI
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Utilities/Passkeys.h"
#include "Scene/Editor/AssetEditorWindows.h"

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
    void ShowFileContextMenu(const FileEntry &file);
    void ShowGridBackgroundContextMenu();
    /// @brief ファイルを拡張子に応じた編集/プレビューウィンドウで開く（同じファイルは多重に開かない）
    void OpenFileEditor(const FileEntry &file);
    /// @brief 開いている編集/プレビューウィンドウを表示し、閉じられたものを片付ける
    void ShowOpenEditors();
    /// @brief 指定の実行ディレクトリ相対パスを開いている編集/プレビューウィンドウを閉じる
    /// @details リネームで古いパスが無効になった場合に呼ぶ
    void CloseEditorsForPath(const std::string &cwdRelativePath);
    /// @brief 新規ファイル作成モーダル（.json / .mat）を表示する
    bool ShowCreateFileModal();
    /// @brief リネームモーダルを表示する
    void ShowRenameModal();
    /// @brief 削除確認モーダルを表示する
    void ShowDeleteConfirmModal();

    FolderNode rootFolder_;
    std::string currentFolder_;
    std::vector<FileEntry> files_;

    std::vector<std::unique_ptr<JSONFileEditorWindow>> jsonEditors_;
    std::vector<std::unique_ptr<MaterialFileEditorWindow>> materialEditors_;
    std::vector<std::unique_ptr<ImagePreviewWindow>> imagePreviews_;
    std::vector<std::unique_ptr<AudioPreviewWindow>> audioPreviews_;

    // 新規ファイル作成（空白部の右クリックメニュー）
    bool isCreateFileRequested_ = false;
    std::string createFileExtension_; // ".json" or ".mat"
    std::string newFileName_;

    // ファイルアイコンの右クリックメニュー（リネーム/削除）
    std::string contextMenuTargetPath_;
    bool isRenameRequested_ = false;
    std::string renameBuffer_;
    bool isDeleteRequested_ = false;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
