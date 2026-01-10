#include "Scenes/TitleScene.h"

#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/ScreenBufferKeepRatio.h"

#include "Objects/Components/ParticleMovement.h"
#include "Objects/Components/PlayerMovement.h"

namespace KashipanEngine {

TitleScene::TitleScene()
    : SceneBase("TitleScene") {
    // シーン遷移コンポーネントの追加
    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    // オフスクリーン描画用バッファ作成
    screenBuffer_ = ScreenBuffer::Create(1920, 1080);

    // コライダーコンポーネント追加
    auto colliderComp = std::make_unique<ColliderComponent>();
    [[maybe_unused]] auto *collider = colliderComp->GetCollider();
    AddSceneComponent(std::move(colliderComp));

    auto *window = Window::GetWindow("Main Window");

    [[maybe_unused]] const auto whiteTex = TextureManager::GetTextureFromFileName("white1x1.png");

    // 2D Camera (window)
    {
        auto obj = std::make_unique<Camera2D>();
        if (window) {
            obj->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
            const float w = static_cast<float>(window->GetClientWidth());
            const float h = static_cast<float>(window->GetClientHeight());
            obj->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
        }
        screenCamera2D_ = obj.get();
        AddObject2D(std::move(obj));
    }

    // 2D Camera (screenBuffer_)
    {
        auto obj = std::make_unique<Camera2D>();
        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object2D.DoubleSidedCulling.BlendNormal");
            const float w = static_cast<float>(screenBuffer_->GetWidth());
            const float h = static_cast<float>(screenBuffer_->GetHeight());
            obj->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
        }
        AddObject2D(std::move(obj));
    }

    // 3D Main Camera (screenBuffer_)
    {
        auto obj = std::make_unique<Camera3D>();
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 8.0f, -10.0f));
            tr->SetRotate(Vector3(M_PI * (30.0f / 180.0f), 0.0f, 0.0f));
        }
        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            const float w = static_cast<float>(screenBuffer_->GetWidth());
            const float h = static_cast<float>(screenBuffer_->GetHeight());
            obj->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
        }
        obj->SetFovY(0.7f);
        mainCamera3D_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Directional Light (両方に描く)
    {
        auto obj = std::make_unique<DirectionalLight>();
        if (auto *light = obj.get()) {
            light->SetEnabled(true);
            light->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            light->SetDirection(Vector3(4.0f, -2.0f, 1.0f));
            light->SetIntensity(1.6f);
        }
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    // ScreenBuffer用スプライト（最終表示）
    {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("ScreenBufferSprite");
        if (screenBuffer_) {
            if (auto *mat = obj->GetComponent2D<Material2D>()) {
                mat->SetTexture(screenBuffer_);
            }
        }
        obj->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
        screenSprite_ = obj.get();
        AddObject2D(std::move(obj));
    }

    // ScreenBuffer の表示サイズを比率維持で調整するコンポーネントを追加
    {
        auto comp = std::make_unique<ScreenBufferKeepRatio>();
        comp->SetSprite(screenSprite_);
        comp->SetTargetSize(0.0f, 0.0f); // ウィンドウサイズに合わせる
        if (screenBuffer_) {
            comp->SetSourceSize(static_cast<float>(screenBuffer_->GetWidth()), static_cast<float>(screenBuffer_->GetHeight()));
        }
        AddSceneComponent(std::move(comp));
    }

    auto *sceneChangeIn = GetSceneComponent<SceneChangeIn>();
    if (sceneChangeIn) {
        sceneChangeIn->Play();
    }

    // BGM再生
    const auto sound = AudioManager::GetSoundHandleFromFileName("titleBGM.mp3");
    bgmPlay_ = AudioManager::Play(sound, 1.0f, 0.0f, true);
}

TitleScene::~TitleScene() {
    if (bgmPlay_ != AudioManager::kInvalidPlayHandle) {
        AudioManager::Stop(bgmPlay_);
        bgmPlay_ = AudioManager::kInvalidPlayHandle;
    }
    ClearObjects2D();
    ClearObjects3D();
}

void TitleScene::OnUpdate() {
    // ScreenBuffer のサイズをウィンドウサイズに合わせる
    if (screenCamera2D_ && screenSprite_) {
        if (auto *window = Window::GetWindow("Main Window")) {
            const float w = static_cast<float>(window->GetClientWidth());
            const float h = static_cast<float>(window->GetClientHeight());
            screenCamera2D_->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            screenCamera2D_->SetViewportParams(0.0f, 0.0f, w, h);
        }
    }

    // SceneChangeOut 完了で次シーンへ
    if (auto *sceneChangeOut = GetSceneComponent<SceneChangeOut>()) {
        if (sceneChangeOut->IsFinished()) {
            ChangeToNextScene();
        }
    }
}

} // namespace KashipanEngine
