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
    void MenuCreditUpdate();

    Model *menuTitle_ = nullptr;
    Model *menuStart_ = nullptr;
    Model *menuQuit_ = nullptr;
    Model *menuCredit_ = nullptr;

    InputCommand *inputCommand_ = nullptr;

    AudioManager::SoundHandle soundHandleSelect_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle soundHandleSubmit_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle soundHandleCancel_ = AudioManager::kInvalidSoundHandle;

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

    // 回転用オフセット（現在未使用）
    std::vector<float> rotOffsetX_;
    float rotSineTime_ = 0.0f;
    float rotSineFrequency_ = 1.0f; // 1秒に1回

    // 移動用オフセット
    std::vector<float> xStart_;
    std::vector<float> xEnd_;
    std::vector<float> xElapsed_;
    std::vector<float> xDuration_;
    std::vector<bool> xAnimating_;

    std::vector<Vector3> basePositions_;
    std::vector<Vector3> baseRotations_;
    std::vector<Vector3> returnStartPos_;
    std::vector<Vector3> returnEndPos_;
    std::vector<Vector3> returnStartRot_;
    std::vector<Vector3> returnEndRot_;
    std::vector<float> returnElapsed_;
    std::vector<float> returnDuration_;
    std::vector<bool> returnAnimating_;

    Vector3 creditMoveStartPos_ = {0.0f, 0.0f, 0.0f};
    Vector3 creditMoveEndPos_ = {0.0f, 0.0f, 0.0f};
    float creditMoveElapsed_ = 0.0f;
    float creditMoveDuration_ = 0.0f;
    bool isCreditMoving_ = false;
    bool isCreditMoved_ = false;
    bool isReturning_ = false;

    Vector3 confirmStartPos_ = {0.0f, 0.0f, 0.0f};
    Vector3 confirmEndPos_ = {0.0f, 0.0f, 2.0f};
    Vector3 confirmStartRot_ = {0.0f, 0.0f, 0.0f};
    Vector3 confirmEndRot_ = {0.0f, 0.0f, 0.0f};
    float confirmElapsed_ = 0.0f;
    float confirmDuration_ = 0.0f;
    bool isConfirming_ = false;
    bool isConfirmed_ = false;
    bool isConfirmedTriggerd_ = false;

    bool isInitialized_ = false;

};

} // namespace KashipanEngine
