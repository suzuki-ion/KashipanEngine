#include "Scenes/ResultScene.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/SceneFade.h"
#include "Scenes/Components/ResultScene/ScoreSaveAndLoad.h"
#include "Scenes/Components/ResultScene/ShowScoreNumModels.h"
#include "Objects/Components/ParticleMovement.h"

namespace KashipanEngine {

ResultScene::ResultScene()
    : SceneBase("ResultScene") {
}

void ResultScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainWindow = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainWindow() : nullptr;
    auto *screenBuffer2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer2D() : nullptr;
    auto *screenBuffer3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer3D() : nullptr;
    auto *windowCamera = sceneDefaultVariables_ ? sceneDefaultVariables_->GetWindowCamera2D() : nullptr;
    auto *camera2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera2D() : nullptr;
    auto *camera3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera3D() : nullptr;
    auto *cameraLight = sceneDefaultVariables_ ? sceneDefaultVariables_->GetLightCamera3D() : nullptr;
    auto *shadowMapBuffer = sceneDefaultVariables_ ? sceneDefaultVariables_->GetShadowMapBuffer() : nullptr;
    auto *shadowMapCameraSync = sceneDefaultVariables_ ? sceneDefaultVariables_->GetShadowMapCameraSync() : nullptr;
    auto *directionalLight = sceneDefaultVariables_ ? sceneDefaultVariables_->GetDirectionalLight() : nullptr;

    auto whiteTex = TextureManager::GetTextureFromFileName("white1x1.png");
    
    if (screenBuffer3D) {
        /*ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.001f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));*/

        /*BloomEffect::Params bp{};
        bp.threshold = 1.0f;
        bp.softKnee = 0.25f;
        bp.intensity = 1.0f;
        bp.blurRadius = 1.0f;
        bp.iterations = 4;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));

        screenBuffer3D->AttachToRenderer("ScreenBuffer_ResultScene");*/
    }

    if (mainWindow && windowCamera) {
        const float w = static_cast<float>(mainWindow->GetClientWidth());
        const float h = static_cast<float>(mainWindow->GetClientHeight());
        windowCamera->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
        windowCamera->SetViewportParams(0.0f, 0.0f, w, h);
    }

    if (screenBuffer2D) {
        const float w = static_cast<float>(screenBuffer2D->GetWidth());
        const float h = static_cast<float>(screenBuffer2D->GetHeight());
        camera2D->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
        camera2D->SetViewportParams(0.0f, 0.0f, w, h);
    }

    if (screenBuffer3D && camera3D) {
        if (auto *tr = camera3D->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 1.0f, -8.0f));
            tr->SetRotate(Vector3(0.0f, 0.0f, 0.0f));
        }
        const float w = static_cast<float>(screenBuffer3D->GetWidth());
        const float h = static_cast<float>(screenBuffer3D->GetHeight());
        camera3D->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
        camera3D->SetViewportParams(0.0f, 0.0f, w, h);
        camera3D->SetFovY(1.2f);
    }

    if (shadowMapBuffer && cameraLight) {
        const float w = static_cast<float>(shadowMapBuffer->GetWidth());
        const float h = static_cast<float>(shadowMapBuffer->GetHeight());
        cameraLight->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
        cameraLight->SetViewportParams(0.0f, 0.0f, w, h);
    }

    if (shadowMapCameraSync) {
        shadowMapCameraSync->SetShadowFar(24.0f);
    }

    directionalLight->SetEnabled(true);
    directionalLight->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    directionalLight->SetDirection(Vector3(1.8f, -2.0f, 1.2f));
    directionalLight->SetIntensity(1.5f);

    {
        auto modelHandle = ModelManager::GetModelDataFromFileName("stageTitle.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("StageTitle");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3(2.0f, 2.0f, 2.0f));
            tr->SetTranslate(Vector3(0.5f, -1.0f, 19.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<Letterbox>());

    auto scoreSaveAndLoad = std::make_unique<ScoreSaveAndLoad>();
    auto showScoreNumModels = std::make_unique<ShowScoreNumModels>();
    auto *scoreSaveAndLoadPtr = scoreSaveAndLoad.get();
    auto *showScoreNumModelsPtr = showScoreNumModels.get();
    AddSceneComponent(std::move(scoreSaveAndLoad));
    AddSceneComponent(std::move(showScoreNumModels));
    if (scoreSaveAndLoadPtr && showScoreNumModelsPtr) {
        scoreSaveAndLoadPtr->Load();
        showScoreNumModelsPtr->SetScores(scoreSaveAndLoadPtr->GetScores());
        showScoreNumModelsPtr->SetVisible(false);
    }

    AddSceneComponent(std::make_unique<SceneFade>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *fade = GetSceneComponent<SceneFade>()) {
        fade->SetColor(Vector4(0.0f, 0.0f, 0.0f, 1.0f));
        fade->SetDuration(1.0f);
        fade->SetDelayBefore(1.0f);
        fade->PlayIn();
    }

    if (sceneDefaultVariables_) {
        auto regFunc = [this](std::unique_ptr<ISceneComponent> comp) {
            return this->AddSceneComponent(std::move(comp));
            };
        auto comp = std::make_unique<ResultSceneAnimator>(regFunc, GetInputCommand());
        resultSceneAnimator_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    // パーティクル
    {
        constexpr std::uint32_t kParticleCount = 64;
        for (std::uint32_t i = 0; i < kParticleCount; ++i) {
            auto obj = std::make_unique<Billboard>();
            obj->SetName(std::string("ParticleBillboard_") + std::to_string(i));
            obj->SetCamera(camera3D);
            obj->SetFacingMode(Billboard::FacingMode::LookAtCamera);
            obj->RegisterComponent<ParticleMovement>(
                ParticleMovement::SpawnBox{
                    Vector3{-32.0f, 0.0f, 16.0f},
                    Vector3{32.0f, 64.0f, 64.0f} },
                    0.5f,
                    5.0f,
                    Vector3{ 0.2f, 0.2f, 0.2f });

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(Vector3(0.0f, -99999.0f, 0.0f));
                tr->SetScale(Vector3(0.0f, 0.0f, 0.0f));
            }
            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetTexture(whiteTex);
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
                mat->SetEnableLighting(false);
                mat->SetEnableShadowMapProjection(false);
            }
            if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            AddObject3D(std::move(obj));
        }
    }
}

ResultScene::~ResultScene() {
}

void ResultScene::OnUpdate() {
    if (auto *ic = GetInputCommand()) {
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("TitleScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
    }

    if (resultSceneAnimator_) {
        if (resultSceneAnimator_->IsAnimating() && GetInputCommand()->Evaluate("Submit").Triggered()) {
            resultSceneAnimator_->EndAnimation();
        } else if (resultSceneAnimator_->IsAnimationFinished() && GetInputCommand()->Evaluate("Submit").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("TitleScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
    }

    if (!GetNextSceneName().empty()) {
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }
}

} // namespace KashipanEngine
