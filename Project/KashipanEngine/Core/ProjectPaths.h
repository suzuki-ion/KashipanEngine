#pragma once
#include <string>
#include <vector>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class ProjectManager;

/// @brief エンジンルート・プロジェクトルートの解決と、論理パスと物理パスの相互変換を行う
/// @details エンジンは「論理パス」と「物理パス」の2種類のパス表記を使い分ける。
///          - 論理パス: "Assets/Application/Model/Player.obj" のように "Assets/" から始まる表記。
///            シーンJSON・マテリアル・スクリプトパスなどデータファイルへ保存されるのはこちらで、
///            どのプロジェクトを開いていても同じ値になる（＝プロジェクト間で持ち運べる）。
///          - 物理パス: "C:/.../Projects/MyGame/Assets/Application/Model/Player.obj" のように
///            実際にファイルを開けるパス。プロジェクトごとに異なる。
///          データに保存するのは常に論理パス、ファイルを開く直前に ToPhysical() で物理パスへ変換する。
class ProjectPaths final {
public:
    /// @brief 論理パスの先頭に付く Assets フォルダ名
    static constexpr const char *kAssetsFolderName = "Assets";
    /// @brief 各プロジェクトが並ぶフォルダ名（エンジンルート直下）
    static constexpr const char *kProjectsFolderName = "Projects";
    /// @brief プロジェクトテンプレートが並ぶフォルダ名（エンジンルート直下）
    /// @details 直下のフォルダ1つが1テンプレートで、その中身が新規プロジェクトの Assets/ になる
    static constexpr const char *kAssetsTemplateFolderName = "AssetsTemplate";
    /// @brief プロジェクト定義ファイル名（プロジェクトルート直下）
    static constexpr const char *kProjectFileName = "Project.json";
    /// @brief エンジンルートの位置を示すマーカーファイル名（exeと同じフォルダに置く）
    static constexpr const char *kEngineRootMarkerFileName = "EngineRoot.txt";

    /// @brief 開くプロジェクトを指定するコマンドライン引数
    /// @details ランチャーやプロジェクトの切り替えによる再起動で使う。
    ///          値にはプロジェクト名（Projects配下のフォルダ名）と絶対パスのどちらも渡せる。
    static constexpr const char *kProjectArgumentName = "--project";

    /// @brief 起動直後に一度だけ呼び、エンジンルートと開くプロジェクトを確定する
    /// @details EngineSettings.json もプロジェクト内にあるため、設定読み込みより先に呼ぶ必要がある。
    static void Initialize(PasskeyForGameEngineMain);

    /// @brief エンジンルートだけを解決する（プロジェクトは開かない）
    /// @details プロジェクトを一覧・作成するだけでエンジンを動かさないランチャーと、
    ///          プロジェクト解決より先にエンジン共通の翻訳を読みたいエンジン起動時に使う。
    ///          これだけで ProjectsRoot() / AssetsTemplateRoot() / InEngineRoot() が使えるようになる。
    ///          この後に Initialize() を呼んでも二重解決にはならない。
    static void InitializeEngineRootOnly(PasskeyForWinMain);

    /// @brief exeが置かれているフォルダ
    static const std::string &ExecutableDirectory();

    /// @brief AssetsTemplate/ と Projects/ が置かれるエンジンのルートフォルダ
    /// @details 配布形態ではexeと同じフォルダ。開発時はexeと同じフォルダに置かれた
    ///          EngineRoot.txt が指すソースツリーのフォルダになる。
    static const std::string &EngineRoot();

    /// @brief 現在開いているプロジェクトのルートフォルダ
    static const std::string &ProjectRoot();

    /// @brief 現在開いているプロジェクトのAssetsフォルダ（物理パス）
    static const std::string &AssetsRoot();

    /// @brief 現在開いているプロジェクトの名前（プロジェクトフォルダ名）
    static const std::string &ProjectName();

    /// @brief exeと同じフォルダにAssetsを直置きした配布形態で動作しているか
    /// @details 配布形態ではプロジェクトの選択・作成・切り替えを行わない。
    static bool IsStandalone();

    /// @brief 有効なプロジェクトが開かれているか（Project.jsonが存在するか）
    static bool HasActiveProject();

    /// @brief 論理パス（プロジェクトルート基準。"Assets/..." など）を実際にファイルを開ける物理パスへ変換する
    /// @details 絶対パスが渡された場合はそのまま返すため、変換済みの値を二重に渡しても安全。
    static std::string ToPhysical(const std::string &logicalPath);

    /// @brief 物理パスを論理パス（プロジェクトルート基準）へ変換する
    /// @details ToPhysical() の逆変換。プロジェクト外のパスが渡された場合は
    ///          区切り文字を正規化して返すだけ。
    static std::string ToLogical(const std::string &physicalPath);

    /// @brief プロジェクトルート基準の相対パスを物理パスへ変換する（EditorSettings.json など）
    static std::string InProjectRoot(const std::string &relativePath);

    /// @brief エンジンルート基準の相対パスを物理パスへ変換する（EditorPreferences.json など）
    static std::string InEngineRoot(const std::string &relativePath);

    /// @brief プロジェクトテンプレートが並ぶフォルダ（物理パス）
    /// @details 個々のテンプレートは、この下の <テンプレート名>/ フォルダ
    static std::string AssetsTemplateRoot();

    /// @brief 全プロジェクトが並ぶフォルダ（物理パス）
    static std::string ProjectsRoot();

    /// @brief 開くプロジェクトを差し替える
    /// @param projectRootPath プロジェクトルートの物理パス
    static void SetActiveProject(Passkey<ProjectManager>, const std::string &projectRootPath);

    /// @brief Assetsフォルダ以下を再帰的に走査し、見つかったファイルを論理パスの一覧で返す
    /// @param extensions 絞り込む拡張子（例: { ".as" }）。空の場合は全ファイルを返す
    /// @return 論理パス（"Assets/..." 形式）のソート済み一覧
    static std::vector<std::string> ListAssetFiles(const std::vector<std::string> &extensions = {});

    /// @brief パス区切りを '/' に統一し、末尾の区切り文字を取り除く
    static std::string NormalizeSeparators(std::string path);

private:
    /// @brief exeの位置とEngineRoot.txtからエンジンルートを決定する
    static void ResolveEngineRoot();

    /// @brief コマンドライン引数・前回開いたプロジェクト・既存プロジェクトの順に
    ///        探して開く対象を決定する
    static void ResolveActiveProject();

    /// @brief コマンドラインの --project に指定された値を取得する（未指定の場合は空文字）
    static std::string GetProjectArgument();

    static inline std::string sExecutableDirectory;
    static inline std::string sEngineRoot;
    static inline std::string sProjectRoot;
    static inline std::string sAssetsRoot;
    static inline std::string sProjectName;
    static inline bool sIsStandalone = false;
    /// @brief エンジンルートを解決済みか（プロジェクトの解決とは独立して先に済ませられる）
    static inline bool sIsEngineRootResolved = false;
    /// @brief 開くプロジェクトまで確定済みか
    static inline bool sIsInitialized = false;
};

} // namespace KashipanEngine
