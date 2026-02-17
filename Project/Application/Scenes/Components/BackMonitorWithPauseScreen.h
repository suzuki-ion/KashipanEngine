#pragma once
#include <KashipanEngine.h>
#include <vector>
#include "BackMonitorRenderer.h"

namespace KashipanEngine {

class Model;
class InputCommand;

enum class PauseModelIndex : size_t {
    Continue = 0,
    Menu = 1,
    Title = 2,
    Quit = 3
};

class BackMonitorWithPauseScreen : public BackMonitorRenderer {
public:
    BackMonitorWithPauseScreen(ScreenBuffer *target, InputCommand *inputCommand);
    ~BackMonitorWithPauseScreen() override;

    void Initialize() override;
    void Update() override;

    int GetSelectedIndex() const { return selectedIndex_; }
    int GetConfirmedIndex() const { return confirmedIndex_; }
    bool IsSubmitted() const { return isSubmitted_; }
    bool IsConfirming() const { return isConfirming_; }
    bool IsConfirmed() const { return isConfirmed_; }
    bool IsConfirmedTriggered() {
        if (isConfirmedTriggerd_) return false;
        if (isConfirmed_) {
            isConfirmedTriggerd_ = true;
            return true;
        }
        return false;
    }

private:
    Model *pauseLogo_ = nullptr;
    Model *menuContinue_ = nullptr;
    Model *menuMenu_ = nullptr;
    Model *menuTitle_ = nullptr;
    Model *menuQuit_ = nullptr;

    InputCommand *inputCommand_ = nullptr;

    AudioManager::SoundHandle soundHandleSelect_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle soundHandleSubmit_ = AudioManager::kInvalidSoundHandle;

    int selectedIndex_ = 0;
    int confirmedIndex_ = -1;
    bool isSubmitted_ = false;

    int modelCount_ = 0;

    std::vector<Model*> models_;

    std::vector<float> zStart_;
    std::vector<float> zEnd_;
    std::vector<float> zElapsed_;
    std::vector<float> zDuration_;
    std::vector<bool> zAnimating_;

    std::vector<float> rotOffsetX_;
    float rotSineTime_ = 0.0f;
    float rotSineFrequency_ = 1.0f;

    std::vector<float> xStart_;
    std::vector<float> xEnd_;
    std::vector<float> xElapsed_;
    std::vector<float> xDuration_;
    std::vector<bool> xAnimating_;

    std::vector<Vector3> basePositions_;
    std::vector<Vector3> baseRotations_;

    Vector3 confirmStartPos_{ 0.0f, 0.0f, 0.0f };
    Vector3 confirmEndPos_{ 0.0f, 0.0f, 2.0f };
    Vector3 confirmStartRot_{ 0.0f, 0.0f, 0.0f };
    Vector3 confirmEndRot_{ 0.0f, 0.0f, 0.0f };
    float confirmElapsed_ = 0.0f;
    float confirmDuration_ = 0.0f;
    bool isConfirming_ = false;
    bool isConfirmed_ = false;
    bool isConfirmedTriggerd_ = false;
    bool isMenuConfirmSliding_ = false;

    float pauseLogoXStart_ = 0.0f;
    float pauseLogoXEnd_ = 0.0f;
    float pauseLogoXElapsed_ = 0.0f;
    float pauseLogoXDuration_ = 0.0f;
    bool pauseLogoXAnimating_ = false;

    bool isInitialized_ = false;
    bool wasActive_ = false;
};

} // namespace KashipanEngine
