#include "Scenes/Components/BackMonitor.h"

namespace KashipanEngine {

BackMonitor::BackMonitor() : ISceneComponent("BackMonitor", 1) {}
BackMonitor::~BackMonitor() {}

void BackMonitor::Initialize() {
    auto *context = GetOwnerContext();
    auto *sceneDefault = context->GetComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = sceneDefault->GetScreenBuffer3D();
    auto *shadowMapBuffer = sceneDefault->GetShadowMapBuffer();
    auto whiteTex = TextureManager::GetTextureFromFileName("white1x1.png");

    // ScreenBuffer生成
    screenBuffer_ = ScreenBuffer::Create(640, 360);
    if (!screenBuffer_) return;

    {
        DotMatrixEffect::Params dp{};
        dp.dotSpacing = 10.0f;
        dp.dotRadius = 4.0f;
        dp.threshold = 0.0f;
        dp.intensity = 2.0f;
        dp.monochrome = false;
        screenBuffer_->RegisterPostEffectComponent(std::make_unique<DotMatrixEffect>(dp));

        screenBuffer_->AttachToRenderer("ScreenBuffer_BackMonitor");
    }

    // Camera2D生成
    {
        auto obj = std::make_unique<Camera2D>();
        obj->SetName("Camera2D_BackMonitor");
        float w = static_cast<float>(screenBuffer_->GetWidth());
        float h = static_cast<float>(screenBuffer_->GetHeight());
        obj->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
        obj->SetViewportParams(0.0f, 0.0f, w, h);
        obj->AttachToRenderer(screenBuffer_, "Object2D.DoubleSidedCulling.BlendNormal");
        camera2D_ = obj.get();
        context->AddObject2D(std::move(obj));
    }
    // Camera3D生成
    {
        auto obj = std::make_unique<Camera3D>();
        obj->SetName("Camera3D_BackMonitor");
        obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        camera3D_ = obj.get();
        context->AddObject3D(std::move(obj));
    }

    // Sprite生成
    {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("Sprite_BackMonitor");
        if (auto *tr = obj->GetComponent2D<Transform2D>()) {
            float w = static_cast<float>(screenBuffer_->GetWidth());
            float h = static_cast<float>(screenBuffer_->GetHeight());
            tr->SetScale(Vector3{ w, h, 1.0f });
            tr->SetTranslate(Vector3{ w * 0.5f, h * 0.5f, 0.0f });
        }
        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            mat->SetTexture(screenBuffer3D);
        }
        obj->AttachToRenderer(screenBuffer_, "Object2D.DoubleSidedCulling.BlendNormal");
        sprite_ = obj.get();
        context->AddObject2D(std::move(obj));
    }

    // 板ポリの親オブジェクト生成
    {
        // 適当な3Dオブジェクトを親にする
        auto obj = std::make_unique<Triangle3D>();
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor Plane Parent");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3{ 25.6f, 14.4f, 1.0f });
            tr->SetTranslate(Vector3{ 10.0f, 9.5f, 35.0f });
        }
        planeParent_ = obj.get();
        context->AddObject3D(std::move(obj));
    }

    // 背景用Plane3D生成
    {
        auto obj = std::make_unique<Plane3D>();
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor Background Plane");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetParentTransform(planeParent_->GetComponent3D<Transform3D>());
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(whiteTex);
            mat->SetEnableLighting(false);
            mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
        }
        obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        planeBack_ = obj.get();
        context->AddObject3D(std::move(obj));
    }

    // モニター用Plane3D生成
    {
        auto obj = std::make_unique<Plane3D>();
        obj->SetUniqueBatchKey();
        obj->SetName("BackMonitor Plane");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetParentTransform(planeParent_->GetComponent3D<Transform3D>());
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(whiteTex);
            mat->SetEnableLighting(false);
        }
        obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        plane3D_ = obj.get();
        context->AddObject3D(std::move(obj));
    }
}

void BackMonitor::Finalize() {
    ScreenBuffer::DestroyNotify(screenBuffer_);
}

void BackMonitor::Update() {
    if (!plane3D_) return;

    // 最初からScreenBufferの内容を映そうとするとバリア張り替えのタイミングの関係でエラーになるのでここで調整

    if (planeMoveCount_ >= 2) return;
    ++planeMoveCount_;
    if (planeMoveCount_ < 2) return;

    if (auto *mat = plane3D_->GetComponent3D<Material3D>()) {
        mat->SetTexture(screenBuffer_);
        mat->SetEnableLighting(false);
    }
}

} // namespace KashipanEngine
