#include "Translation.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/FileIO/JSON.h"
#include "Core/ProjectPaths.h"
#include <Windows.h>
#include <algorithm>
#include <unordered_map>

namespace KashipanEngine {
namespace {
/// @brief 言語データ構造体
/// @details グローバル層とプロジェクト層を別々に保持しつつ、実際に引かれる値は
///          合成済みの translations から返す。層を分けているのは、エディター上で
///          プロジェクト側の翻訳だけを編集・保存できるようにするため。
struct LanguageData {
    std::string langCode;
    std::string langName;
    std::string fontPath;
    /// @brief グローバル層とプロジェクト層を合成した結果（Translation() が引くのはこれ）
    std::unordered_map<std::string, std::string> translations;
    /// @brief エンジン共通（エンジンルート直下 Locales/）の翻訳。エディターからは編集できない
    std::unordered_map<std::string, std::string> globalTranslations;
    /// @brief プロジェクト固有の翻訳。エディターでの編集対象
    std::unordered_map<std::string, std::string> projectTranslations;
    /// @brief プロジェクト層の保存先（論理パス）。未読み込みの場合は空
    std::string projectFilePath;
};
std::unordered_map<std::string, LanguageData> sLanguageData;

/// @brief グローバル層へプロジェクト層を重ねて、実際に引かれる translations を作り直す
void RebuildMergedTranslations(LanguageData &langData) {
    langData.translations = langData.globalTranslations;
    for (const auto &[key, value] : langData.projectTranslations) {
        langData.translations[key] = value;
    }
}

/// @brief 指定言語のデータを取得する（存在しない場合は nullptr）
LanguageData *FindLanguageData(const std::string &lang) {
    auto it = sLanguageData.find(lang);
    return it == sLanguageData.end() ? nullptr : &it->second;
}

class Language {
public:
    Language() {
        ULONG numLangs = 0;
        WCHAR buffer[256];
        ULONG bufferSize = sizeof(buffer) / sizeof(WCHAR);
        if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLangs, buffer, &bufferSize)) {
            // 読み込めなかった場合は英語をデフォルトにする
            wcscpy_s(buffer, L"en-US");
            numLangs = 1;
        }
        // stringに変換
        language = ConvertString(std::wstring(buffer));
    }
    ~Language() = default;
    Language(const Language &) = delete;
    Language &operator=(const Language &) = delete;
    Language(Language &&) = delete;
    Language &operator=(Language &&) = delete;

    static void Set(const std::string &lang) {
        language = lang;
    }
    static inline const std::string &Get() {
        return language;
    }

private:
    static inline std::string language;
} sLanguage;
} // namespace

bool LoadTranslationFile(const std::string &filePath, TranslationLayer layer) {
    JSON json = LoadJSON(ProjectPaths::ToPhysical(filePath));
    if (json.empty()) {
        Log(Translation("engine.translations.load.failed") + filePath, LogSeverity::Error);
        return false;
    }
    // どの言語への追加なのかが分からないと重ねようがないため、localeCode と translations は必須。
    // localeName / fontPath はエンジン側のファイルが持つ情報なので、
    // 後から重ねるプロジェクト側のファイルでは省略できる。
    if (!json.contains("localeCode") || !json.contains("translations")) {
        Log(Translation("engine.translations.load.failed.missingfields") + filePath, LogSeverity::Error);
        return false;
    }

    const std::string langCode = json["localeCode"].get<std::string>();
    // 既に同じ言語が読み込まれている場合は置き換えずに重ねる。
    // これによりエンジン共通の翻訳へプロジェクト固有の翻訳を追加できる（後から読んだ方が優先）。
    LanguageData &langData = sLanguageData[langCode];
    langData.langCode = langCode;
    if (json.contains("localeName")) langData.langName = json["localeName"].get<std::string>();
    if (json.contains("fontPath")) langData.fontPath = json["fontPath"].get<std::string>();

    // 翻訳データの読み込み（層ごとに分けて保持する）
    auto &targetLayer = (layer == TranslationLayer::Global)
        ? langData.globalTranslations
        : langData.projectTranslations;
    if (layer == TranslationLayer::Project) {
        langData.projectFilePath = filePath;
    }

    size_t addedCount = 0;
    size_t overriddenCount = 0;
    for (auto &[key, value] : json["translations"].items()) {
        const bool overridesOtherLayer = (layer == TranslationLayer::Project)
            ? langData.globalTranslations.contains(key)
            : false;
        auto [it, inserted] = targetLayer.insert_or_assign(key, value.get<std::string>());
        (void)it;
        ++addedCount;
        if (overridesOtherLayer || !inserted) ++overriddenCount;
    }
    RebuildMergedTranslations(langData);

    // 上書きは意図的な差し替えのこともあるためエラーにはしないが、
    // キー名の衝突事故を追えるように件数だけ残す
    if (overriddenCount > 0) {
        Log(Translation("engine.translations.overridden") + std::to_string(overriddenCount) +
            Translation("engine.translations.overridden.file") + filePath, LogSeverity::Debug);
    }

    // 表示名が未設定（プロジェクト側のファイルだけを読んだ場合）は言語コードで代用する
    const std::string &langName = langData.langName.empty() ? langCode : langData.langName;
    Log(Translation("engine.translations.loaded") + langName +
        Translation("engine.translations.loaded.count") + std::to_string(addedCount), LogSeverity::Info);
    return true;
}

std::vector<std::string> LoadGlobalTranslationFiles() {
    // エンジン共通の翻訳はエンジンルート直下の Locales/ に置く決まりにしてある。
    // EngineSettings.json はプロジェクト内にあり、プロジェクトを開くより先に翻訳が必要になる
    // （プロジェクト解決中のログも翻訳したい）ため、設定で場所を指定させずに規約で拾う。
    std::vector<std::string> loadedFiles;
    const std::string localesRoot = ProjectPaths::InEngineRoot(kGlobalLocalesFolderName);
    if (!IsDirectoryExist(localesRoot)) return loadedFiles;

    const DirectoryData directory = GetDirectoryDataByExtension(localesRoot, { ".json" }, false, true);
    // 読み込み順を環境によらず一定にする（同じキーがあった場合の勝敗を安定させるため）
    std::vector<std::string> files = directory.files;
    std::sort(files.begin(), files.end());
    for (const auto &file : files) {
        if (LoadTranslationFile(file, TranslationLayer::Global)) loadedFiles.push_back(file);
    }
    return loadedFiles;
}

std::vector<std::string> GetLoadedLanguages() {
    std::vector<std::string> languages;
    languages.reserve(sLanguageData.size());
    for (const auto &[langCode, data] : sLanguageData) {
        (void)data;
        languages.push_back(langCode);
    }
    std::sort(languages.begin(), languages.end());
    return languages;
}

const std::string &GetLanguageDisplayName(const std::string &lang) {
    const LanguageData *langData = FindLanguageData(lang);
    // 表示名が未設定なら言語コードをそのまま見せる
    return (langData && !langData->langName.empty()) ? langData->langName : lang;
}

const std::unordered_map<std::string, std::string> &GetProjectTranslations(const std::string &lang) {
    static const std::unordered_map<std::string, std::string> kEmpty;
    const LanguageData *langData = FindLanguageData(lang);
    return langData ? langData->projectTranslations : kEmpty;
}

const std::unordered_map<std::string, std::string> &GetGlobalTranslations(const std::string &lang) {
    static const std::unordered_map<std::string, std::string> kEmpty;
    const LanguageData *langData = FindLanguageData(lang);
    return langData ? langData->globalTranslations : kEmpty;
}

bool IsGlobalTranslationKey(const std::string &lang, const std::string &key) {
    const LanguageData *langData = FindLanguageData(lang);
    return langData && langData->globalTranslations.contains(key);
}

void SetProjectTranslation(const std::string &lang, const std::string &key, const std::string &text) {
    if (lang.empty() || key.empty()) return;
    LanguageData &langData = sLanguageData[lang];
    if (langData.langCode.empty()) langData.langCode = lang;
    langData.projectTranslations[key] = text;
    RebuildMergedTranslations(langData);
    InvalidateTranslationLabelCache();
}

bool RemoveProjectTranslation(const std::string &lang, const std::string &key) {
    LanguageData *langData = FindLanguageData(lang);
    if (!langData) return false;
    if (langData->projectTranslations.erase(key) == 0) return false;
    // グローバル層に同じキーがあれば、上書きが外れて元の文言へ戻る
    RebuildMergedTranslations(*langData);
    InvalidateTranslationLabelCache();
    return true;
}

const std::string &GetProjectTranslationFilePath(const std::string &lang) {
    static const std::string kEmpty;
    const LanguageData *langData = FindLanguageData(lang);
    return langData ? langData->projectFilePath : kEmpty;
}

bool SaveProjectTranslationFile(const std::string &lang) {
    LanguageData *langData = FindLanguageData(lang);
    if (!langData) return false;

    // 保存先が未設定（EngineSettings.json に指定が無い言語）の場合は規定の場所を使う
    if (langData->projectFilePath.empty()) {
        langData->projectFilePath =
            std::string(ProjectPaths::kAssetsFolderName) + "/" + kProjectLocalesFolderName + "/" + lang + ".json";
    }

    // 保存対象はプロジェクト層のみ。グローバル層の内容を巻き込んで書き出さないようにする
    JSON json = JSON::object();
    json["localeCode"] = langData->langCode.empty() ? lang : langData->langCode;
    JSON translationsJson = JSON::object();
    // キー順を安定させて差分を読みやすくする
    std::vector<std::string> keys;
    keys.reserve(langData->projectTranslations.size());
    for (const auto &[key, value] : langData->projectTranslations) {
        (void)value;
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    for (const auto &key : keys) {
        translationsJson[key] = langData->projectTranslations.at(key);
    }
    json["translations"] = std::move(translationsJson);

    const std::string physicalPath = ProjectPaths::ToPhysical(langData->projectFilePath);
    if (!SaveJSON(json, physicalPath)) {
        Log(Translation("engine.translations.save.failed") + langData->projectFilePath, LogSeverity::Error);
        return false;
    }
    Log(Translation("engine.translations.save.succeeded") + langData->projectFilePath, LogSeverity::Info);
    return true;
}

const std::string &GetTranslationText(const std::string &lang, const std::string &key) {
    auto langIt = sLanguageData.find(lang);
    if (langIt != sLanguageData.end()) {
        const auto &translations = langIt->second.translations;
        auto transIt = translations.find(key);
        if (transIt != translations.end()) {
            return transIt->second;
        }
    }
    // 指定言語自体が未読込、またはそのキーがまだ翻訳されていない場合は英語へフォールバックする。
    // 翻訳が追いついていない言語や、アプリ側が追加したばかりで一部の言語にしか
    // 存在しないキーでも、生のキー文字列がそのまま画面に出るのを防ぐ。
    if (lang != kFallbackLanguageCode) {
        auto fallbackIt = sLanguageData.find(kFallbackLanguageCode);
        if (fallbackIt != sLanguageData.end()) {
            const auto &fallbackTranslations = fallbackIt->second.translations;
            auto fallbackTransIt = fallbackTranslations.find(key);
            if (fallbackTransIt != fallbackTranslations.end()) {
                return fallbackTransIt->second;
            }
        }
    }
    // 英語にも無い場合の最後の手段。呼び出し側でキーの入力ミスなどに気づけるようにする
    return key;
}

const std::string &GetTranslationText(const std::string &key) {
    return GetTranslationText(sLanguage.Get(), key);
}

namespace {
// 生成した "翻訳テキスト##キー" を使い回す。unordered_map はリハッシュしても
// 要素のアドレスが変わらないため、返した const char* はそのまま保持できる。
std::unordered_map<std::string, std::string> sLabelCache;
std::string sLabelCacheLang;
} // namespace

void InvalidateTranslationLabelCache() {
    sLabelCache.clear();
}

const char *TranslationLabel(const std::string &key) {
    if (sLabelCacheLang != sLanguage.Get()) {
        sLabelCache.clear();
        sLabelCacheLang = sLanguage.Get();
    }
    auto it = sLabelCache.find(key);
    if (it == sLabelCache.end()) {
        it = sLabelCache.emplace(key, GetTranslationText(key) + "##" + key).first;
    }
    return it->second.c_str();
}

const std::string &GetCurrentLanguage() {
    return sLanguage.Get();
}

void SetCurrentLanguage(const std::string &lang) {
    sLanguage.Set(lang);
}

const std::string &GetCurrentLanguageFontPath() {
    static const std::string kEmpty;
    const std::string &lang = sLanguage.Get();
    auto langIt = sLanguageData.find(lang);
    if (langIt != sLanguageData.end() && !langIt->second.fontPath.empty()) {
        return langIt->second.fontPath;
    }
    // 現在の言語が未読込、またはフォントパス未設定（プロジェクト側のファイルのみ読み込んだ場合）は
    // 英語のフォント設定へフォールバックする
    if (lang != kFallbackLanguageCode) {
        auto fallbackIt = sLanguageData.find(kFallbackLanguageCode);
        if (fallbackIt != sLanguageData.end()) {
            return fallbackIt->second.fontPath;
        }
    }
    return kEmpty;
}

} // namespace KashipanEngine