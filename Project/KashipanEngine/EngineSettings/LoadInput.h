#pragma once
#include <algorithm>
#include <string>
#include "EngineSettings.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

// input セクションの読込
inline void LoadInputSettings(const JSON &rootJSON, EngineSettings &settings) {
    JSON inputJSON = rootJSON.value("input", JSON::object());
    settings.input.mouseMoveThreshold = (std::max)(inputJSON.value("mouseMoveThreshold", settings.input.mouseMoveThreshold), 0.0f);
    settings.input.controllerStickThreshold = std::clamp(inputJSON.value("controllerStickThreshold", settings.input.controllerStickThreshold), 0.0f, 1.0f);
    settings.input.controllerTriggerThreshold = std::clamp(inputJSON.value("controllerTriggerThreshold", settings.input.controllerTriggerThreshold), 0.0f, 1.0f);

    LogSeparator();
    Log(Translation("engine.settings.input.section"), LogSeverity::Info);
    LogSeparator();
    Log(Translation("engine.settings.input.mousemovethreshold") + std::to_string(settings.input.mouseMoveThreshold), LogSeverity::Info);
    Log(Translation("engine.settings.input.controllerstickthreshold") + std::to_string(settings.input.controllerStickThreshold), LogSeverity::Info);
    Log(Translation("engine.settings.input.controllertriggerthreshold") + std::to_string(settings.input.controllerTriggerThreshold), LogSeverity::Info);
}

} // namespace KashipanEngine
