#include "Scenes/TitleScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/TitleScene/StartTextUpdate.h"
#include "Scenes/Components/TitleScene/TitleSceneAnimator.h"

namespace KashipanEngine {

TitleScene::TitleScene()
    : SceneBase("TitleScene") {
}

void TitleScene::Initialize() {
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
        ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.001f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));

        BloomEffect::Params bp{};
        bp.threshold = 0.0f;
        bp.softKnee = 0.0f;
        bp.intensity = 0.0f;
        bp.blurRadius = 0.0f;
        bp.iterations = 4;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));

        screenBuffer3D->AttachToRenderer("ScreenBuffer_TitleScene");
    }

    // 2D Camera (window)
    if (mainWindow && windowCamera) {
        const float w = static_cast<float>(mainWindow->GetClientWidth());
        const float h = static_cast<float>(mainWindow->GetClientHeight());
        windowCamera->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
        windowCamera->SetViewportParams(0.0f, 0.0f, w, h);
    }

    // 2D Camera (screenBuffer2D_)
    if (screenBuffer2D) {
        const float w = static_cast<float>(screenBuffer2D->GetWidth());
        const float h = static_cast<float>(screenBuffer2D->GetHeight());
        camera2D->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
        camera2D->SetViewportParams(0.0f, 0.0f, w, h);
    }

    // 3D Main Camera (screenBuffer3D_)
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

    // Light Camera (shadowMapBuffer_)
    if (shadowMapBuffer && cameraLight) {
        const float w = static_cast<float>(shadowMapBuffer->GetWidth());
        const float h = static_cast<float>(shadowMapBuffer->GetHeight());
        cameraLight->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
        cameraLight->SetViewportParams(0.0f, 0.0f, w, h);
    }

    // Directional Light (screenBuffer3D_)
    directionalLight->SetEnabled(true);
    directionalLight->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    directionalLight->SetDirection(Vector3(1.8f, -2.0f, 1.2f));
    directionalLight->SetIntensity(1.0f);

    //==================================================
    // ゲームオブジェクト
    //==================================================

    // 床用ボックス
    {
        auto obj = std::make_unique<Box>();
        obj->SetUniqueBatchKey();
        obj->SetName("GroundBox");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3(128.0f, 1.0f, 128.0f));
            tr->SetTranslate(Vector3(0.0f, -0.5f, 0.0f));
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    if (sceneDefaultVariables_) {
        auto regFunc = [this](std::unique_ptr<ISceneComponent> comp) {
            return this->AddSceneComponent(std::move(comp));
        };
        AddSceneComponent(std::make_unique<TitleSceneAnimator>(regFunc, GetInputCommand()));
    }

    // タイトルロゴ
    {
        auto modelHandle = ModelManager::GetModelDataFromFileName("title.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetName("TitleLogo");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 28.0f, 10.0f));
            tr->SetRotate(Vector3(0.0f, 0.0f, 0.0f));
            tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }
}

TitleScene::~TitleScene() {
}

void TitleScene::OnUpdate() {
    if (auto *ic = GetInputCommand()) {
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("MenuScene");
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
