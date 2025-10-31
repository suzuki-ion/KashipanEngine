#include "Translation.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Json.h"
#include <Windows.h>
#include <unordered_map>

namespace KashipanEngine {
namespace {
/// @brief 言語データ構造体
struct LanguageData {
    std::string langCode;
    std::string langName;
    std::string fontPath;
    std::unordered_map<std::string, std::string> translations;
};
std::unordered_map<std::string, LanguageData> sLanguageDatas;

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

    static inline const std::string &Get() {
        return language;
    }

private:
    static inline std::string language;
} sLanguage;
} // namespace

bool LoadTranslationFile(const std::string &filePath) {
    Json json = LoadJson(filePath);
    if (json.empty()) {
        Log("Failed to load translation file: " + filePath, LogSeverity::Error);
        return false;
    }
    // 必要な情報が存在するか確認
    if (!json.contains("localeCode") ||
        !json.contains("localeName") ||
        !json.contains("fontPath") ||
        !json.contains("translations")) {
        Log("Translation file is missing required fields: " + filePath, LogSeverity::Error);
        return false;
    }
    LanguageData langData;
    langData.langCode = json["localeCode"].get<std::string>();
    langData.langName = json["localeName"].get<std::string>();
    langData.fontPath = json["fontPath"].get<std::string>();
    // 翻訳データの読み込み
    for (auto& [key, value] : json["translations"].items()) {
        langData.translations[key] = value.get<std::string>();
    }

    // 安全版でログに出力（未ロード時の参照寿命問題を回避）
    Log(Translation("engine.translations.loaded") + langData.langName, LogSeverity::Info);
    sLanguageDatas[langData.langCode] = std::move(langData);
    return true;
}

const std::string &GetTranslationText(const std::string &lang, const std::string &key) {
    std::string useLang = lang;
    auto langIt = sLanguageDatas.find(lang);
    if (langIt == sLanguageDatas.end()) {
        useLang = "en-US";
        langIt = sLanguageDatas.find(useLang);
        if (langIt == sLanguageDatas.end()) {
            return key;
        }
    }
    const auto &translations = langIt->second.translations;
    auto transIt = translations.find(key);
    if (transIt == translations.end()) {
        return key;
    }
    return transIt->second;
}

const std::string &GetTranslationText(const std::string &key) {
    return GetTranslationText(sLanguage.Get(), key);
}

const std::string &GetCurrentLanguage() {
    return sLanguage.Get();
}

} // namespace KashipanEngine