#pragma once
#include <string>
#include "EngineSettings.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

// translations セクションの読込
// ここで読むのはプロジェクト固有（アプリケーション側）の翻訳ファイルのみ。
// エンジン共通の翻訳はエンジンルート直下の Locales/ から、この設定より前に
// LoadGlobalTranslationFiles() が読み込んでいる。
// 後から読むためエンジン共通の翻訳へ追加され、同名キーはプロジェクト側が優先される。
inline void LoadTranslationsSettings(const JSON &rootJSON, EngineSettings &settings) {
    JSON translationsJSON = rootJSON.value("translations", JSON::object());
    for (auto &[lang, pathJSON] : translationsJSON.items()) {
        const std::string path = pathJSON.get<std::string>();
        settings.translations.languageFilePaths[lang] = path;
        LoadTranslationFile(path);
    }

    LogSeparator();
    Log(Translation("engine.settings.translations.section"), LogSeverity::Info);
    LogSeparator();
    if (settings.translations.languageFilePaths.empty()) {
        Log(Translation("engine.settings.translations.project.none"), LogSeverity::Info);
    }
    for (const auto &[lang, path] : settings.translations.languageFilePaths) {
        Log(Translation("engine.settings.translations.language") + lang + Translation("engine.settings.translations.filepath") + path, LogSeverity::Info);
    }
}

} // namespace KashipanEngine
