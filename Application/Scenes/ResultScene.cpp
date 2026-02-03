#include "Scenes/ResultScene.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/SceneFade.h"
#include "Scenes/Components/TitleScene/CameraStartMovement.h"

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
    auto *directionalLight = sceneDefaultVariables_ ? sceneDefaultVariables_->GetDirectionalLight() : nullptr;

    if (screenBuffer3D) {
        /*ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.001f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));*/

        BloomEffect::Params bp{};
        bp.threshold = 1.0f;
        bp.softKnee = 0.25f;
        bp.intensity = 1.0f;
        bp.blurRadius = 1.0f;
        bp.iterations = 4;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));

        screenBuffer3D->AttachToRenderer("ScreenBuffer_ResultScene");
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
            tr->SetTranslate(Vector3(0.0f, 24.0f, -21.0f));
            tr->SetRotate(Vector3(0.6f, 0.0f, 0.0f));
        }
        const float w = static_cast<float>(screenBuffer3D->GetWidth());
        const float h = static_cast<float>(screenBuffer3D->GetHeight());
        camera3D->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
        camera3D->SetViewportParams(0.0f, 0.0f, w, h);
        camera3D->SetFovY(0.7f);
    }

    if (shadowMapBuffer && cameraLight) {
        const float w = static_cast<float>(shadowMapBuffer->GetWidth());
        const float h = static_cast<float>(shadowMapBuffer->GetHeight());
        cameraLight->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
        cameraLight->SetViewportParams(0.0f, 0.0f, w, h);
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
            tr->SetTranslate(Vector3(0.0f, 0.0f, 4.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<Letterbox>());
    AddSceneComponent(std::make_unique<CameraStartMovement>());

    if (sceneDefaultVariables_) {
        auto regFunc = [this](std::unique_ptr<ISceneComponent> comp) {
            return this->AddSceneComponent(std::move(comp));
        };
        auto comp = std::make_unique<ResultSceneAnimator>(regFunc, GetInputCommand());
        resultSceneAnimator_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    AddSceneComponent(std::make_unique<SceneFade>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *fade = GetSceneComponent<SceneFade>()) {
        fade->SetColor(Vector4(0.0f, 0.0f, 0.0f, 1.0f));
        fade->SetDuration(1.0f);
        fade->SetDelayBefore(1.0f);
        fade->PlayIn();
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
