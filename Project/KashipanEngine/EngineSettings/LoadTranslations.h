#pragma once
#include <string>
#include "EngineSettings.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

// translations セクションの読込
inline void LoadTranslationsSettings(const JSON &rootJSON, EngineSettings &settings) {
    JSON translationsJSON = rootJSON.value("translations", JSON::object());
    for (auto &[lang, pathJSON] : translationsJSON.items()) {
        const std::string path = pathJSON.get<std::string>();
        settings.translations.languageFilePaths[lang] = path;
        LoadTranslationFile(path);
    }

    LogSeparator();
    Log("Translations", LogSeverity::Info);
    LogSeparator();
    for (const auto &[lang, path] : settings.translations.languageFilePaths) {
        Log("Language: " + lang + ", File Path: " + path, LogSeverity::Info);
    }
}

} // namespace KashipanEngine
