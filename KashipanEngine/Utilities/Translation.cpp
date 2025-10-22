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
        return false;
    }
    // 必要な情報が存在するか確認
    if (!json.contains("localeCode") ||
        !json.contains("localeName") ||
        !json.contains("fontPath") ||
        !json.contains("translations")) {
        return false;
    }
    return true;
}

const std::string &GetTranslationText(const std::string &lang, const std::string &key) {
    static const std::string emptyString = "";
    auto langIt = sLanguageDatas.find(lang);
    if (langIt == sLanguageDatas.end()) {
        return emptyString;
    }
    const auto &translations = langIt->second.translations;
    auto transIt = translations.find(key);
    if (transIt == translations.end()) {
        return emptyString;
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