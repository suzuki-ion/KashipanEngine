#pragma once
#include <string>
#include "EngineSettings.h"
#include "Utilities/FileIO/Json.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

// translations セクションの読込
inline void LoadTranslationsSettings(const Json &rootJson, EngineSettings &settings) {
    Json translationsJson = rootJson.value("translations", Json::object());
    for (auto &[lang, pathJson] : translationsJson.items()) {
        const std::string path = pathJson.get<std::string>();
        settings.translations.languageFilePaths[lang] = path;
        LoadTranslationFile(path);
    }
}

} // namespace KashipanEngine
