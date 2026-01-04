#pragma once
#include "Core/GameEngine.h"

#include "Core/Window.h"
#include "Core/WindowsAPI.h"
#include "Core/WindowsAPI/WindowEvents/DefaultEvents.h"
#include "EngineSettings.h"

#include "Scenes/TestScene.h"

#include <memory>

namespace KashipanEngine {

inline void AppInitialize(const GameEngine::Context &context) {
    auto monitorInfoOpt = WindowsAPI::QueryMonitorInfo();
    const RECT area = monitorInfoOpt ? monitorInfoOpt->WorkArea() : RECT{ 0, 0, 1280, 720 };

    Window::CreateNormal(
        "Main Window",
        1920,
        1080);
    auto *overlay = Window::CreateOverlay(
        "Overlay Window",
        area.right,
        area.bottom,
        true);

    if (overlay) {
        overlay->RegisterWindowEvent(std::make_unique<WindowDefaultEvent::SysCommandCloseEventSimple>());
    }

    if (context.sceneManager) {
        context.sceneManager->RegisterScene<TestScene>("TestScene");
        context.sceneManager->ChangeScene("TestScene");
    }
}

} // namespace KashipanEngine
