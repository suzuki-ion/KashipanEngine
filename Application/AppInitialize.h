#pragma once
#include <KashipanEngine.h>

#include "Scenes/TitleScene.h"
#include "Scenes/MenuScene.h"
#include "Scenes/TestScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/ResultScene.h"
#include "Scenes/GameOverScene.h"

namespace KashipanEngine {

namespace {
SamplerManager::SamplerHandle CreateShadowSampler() {
}
} // namespace

inline void AppInitialize(const GameEngine::Context &context) {
    auto monitorInfoOpt = WindowsAPI::QueryMonitorInfo();
    const RECT area = monitorInfoOpt ? monitorInfoOpt->WorkArea() : RECT{ 0, 0, 1280, 720 };

    D3D12_SAMPLER_DESC desc{};
    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
    desc.MaxAnisotropy = 1;
    auto samplerHandle = SamplerManager::CreateSampler(desc);

    Window::CreateNormal("Main Window", 1920, 1080);

    if (context.sceneManager) {
        auto *sm = context.sceneManager;
        sm->AddSceneVariable("ShadowSampler", samplerHandle);

        sm->RegisterScene<TitleScene>("TitleScene");
        sm->RegisterScene<MenuScene>("MenuScene");
        sm->RegisterScene<GameScene>("GameScene");
        sm->RegisterScene<ResultScene>("ResultScene");
        sm->RegisterScene<GameOverScene>("GameOverScene");
        sm->RegisterScene<TestScene>("TestScene");

        context.sceneManager->ChangeScene("TitleScene");
    }

    if (context.inputCommand) {
        auto *ic = context.inputCommand;
        ic->Clear();

        // 移動
        ic->RegisterCommand("MoveX", InputCommand::KeyboardKey{ Key::A }, InputCommand::InputState::Down, true);
        ic->RegisterCommand("MoveX", InputCommand::KeyboardKey{ Key::D }, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveX", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down);

        ic->RegisterCommand("MoveZ", InputCommand::KeyboardKey{ Key::W }, InputCommand::InputState::Down);
        ic->RegisterCommand("MoveZ", InputCommand::KeyboardKey{ Key::S }, InputCommand::InputState::Down, true);
        ic->RegisterCommand("MoveZ", InputCommand::ControllerAnalog::LeftStickY, InputCommand::InputState::Down);

        // 攻撃
        ic->RegisterCommand("AttackCharge", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Down);
        ic->RegisterCommand("AttackCharge", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Down);

        ic->RegisterCommand("Attack", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Release);
        ic->RegisterCommand("Attack", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Release);

        // ダッシュ
        ic->RegisterCommand("Dash", InputCommand::KeyboardKey{ Key::Shift }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Dash", ControllerButton::X, InputCommand::InputState::Trigger);

        // 決定
        ic->RegisterCommand("Submit", InputCommand::KeyboardKey{ Key::Enter }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Trigger);
        ic->RegisterCommand("Submit", ControllerButton::A, InputCommand::InputState::Trigger);

        // デバッグ用シーン遷移
        ic->RegisterCommand("DebugSceneChange", InputCommand::KeyboardKey{ Key::F1 }, InputCommand::InputState::Trigger);
    }
}

} // namespace KashipanEngine
