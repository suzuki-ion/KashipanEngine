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

#include "Objects/GameObjects/3D/Box.h"

#include <algorithm>
#include <cmath>

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
    auto *screenBuffer2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer2D() : nullptr;
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
            tr->SetRotate(Vector3{0.0f, kPi, 0.0f});
        }
        mainCamera_->SetFovY(1.0f);
    }

    if (screenBuffer2D) {
        auto title = std::make_unique<Text>(64);
        title->SetName("TitleText");
        title->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        title->SetText("グランナー");
        title->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = title->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{560.0f, 600.0f, 0.0f});
        }
        titleText_ = title.get();
        AddObject2D(std::move(title));

        auto start = std::make_unique<Text>(64);
        start->SetName("StartText");
        start->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        start->SetText("＞ スタート");
        start->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = start->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{560.0f, 500.0f, 0.0f});
        }
        startText_ = start.get();
        AddObject2D(std::move(start));

        auto quit = std::make_unique<Text>(64);
        quit->SetName("QuitText");
        quit->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        quit->SetText("  おわる");
        quit->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = quit->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{560.0f, 430.0f, 0.0f});
        }
        quitText_ = quit.get();
        AddObject2D(std::move(quit));
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

    if (auto *ic = GetInputCommand()) {
        if (ic->Evaluate("SelectUp").Triggered()) {
            selectionIndex_ = 0;
        }
        if (ic->Evaluate("SelectDown").Triggered()) {
            selectionIndex_ = 1;
        }

        if (startText_) {
            startText_->SetText(selectionIndex_ == 0 ? "＞ スタート" : "  スタート");
        }
        if (quitText_) {
            quitText_->SetText(selectionIndex_ == 1 ? "＞ おわる" : "  おわる");
        }

        if (ic->Evaluate("Submit").Triggered()) {
            if (selectionIndex_ == 0) {
                if (GetNextSceneName().empty()) {
                    SetNextSceneName("GameScene");
                }
                if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                    out->Play();
                }
            } else {
                if (sceneDefaultVariables_ && sceneDefaultVariables_->GetMainWindow()) {
                    sceneDefaultVariables_->GetMainWindow()->DestroyNotify();
                }
            }
        }

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
