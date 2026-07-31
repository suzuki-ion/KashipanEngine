#pragma once
#include <string>
#include <vector>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief ゲームプロジェクトの一覧取得・新規作成・切り替えを行う
/// @details プロジェクトはエンジンルート直下の Projects/<名前>/ に1つずつ置かれ、
///          Project.json を持つフォルダをプロジェクトとみなす。
///          新規作成時は AssetsTemplate/ をプロジェクトの Assets/ へコピーして初期状態とする。
class ProjectManager final {
public:
    /// @brief Project.json に記録するフォーマットのバージョン
    /// @details 将来プロジェクトの構成を変えた際に、古いプロジェクトを移行するために使う
    static constexpr int kProjectFormatVersion = 1;

    /// @brief プロジェクト1件分の情報
    struct ProjectInfo {
        /// @brief プロジェクト名（＝プロジェクトフォルダ名）
        std::string name;
        /// @brief プロジェクトルートの物理パス
        std::string rootPath;
        /// @brief プロジェクトの説明（任意）
        std::string description;
        /// @brief Project.json のフォーマットバージョン
        int formatVersion = kProjectFormatVersion;
    };

    /// @brief 起動時に呼び、開くプロジェクトが無ければテンプレートから新規作成する
    /// @return 有効なプロジェクトが開かれた場合は true
    static bool EnsureActiveProject(PasskeyForGameEngineMain);

    /// @brief Projects配下のプロジェクト一覧を取得する（名前順）
    static std::vector<ProjectInfo> GetProjectList();

    /// @brief 現在開いているプロジェクトの情報を取得する
    static ProjectInfo GetActiveProject();

    /// @brief 新規プロジェクトを作成する（AssetsTemplateをコピーしてProject.jsonを生成）
    /// @param name プロジェクト名（フォルダ名として使うため、パス区切りなどは含められない）
    /// @param outErrorMessage 失敗した場合の理由（不要な場合は nullptr）
    /// @return 作成に成功した場合は true
    static bool CreateProject(const std::string &name, std::string *outErrorMessage = nullptr);

    /// @brief 次回起動時に開くプロジェクトを設定する
    static void SetStartupProject(const std::string &name);

    /// @brief 次回起動時に開くプロジェクト名を取得する（未設定の場合は空文字）
    static std::string GetStartupProject();

    /// @brief 指定プロジェクトを開き直すため、再起動の予約だけを行う
    /// @details 実行中のエンジンは読み込み済みのアセットをGPUリソースごと差し替えられないため、
    ///          プロジェクトの切り替えはexeの起動し直しで行う。
    ///          この関数は開き直す対象を記録するだけなので、呼び出し側で続けて
    ///          GameEngine::RequestQuit() を呼び、アプリケーションを終了させること
    ///          （このクラスはランチャーからも使うため、エンジン本体に依存させていない）。
    ///          実際にプロセスを起動するのは後始末がすべて終わったあと（LaunchPendingRestart）。
    /// @return 指定されたプロジェクトが存在し、予約を受け付けた場合は true
    static bool RequestRestartWithProject(const std::string &name);

    /// @brief 再起動が要求されている場合に、新しいプロセスを起動する
    /// @details エンジンの終了処理がすべて完了したあとに一度だけ呼ぶこと。
    /// @return 新しいプロセスを起動した場合は true
    static bool LaunchPendingRestart(PasskeyForGameEngineMain);

    /// @brief 指定プロジェクトでエディターを起動する（ランチャーから使う）
    /// @param name プロジェクト名
    /// @param outErrorMessage 失敗した場合の理由（不要な場合は nullptr）
    /// @return 起動に成功した場合は true
    static bool LaunchEditor(const std::string &name, std::string *outErrorMessage = nullptr);

    /// @brief プロジェクト名として使える文字列かどうかを判定する
    /// @param outErrorMessage 使えない場合の理由（不要な場合は nullptr）
    static bool IsValidProjectName(const std::string &name, std::string *outErrorMessage = nullptr);

private:
    /// @brief 指定フォルダの Project.json を読み込む
    /// @return Project.json が存在しない・壊れている場合は false
    static bool ReadProjectFile(const std::string &projectRootPath, ProjectInfo &outInfo);

    /// @brief Project.json を書き出す
    static bool WriteProjectFile(const ProjectInfo &info);

    /// @brief 指定プロジェクトを開くようエディターのexeを起動する
    static bool StartEditorProcess(const std::string &projectName, std::string *outErrorMessage);

    /// @brief 再起動後に開くプロジェクト名（空の場合は再起動しない）
    static inline std::string sPendingRestartProjectName_;
};

} // namespace KashipanEngine
