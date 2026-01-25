#include "SceneDefaultVariables.h"
#include "Scene/SceneContext.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"

namespace KashipanEngine {

void SceneDefaultVariables::Initialize() {
    auto *sceneContext = GetOwnerContext();
<<<<<<< HEAD:Project/KashipanEngine/Scene/Components/SceneDefaultVariables.cpp
    mainWindow_ = Window::GetWindow("Main Window");
=======
    mainWindow_ = Window::GetWindow("2301_CLUBOM");
>>>>>>> TD2_3:KashipanEngine/Scene/Components/SceneDefaultVariables.cpp

    screenBuffer3D_ = ScreenBuffer::Create(1920, 1080);
    screenBuffer2D_ = ScreenBuffer::Create(1920, 1080);
    shadowMapBuffer_ = ShadowMapBuffer::Create(2048, 2048);

    // ScreenBuffer3D用カメラ
    {
        auto obj = std::make_unique<Camera3D>();
        obj->SetName("Camera3D_ScreenBuffer3D");
        if (screenBuffer3D_) {
            obj->AttachToRenderer(screenBuffer3D_, "Object3D.Solid.BlendNormal");
        }
        mainCamera3D_ = obj.get();
        sceneContext->AddObject3D(std::move(obj));
    }
    // ScreenBuffer2D用カメラ
    {
        auto obj = std::make_unique<Camera2D>();
        obj->SetName("Camera2D_ScreenBuffer2D");
        if (screenBuffer2D_) {
            float w = static_cast<float>(screenBuffer2D_->GetWidth());
            float h = static_cast<float>(screenBuffer2D_->GetHeight());
            obj->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
            obj->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
        }
        mainCamera2D_ = obj.get();
        sceneContext->AddObject2D(std::move(obj));
    }
    
    // ScreenBuffer3D用スプライト
    {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("Sprite_ScreenBuffer3D");
        if (screenBuffer3D_) {
            if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                float w = static_cast<float>(screenBuffer3D_->GetWidth());
                float h = static_cast<float>(screenBuffer3D_->GetHeight());
                tr->SetScale(Vector3{ w, h, 1.0f });
                tr->SetTranslate(Vector3{ w * 0.5f, h * 0.5f, 0.0f });
            }
            if (auto* mat = obj->GetComponent2D<Material2D>()) {
                mat->SetTexture(screenBuffer3D_);
            }
            obj->AttachToRenderer(mainWindow_, "Object2D.DoubleSidedCulling.BlendNormal");
        }
        screenBuffer3DSprite_ = obj.get();
        sceneContext->AddObject2D(std::move(obj));
    }
    // ScreenBuffer2D用スプライト
    {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("Sprite_ScreenBuffer2D");
        if (screenBuffer2D_) {
            if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                float w = static_cast<float>(screenBuffer2D_->GetWidth());
                float h = static_cast<float>(screenBuffer2D_->GetHeight());
                tr->SetScale(Vector3{ w, h, 1.0f });
                tr->SetTranslate(Vector3{ w * 0.5f, h * 0.5f, 0.0f });
            }
            if (auto* mat = obj->GetComponent2D<Material2D>()) {
                mat->SetTexture(screenBuffer2D_);
            }
            obj->AttachToRenderer(mainWindow_, "Object2D.DoubleSidedCulling.BlendNormal");
        }
        screenBuffer2DSprite_ = obj.get();
        sceneContext->AddObject2D(std::move(obj));
    }

    // 平行光源
    {
        auto obj = std::make_unique<DirectionalLight>();
        obj->SetName("DirectionalLight");
        obj->AttachToRenderer(screenBuffer3D_, "Object3D.Solid.BlendNormal");
        directionalLight_ = obj.get();
        sceneContext->AddObject3D(std::move(obj));
    }
    // ライト管理用
    {
        auto obj = std::make_unique<LightManager>();
        obj->SetName("LightManager");
        obj->AttachToRenderer(screenBuffer3D_, "Object3D.Solid.BlendNormal");
        lightManager_ = obj.get();
        sceneContext->AddObject3D(std::move(obj));
    }
    // シャドウマッピング用ライトカメラ
    {
        auto obj = std::make_unique<Camera3D>();
        obj->SetName("LightCamera3D");
        obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
        lightCamera3D_ = obj.get();
        sceneContext->AddObject3D(std::move(obj));
    }
    // シャドウマッピング用バインダー
    {
        auto obj = std::make_unique<ShadowMapBinder>();
        obj->SetName("ShadowMapBinder");
        obj->SetShadowMapBuffer(shadowMapBuffer_);
        obj->SetCamera3D(lightCamera3D_);
        obj->AttachToRenderer(screenBuffer3D_, "Object3D.Solid.BlendNormal");
        shadowMapBinder_ = obj.get();
        sceneContext->AddObject3D(std::move(obj));
    }
    // Window用カメラ
    {
        auto obj = std::make_unique<Camera2D>();
        obj->SetName("Camera2D_Window");
        if (mainWindow_) {
            float w = static_cast<float>(mainWindow_->GetClientWidth());
            float h = static_cast<float>(mainWindow_->GetClientHeight());
            obj->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
            obj->AttachToRenderer(mainWindow_, "Object2D.DoubleSidedCulling.BlendNormal");
        }
        windowCamera2D_ = obj.get();
        sceneContext->AddObject2D(std::move(obj));
    }
}

void SceneDefaultVariables::Finalize() {
    ScreenBuffer::DestroyNotify(screenBuffer3D_);
    ScreenBuffer::DestroyNotify(screenBuffer2D_);
    ShadowMapBuffer::DestroyNotify(shadowMapBuffer_);
}

void SceneDefaultVariables::Update() {
    // Window用カメラのサイズ更新
    if (mainWindow_ && windowCamera2D_) {
        float w = static_cast<float>(mainWindow_->GetClientWidth());
        float h = static_cast<float>(mainWindow_->GetClientHeight());
        windowCamera2D_->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
        windowCamera2D_->SetViewportParams(0.0f, 0.0f, w, h);
    }
}

void SceneDefaultVariables::SetSceneComponents(std::function<bool(std::unique_ptr<ISceneComponent>)> registerFunc) {
    // Colliderコンポーネント
    {
        auto comp = std::make_unique<ColliderComponent>();
        colliderComp_ = comp.get();
        registerFunc(std::move(comp));
    }

    // ScreenBufferアスペクト比維持コンポーネント
    {
        auto comp = std::make_unique<ScreenBufferKeepRatio>();
        comp->AddSprite(screenBuffer2DSprite_,
            static_cast<float>(screenBuffer2D_ ? screenBuffer2D_->GetWidth() : 0),
            static_cast<float>(screenBuffer2D_ ? screenBuffer2D_->GetHeight() : 0));
        comp->AddSprite(screenBuffer3DSprite_,
            static_cast<float>(screenBuffer3D_ ? screenBuffer3D_->GetWidth() : 0),
            static_cast<float>(screenBuffer3D_ ? screenBuffer3D_->GetHeight() : 0));
        keepRatioComp_ = comp.get();
        registerFunc(std::move(comp));
    }

    // シャドウマップカメラ同期コンポーネント
    {
        auto comp = std::make_unique<ShadowMapCameraSync>();
        comp->SetMainCamera(mainCamera3D_);
        comp->SetLightCamera(lightCamera3D_);
        comp->SetDirectionalLight(directionalLight_);
        comp->SetShadowMapBinder(shadowMapBinder_);
        comp->SetShadowMapBuffer(shadowMapBuffer_);
        shadowMapCameraSync_ = comp.get();
        registerFunc(std::move(comp));
    }
}

} // namespace KashipanEngine