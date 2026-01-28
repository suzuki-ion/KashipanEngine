#pragma once
#include <KashipanEngine.h>
#include <array>
#include "BackMonitorRenderer.h"

namespace KashipanEngine {

class Model;
class InputCommand;

class BackMonitorWithMenuScreen : public BackMonitorRenderer {
public:
    BackMonitorWithMenuScreen(ScreenBuffer *target, InputCommand *inputCommand);
    ~BackMonitorWithMenuScreen() override;

    void Initialize() override;
    void Update() override;

private:
    Model *menuTitle_ = nullptr;
    Model *menuStart_ = nullptr;
    Model *menuQuit_ = nullptr;
    Model *menuCredit_ = nullptr;

    InputCommand *inputCommand_ = nullptr;

    int selectedIndex_ = 0;
    int confirmedIndex_ = -1;
    bool isSubmitted_ = false;

    // [Start, Credit, Title, Quit]
    std::array<Model*, 4> models_ = { nullptr, nullptr, nullptr, nullptr };

    std::array<float, 4> zStart_ = { 2.5f, 2.5f, 2.5f, 2.5f };
    std::array<float, 4> zEnd_ = { 2.5f, 2.5f, 2.5f, 2.5f };
    std::array<float, 4> zElapsed_ = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> zDuration_ = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<bool, 4> zAnimating_ = { false, false, false, false };

    // For rotation (sinusoidal) on selected model
    std::array<float, 4> rotOffsetX_ = { 0.0f, 0.0f, 0.0f, 0.0f };
    float rotSineTime_ = 0.0f;
    float rotSineFrequency_ = 1.0f; // cycles per second

    // For confirm X-move animation of non-confirmed models
    std::array<float, 4> xStart_ = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> xEnd_ = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> xElapsed_ = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> xDuration_ = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<bool, 4> xAnimating_ = { false, false, false, false };

    Vector3 confirmStartPos_ = {0.0f, 0.0f, 0.0f};
    Vector3 confirmEndPos_ = {0.0f, 0.0f, 2.0f};
    Vector3 confirmStartRot_ = {0.0f, 0.0f, 0.0f};
    Vector3 confirmEndRot_ = {0.0f, 0.0f, 0.0f};
    float confirmElapsed_ = 0.0f;
    float confirmDuration_ = 0.0f;
    bool isConfirming_ = false;
};

} // namespace KashipanEngine
