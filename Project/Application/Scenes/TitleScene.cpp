#include "Scenes/TitleScene.h"
#include "Scenes/TitleScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/StageGroundGenerator.h"
#include "Scenes/Components/StageDecoPlaneGenerator.h"
#include "Scenes/Components/StageDecoBoxGenerator.h"
#include "Scenes/Components/StageObjectController.h"
#include "Scenes/Components/StageObjectRandomGenerator.h"
#include "Scenes/Components/TitleSceneAudioPlayer.h"
#include "Scenes/Components/TitleSceneUIController.h"
#include "Scenes/Components/StageSelectUIController.h"
#include "Scenes/Components/StageSelectRankingUIController.h"

#include "Objects/GameObjects/3D/Box.h"

#include <algorithm>

namespace KashipanEngine {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

TitleScene::TitleScene()
    : SceneBase("TitleScene") {
}

void TitleScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    auto *screenBuffer3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer3D() : nullptr;
    mainCamera_ = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera3D() : nullptr;

    if (screenBuffer3D) {
        BloomEffect::Params bp;
        bp.intensity = 1.0f;
        bp.blurRadius = 2.0f;
        bp.iterations = 4;
        bp.softKnee = 0.2f;
        bp.threshold = 0.5f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));
        screenBuffer3D->AttachToRenderer("ScreenBuffer3D.Default");
    }

    AddSceneComponent(std::make_unique<StageGroundGenerator>());
    AddSceneComponent(std::make_unique<StageDecoPlaneGenerator>());
    AddSceneComponent(std::make_unique<StageDecoBoxGenerator>());
    AddSceneComponent(std::make_unique<StageObjectController>());
    AddSceneComponent(std::make_unique<StageObjectRandomGenerator>());

    if (sceneDefaultVariables_ && sceneDefaultVariables_->GetColliderComp()) {
        auto player = std::make_unique<Box>();
        player->SetName("PlayerRoot");
        player->SetUniqueBatchKey();

        if (screenBuffer3D) {
            player->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        }

        if (auto *tr = player->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{0.0f, 0.0f, -2.0f});
        }
        if (auto *mat = player->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 0.0f});
        }

        dummyPlayer_ = player.get();
        AddObject3D(std::move(player));
    }

    if (mainCamera_) {
        if (auto *tr = mainCamera_->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{0.0f, 0.0f, 6.0f});
            tr->SetRotate(Vector3{0.0f, 3.14159265358979323846f, 0.0f});
        }
        mainCamera_->SetFovY(1.0f);
    }

    AddSceneComponent(std::make_unique<TitleSceneUIController>());
    titleSceneUIController_ = GetSceneComponent<TitleSceneUIController>();
    AddSceneComponent(std::make_unique<StageSelectUIController>());
    stageSelectUIController_ = GetSceneComponent<StageSelectUIController>();
    AddSceneComponent(std::make_unique<StageSelectRankingUIController>());
    stageSelectRankingUIController_ = GetSceneComponent<StageSelectRankingUIController>();
    if (stageSelectRankingUIController_) {
        stageSelectRankingUIController_->SetStageSelectUIController(stageSelectUIController_);
    }

    titleSceneUIController_->SetEnableUpdating(true);
    stageSelectUIController_->SetEnableUpdating(false);
    if (stageSelectRankingUIController_) {
        stageSelectRankingUIController_->SetEnableUpdating(false);
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());
    AddSceneComponent(std::make_unique<ParticleManager>());
    AddSceneComponent(std::make_unique<TitleSceneAudioPlayer>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }
}

TitleScene::~TitleScene() {
}

void TitleScene::OnUpdate() {
    const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());

    if (dummyPlayer_) {
        if (auto *tr = dummyPlayer_->GetComponent3D<Transform3D>()) {
            auto pos = tr->GetTranslate();
            pos.x = 0.0f;
            pos.y = 0.0f;
            pos.z -= moveSpeedZ_ * dt;
            tr->SetTranslate(pos);

            if (mainCamera_) {
                if (auto *camTr = mainCamera_->GetComponent3D<Transform3D>()) {
                    camTr->SetTranslate(Vector3{0.0f, 0.0f, pos.z + cameraPlayerOffsetZ_});
                    camTr->SetRotate(Vector3{0.0f, kPi, 0.0f});
                }
                mainCamera_->SetFovY(1.0f);
            }
        }
    }

    if (!GetNextSceneName().empty()) {
        if (auto* out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

    if (titleSceneUIController_) {
        const auto action = titleSceneUIController_->ConsumeRequestedAction();
        if (action == TitleSceneUIController::RequestAction::StartGame) {
            titleSceneUIController_->SetEnableUpdating(false);
            if (stageSelectUIController_) {
                stageSelectUIController_->SetEnableUpdating(true);
                if (stageSelectRankingUIController_) {
                    stageSelectRankingUIController_->SetEnableUpdating(true);
                }
            } else {
                if (GetNextSceneName().empty()) {
                    SetNextSceneName("GameScene");
                }
                if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                    out->Play();
                }
            }
        } else if (action == TitleSceneUIController::RequestAction::Quit) {
            if (sceneDefaultVariables_ && sceneDefaultVariables_->GetMainWindow()) {
                sceneDefaultVariables_->GetMainWindow()->DestroyNotify();
            }
        }
    }

    if (stageSelectUIController_ && stageSelectUIController_->IsUpdating()) {
        const auto action = stageSelectUIController_->ConsumeRequestedAction();
        if (action == StageSelectUIController::RequestAction::StageSelected) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("GameScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        } else if (action == StageSelectUIController::RequestAction::Canceled) {
            stageSelectUIController_->SetEnableUpdating(false);
            if (stageSelectRankingUIController_) {
                stageSelectRankingUIController_->SetEnableUpdating(false);
            }
            if (titleSceneUIController_) {
                titleSceneUIController_->SetEnableUpdating(true);
            }
        }
    }

    if (auto *ic = GetInputCommand()) {

#if defined(DEBUG_BUILD) or defined(DEVELOPMENT_BUILD)
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("GameScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
#endif
    }
}

} // namespace KashipanEngine
