#include "Scenes/Components/Tutorial/TutorialBase.h"
#include "Scenes/Components/BackMonitor.h"
#include "Scenes/Components/BackMonitorWithGameScreen.h"
#include "Scenes/Components/BackMonitorWithMenuScreen.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/StageLighting.h"

namespace KashipanEngine {

void TutorialBase::StartTutorial() {
    if (cameraController_) {
        cameraController_->SetTargetTranslate(gameTargetPos_);
        cameraController_->SetTargetRotate(gameTargetRot_);
    }
    if (stageLighting_) {
        stageLighting_->EnableLighting();
    }
    isActive_ = true;
    isLookAtMonitor_ = false;
    isLookAtStage_ = true;
}

void TutorialBase::StartMonitorText() {
    if (cameraController_) {
        cameraController_->SetTargetTranslate(menuTargetPos_);
        cameraController_->SetTargetRotate(menuTargetRot_);
    }
    if (stageLighting_) {
        stageLighting_->DisableLighting();
    }
    isActive_ = true;
    isLookAtMonitor_ = true;
    isLookAtStage_ = false;
}

void TutorialBase::QuitTutorial() {
    if (backMonitorMenu_) {
        backMonitorMenu_->SetActive(true);
    }
    if (cameraController_) {
        cameraController_->SetTargetTranslate(menuTargetPos_);
        cameraController_->SetTargetRotate(menuTargetRot_);
    }
    if (stageLighting_) {
        stageLighting_->DisableLighting();
    }
    isActive_ = false;
    isLookAtMonitor_ = false;
    isLookAtStage_ = false;
}

void TutorialBase::InitializeInternal() {
    auto *context = GetOwnerContext();
    if (!context) return;

    auto *backMonitor = context->GetComponent<BackMonitor>();
    screenBuffer_ = backMonitor ? backMonitor->GetScreenBuffer() : nullptr;

    backMonitorGame_ = context->GetComponent<BackMonitorWithGameScreen>();
    backMonitorMenu_ = context->GetComponent<BackMonitorWithMenuScreen>();
    cameraController_ = context->GetComponent<CameraController>();
    stageLighting_ = context->GetComponent<StageLighting>();
}

bool TutorialBase::IsSubmit() const {
    return inputCommand_ && inputCommand_->Evaluate("Submit").Triggered();
}

} // namespace KashipanEngine
