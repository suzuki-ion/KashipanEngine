#include "Scenes/TitleScene.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/SceneFade.h"
#include "Scenes/Components/TitleScene/StartTextUpdate.h"
#include "Scenes/Components/TitleScene/TitleSceneAnimator.h"
#include "Objects/Components/ParticleMovement.h"

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

    auto whiteTex = TextureManager::GetTextureFromFileName("white1x1.png");
    
    if (screenBuffer3D) {
        BloomEffect::Params bp{};
        bp.threshold = 1.0f;
        bp.softKnee = 0.25f;
        bp.intensity = 1.0f;
        bp.blurRadius = 1.0f;
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
    directionalLight->SetIntensity(1.5f);

    //==================================================
    // ゲームオブジェクト
    //==================================================

    // タイトル用ステージ
    {
        auto modelHandle = ModelManager::GetModelDataFromFileName("stageTitle.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("StageTitle");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3(2.0f, 2.0f, 2.0f));
            tr->SetTranslate(Vector3(0.0f, -1.0f, 19.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<Letterbox>());

    if (sceneDefaultVariables_) {
        auto regFunc = [this](std::unique_ptr<ISceneComponent> comp) {
            return this->AddSceneComponent(std::move(comp));
        };
        auto comp = std::make_unique<TitleSceneAnimator>(regFunc, GetInputCommand());
        titleSceneAnimator_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    AddSceneComponent(std::make_unique<SceneFade>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *fade = GetSceneComponent<SceneFade>()) {
        // フェードイン（色->透明）を実行
        fade->SetColor(Vector4(0.0f, 0.0f, 0.0f, 1.0f));
        fade->SetDuration(1.0f);
        fade->SetDelayBefore(1.0f); // 1秒待ってからフェードを開始
        fade->PlayIn();
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
                    Vector3{-64.0f, 24.0f, 32.0f},
                    Vector3{64.0f, 80.0f, 64.0f} },
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

TitleScene::~TitleScene() {
}

void TitleScene::OnUpdate() {
    if (auto *ic = GetInputCommand()) {
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("GameScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
    }

    if (titleSceneAnimator_) {
        if (titleSceneAnimator_->IsAnimationFinishedTriggered() ||
            titleSceneAnimator_->IsAnimating() && GetInputCommand()->Evaluate("Submit").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("GameScene");
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
