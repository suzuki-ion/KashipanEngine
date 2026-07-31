#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace KashipanEngine {

/// @brief エンジン共通（全プロジェクト共有）の翻訳ファイルが置かれるフォルダ名
/// @details エンジンルート直下。ここに置いた .json は起動時に自動で全て読み込まれる。
constexpr const char *kGlobalLocalesFolderName = "Locales";

/// @brief プロジェクト固有の翻訳ファイルが置かれるフォルダ名（Assets からの相対）
/// @details エディターから新しい言語を保存する際の既定の保存先に使う。
constexpr const char *kProjectLocalesFolderName = "Locales";

/// @brief 翻訳がどの層に属するか
enum class TranslationLayer {
    /// @brief エンジン共通。全プロジェクトで共有され、エディターからは編集できない
    Global,
    /// @brief プロジェクト固有。アプリケーション側の翻訳で、エディターから編集できる
    Project,
};

/// @brief 翻訳設定ファイル(.json)を読み込む
/// @param filePath 論理パスまたは物理パス
/// @param layer 読み込んだ内容をどの層として扱うか
/// @return 読み込みに成功した場合は true
/// @details 同じ localeCode の翻訳が既に読み込まれている場合、置き換えではなく「重ねる」。
///          同名キーはプロジェクト層が優先されるため、エンジン共通の翻訳へ
///          アプリケーション側から追加も上書きもできる。
///          localeCode と translations は必須。localeName / fontPath は省略でき、
///          省略した場合は既に読み込まれている値がそのまま残る。
bool LoadTranslationFile(const std::string &filePath, TranslationLayer layer = TranslationLayer::Project);

/// @brief エンジンルート直下の Locales/ にある翻訳ファイルを全て読み込む
/// @return 読み込んだファイルの物理パス一覧
/// @details 全プロジェクトで共通のエンジン・エディターの文言がここに入る。
///          EngineSettings.json はプロジェクト内にあり、プロジェクトを決める処理より
///          後にしか読めないため、こちらは設定ではなく配置の規約で解決する。
///          そのおかげでプロジェクト解決中に出るログも翻訳済みで出力できる。
std::vector<std::string> LoadGlobalTranslationFiles();

/// @brief キーに対応する翻訳テキストを取得する
/// @param lang 言語コード
/// @param key 翻訳キー
/// @return 翻訳テキスト。見つからなかった場合はキーをそのまま返す
const std::string &GetTranslationText(const std::string &lang, const std::string &key);

/// @brief キーに対応する翻訳テキストを取得する（現在の言語設定を使用）
/// @param key 翻訳キー
/// @return 翻訳テキスト。見つからなかった場合はキーをそのまま返す
const std::string &GetTranslationText(const std::string &key);

/// @brief キーに対応する翻訳テキストを取得する
/// @param lang 言語コード
/// @param key 翻訳キー
/// @return 翻訳テキスト。見つからなかった場合はキーをそのまま返す
inline const std::string &Translation(const std::string &lang, const std::string &key) { return GetTranslationText(lang, key); }

/// @brief キーに対応する翻訳テキストを取得する（現在の言語設定を使用）
/// @param key 翻訳キー
/// @return 翻訳テキスト。見つからなかった場合はキーをそのまま返す
inline const std::string &Translation(const std::string &key) { return GetTranslationText(key); }

/// @brief キーに対応する翻訳テキストを const char* で取得する（現在の言語設定を使用）
/// @param key 翻訳キー
/// @return 翻訳テキスト。見つからなかった場合はキーをそのまま返す
/// @note ImGui のように const char* を要求するAPIへ直接渡すためのもの。
///       ImGuiのIDに影響する引数（ボタン名やウィンドウ名など）には TranslationLabel を使うこと。
inline const char *TranslationC(const std::string &key) { return GetTranslationText(key).c_str(); }

/// @brief キーに対応する翻訳テキストへID用サフィックス("##<キー>")を付与して取得する
/// @param key 翻訳キー
/// @return "翻訳テキスト##翻訳キー" の形式の文字列
/// @note ImGui はラベル文字列からウィジェットIDを生成するため、翻訳テキストをそのまま渡すと
///       言語を切り替えた際にIDが変わり、ウィンドウ位置やツリーの開閉状態などが失われてしまう。
///       言語に依存しない翻訳キーをID部分に埋め込むことでIDを固定する。
///       表示のみでIDを持たない ImGui::Text 等には TranslationC を使うこと。
const char *TranslationLabel(const std::string &key);

/// @brief TranslationLabel が持つキャッシュを破棄する
/// @details 翻訳テキストを実行中に書き換えた場合、これを呼ばないと古い文字列が表示され続ける。
void InvalidateTranslationLabelCache();

//==================================================
// プロジェクト固有（アプリケーション側）翻訳の編集
//==================================================
// エディターの翻訳エディターから使う。グローバル層は対象外で、参照のみできる。

/// @brief 読み込み済みの言語コード一覧を取得する（昇順）
std::vector<std::string> GetLoadedLanguages();

/// @brief 言語の表示名を取得する（未設定の場合は言語コードをそのまま返す）
const std::string &GetLanguageDisplayName(const std::string &lang);

/// @brief プロジェクト層の翻訳一覧を取得する（編集対象）
const std::unordered_map<std::string, std::string> &GetProjectTranslations(const std::string &lang);

/// @brief グローバル層の翻訳一覧を取得する（参照専用）
const std::unordered_map<std::string, std::string> &GetGlobalTranslations(const std::string &lang);

/// @brief そのキーがエンジン共通の翻訳として存在するか
/// @details プロジェクト側で同じキーを定義すると上書きになるため、その警告に使う。
bool IsGlobalTranslationKey(const std::string &lang, const std::string &key);

/// @brief プロジェクト層へ翻訳を追加・更新する（メモリ上のみ。保存は SaveProjectTranslationFile）
void SetProjectTranslation(const std::string &lang, const std::string &key, const std::string &text);

/// @brief プロジェクト層から翻訳を削除する
/// @return 削除した場合は true
/// @details グローバル層に同じキーがある場合、削除すると元のエンジン側の文言へ戻る。
bool RemoveProjectTranslation(const std::string &lang, const std::string &key);

/// @brief プロジェクト層の保存先（論理パス）を取得する。未読み込みの場合は空
const std::string &GetProjectTranslationFilePath(const std::string &lang);

/// @brief プロジェクト層の翻訳をファイルへ保存する
/// @return 保存に成功した場合は true
/// @details 書き出すのはプロジェクト層のみで、グローバル層の内容は含めない。
bool SaveProjectTranslationFile(const std::string &lang);

/// @brief 現在の言語コードを取得する
/// @return 言語コード
const std::string &GetCurrentLanguage();

/// @brief 現在の言語コードを設定する
void SetCurrentLanguage(const std::string &lang);

/// @brief 現在の言語設定で使用するフォントパスを取得する
const std::string &GetCurrentLanguageFontPath();

} // namespace KashipanEngine
