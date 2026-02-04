#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class BackMonitor;
class BackMonitorWithGameScreen;
class BackMonitorWithMenuScreen;
class CameraController;
class StageLighting;

class TutorialBase : public ISceneComponent {
public:
    TutorialBase(InputCommand *inputCommand,
        const Vector3 &gameTargetPos,
        const Vector3 &gameTargetRot,
        const Vector3 &menuTargetPos,
        const Vector3 &menuTargetRot)
        : ISceneComponent("TutorialBase", 1),
          inputCommand_(inputCommand),
          gameTargetPos_(gameTargetPos),
          gameTargetRot_(gameTargetRot),
          menuTargetPos_(menuTargetPos),
          menuTargetRot_(menuTargetRot) {}

    ~TutorialBase() override = default;

	// カメラをモニターからステージに向ける
    void StartTutorial();

	// カメラをステージからモニターに向ける
    void StartMonitorText();

	// チュートリアル終了
    void QuitTutorial();

    bool IsActive() const { return isActive_; }
    bool IsLookAtMonitor() const { return isLookAtMonitor_; }
    bool IsLookAtStage() const { return isLookAtStage_; }

protected:
    void InitializeInternal();

    // ABottom判定
    bool IsSubmit() const;

    ScreenBuffer *GetMonitorScreenBuffer() const { return screenBuffer_; }
    BackMonitorWithGameScreen *GetBackMonitorGame() const { return backMonitorGame_; }
    BackMonitorWithMenuScreen *GetBackMonitorMenu() const { return backMonitorMenu_; }
    CameraController *GetCameraController() const { return cameraController_; }

private:
    ScreenBuffer *screenBuffer_ = nullptr;
    BackMonitorWithGameScreen *backMonitorGame_ = nullptr;
    BackMonitorWithMenuScreen *backMonitorMenu_ = nullptr;
    CameraController *cameraController_ = nullptr;
    StageLighting *stageLighting_ = nullptr;

    InputCommand *inputCommand_ = nullptr;
    Vector3 gameTargetPos_{};
    Vector3 gameTargetRot_{};
    Vector3 menuTargetPos_{};
    Vector3 menuTargetRot_{};

    bool isActive_ = false;

    bool isLookAtMonitor_ = false;
    bool isLookAtStage_ = false;
};

} // namespace KashipanEngine
