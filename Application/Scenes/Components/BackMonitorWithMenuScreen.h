#pragma once
#include <KashipanEngine.h>
#include <vector>
#include "BackMonitorRenderer.h"

namespace KashipanEngine {

class Model;
class InputCommand;

enum class MenuModelIndex : size_t {
    Start = 0,
    Credit = 1,
    Title = 2,
    Quit = 3
};

class BackMonitorWithMenuScreen : public BackMonitorRenderer {
public:
    BackMonitorWithMenuScreen(ScreenBuffer *target, InputCommand *inputCommand);
    ~BackMonitorWithMenuScreen() override;

    void Initialize() override;
    void Update() override;

    int GetSelectedIndex() const { return selectedIndex_; }
    int GetConfirmedIndex() const { return confirmedIndex_; }
    bool IsSubmitted() const { return isSubmitted_; }
    bool IsConfirming() const { return isConfirming_; }
    bool IsConfirmed() const { return isSubmitted_ && !isConfirming_; }

private:
    Model *menuTitle_ = nullptr;
    Model *menuStart_ = nullptr;
    Model *menuQuit_ = nullptr;
    Model *menuCredit_ = nullptr;

    InputCommand *inputCommand_ = nullptr;

    int selectedIndex_ = 0;
    int confirmedIndex_ = -1;
    bool isSubmitted_ = false;

    int modelCount_ = 0;

    // [Start, Credit, Title, Quit]
    std::vector<Model*> models_;

    std::vector<float> zStart_;
    std::vector<float> zEnd_;
    std::vector<float> zElapsed_;
    std::vector<float> zDuration_;
    std::vector<bool> zAnimating_;

    // For rotation (sinusoidal) on selected model
    std::vector<float> rotOffsetX_;
    float rotSineTime_ = 0.0f;
    float rotSineFrequency_ = 1.0f; // cycles per second

    // For confirm X-move animation of non-confirmed models
    std::vector<float> xStart_;
    std::vector<float> xEnd_;
    std::vector<float> xElapsed_;
    std::vector<float> xDuration_;
    std::vector<bool> xAnimating_;

    Vector3 confirmStartPos_ = {0.0f, 0.0f, 0.0f};
    Vector3 confirmEndPos_ = {0.0f, 0.0f, 2.0f};
    Vector3 confirmStartRot_ = {0.0f, 0.0f, 0.0f};
    Vector3 confirmEndRot_ = {0.0f, 0.0f, 0.0f};
    float confirmElapsed_ = 0.0f;
    float confirmDuration_ = 0.0f;
    bool isConfirming_ = false;
};

} // namespace KashipanEngine
