#pragma once
#ifdef USE_IMGUI
#include <string>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class SceneEditor;
class SceneEditorContext;

/// @brief 登録シーンの一覧表示・編集・切り替えを行うエディターウィンドウ
/// @details SceneManagerに登録されているシーンの「登録・削除・名前変更・読み込みJSONパスの変更・
///          スタートアップシーンの指定」と、登録シーンへの切り替えを行う。
///          変更内容は即座にシーンリスト定義ファイル（SceneManager::kDefaultSceneListFilePath）へ保存される
class SceneListEditor final {
public:
    SceneListEditor(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}

    void ShowImGui();

private:
    SceneEditorContext *context_ = nullptr;

    /// @brief 新規登録するシーン名の入力バッファ
    std::string newSceneName_;
    /// @brief 新規登録するシーンのファイルパスの入力バッファ
    std::string newSceneFilePath_;

    /// @brief フォルダ形式への変換後、元の単一ファイルを削除するか確認するポップアップの状態
    bool isConfirmDeleteOldFileRequested_ = false;
    /// @brief 削除確認ポップアップで表示・削除対象になる元ファイルのパス
    std::string pendingDeleteOldFilePath_;

    /// @brief シーン複製ポップアップを開く要求フラグ
    bool isDuplicateSceneRequested_ = false;
    /// @brief 複製元のシーン名
    std::string duplicateSourceName_;
    /// @brief 複製先の名前の入力バッファ
    std::string duplicateNewName_;
    /// @brief 複製先のファイルパスの入力バッファ（複製元がファイルパスを持たない場合は使用しない）
    std::string duplicateNewFilePath_;

    /// @brief 登録済みシーンを単一ファイル形式（.json）からフォルダ形式（.scene）へ変換する
    void ConvertSceneToFolderFormat(const std::string &sceneName, const std::string &oldFilePath);
    /// @brief 変換後の削除確認ポップアップを表示する
    void ShowConfirmDeleteOldFilePopup();
    /// @brief シーン複製ポップアップを表示する
    void ShowDuplicateScenePopup();
};

} // namespace KashipanEngine
#endif // USE_IMGUI
