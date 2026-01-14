#pragma once
#include <string>
#include <unordered_map>
#include <Windows.h>
#include "EngineSettings.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {
namespace {
const std::unordered_map<std::string, DWORD> kWindowStyleMap = {
    {"WS_OVERLAPPED", WS_OVERLAPPED},
    {"WS_POPUP", WS_POPUP},
    {"WS_CHILD", WS_CHILD},
    {"WS_MINIMIZE", WS_MINIMIZE},
    {"WS_VISIBLE", WS_VISIBLE},
    {"WS_DISABLED", WS_DISABLED},
    {"WS_CLIPSIBLINGS", WS_CLIPSIBLINGS},
    {"WS_CLIPCHILDREN", WS_CLIPCHILDREN},
    {"WS_MAXIMIZE", WS_MAXIMIZE},
    {"WS_CAPTION", WS_CAPTION},
    {"WS_BORDER", WS_BORDER},
    {"WS_DLGFRAME", WS_DLGFRAME},
    {"WS_VSCROLL", WS_VSCROLL},
    {"WS_HSCROLL", WS_HSCROLL},
    {"WS_SYSMENU", WS_SYSMENU},
    {"WS_THICKFRAME", WS_THICKFRAME},
    {"WS_SIZEBOX", WS_SIZEBOX},
    {"WS_GROUP", WS_GROUP},
    {"WS_TABSTOP", WS_TABSTOP},
    {"WS_MINIMIZEBOX", WS_MINIMIZEBOX},
    {"WS_MAXIMIZEBOX", WS_MAXIMIZEBOX},
    {"WS_TILED", WS_TILED},
    {"WS_ICONIC", WS_ICONIC},
    {"WS_TILEDWINDOW", WS_TILEDWINDOW},
    {"WS_OVERLAPPEDWINDOW", WS_OVERLAPPEDWINDOW},
    {"WS_POPUPWINDOW", WS_POPUPWINDOW},
    {"WS_CHILDWINDOW", WS_CHILDWINDOW},
    // 何も指定しない場合用
    {"WS_NONE", 0}
};
} // namespace KashipanEngine

// window セクションの読込
inline void LoadWindowSettings(const JSON &rootJSON, EngineSettings &settings) {
    JSON windowJSON = rootJSON.value("window", JSON::object());
    settings.window.initialWindowTitle = windowJSON.value("initialWindowTitle", settings.window.initialWindowTitle);
    settings.window.initialWindowWidth = windowJSON.value("initialWindowWidth", settings.window.initialWindowWidth);
    settings.window.initialWindowHeight = windowJSON.value("initialWindowHeight", settings.window.initialWindowHeight);
    settings.window.initialWindowIconPath = windowJSON.value("initialWindowIconPath", settings.window.initialWindowIconPath);
    // ウィンドウスタイルの読み込み
    settings.window.initialWindowStyle = 0;
    // ログ用
    std::string styleNames;
    for (const auto &styleName : windowJSON.value("initialWindowStyle", std::vector<std::string>{})) {
        auto it = kWindowStyleMap.find(styleName);
        if (it != kWindowStyleMap.end()) {
            settings.window.initialWindowStyle |= it->second;
            if (!styleNames.empty()) {
                styleNames += " | ";
            }
            styleNames += styleName;
        }
    }
    if (styleNames.empty()) {
        styleNames = "WS_NONE";
    }

    LogSeparator();
    Log("Window", LogSeverity::Info);
    LogSeparator();
    Log("Initial Window Title: " + settings.window.initialWindowTitle, LogSeverity::Info);
    Log("Initial Window Width: " + std::to_string(settings.window.initialWindowWidth), LogSeverity::Info);
    Log("Initial Window Height: " + std::to_string(settings.window.initialWindowHeight), LogSeverity::Info);
    Log("Initial Window Style: " + styleNames, LogSeverity::Info);
    Log("Initial Window Icon Path: " + settings.window.initialWindowIconPath, LogSeverity::Info);
}

} // namespace KashipanEngine
