#include "Scenes/CollisionTestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Objects/Components/PlayerInputHandler.h"

namespace KashipanEngine {

CollisionTestScene::CollisionTestScene()
    : SceneBase("CollisionTestScene") {}

void CollisionTestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainCamera3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera3D() : nullptr;
    auto *screenBuffer3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer3D() : nullptr;
    if (!mainCamera3D || !screenBuffer3D) return;

    const size_t boxCounts = 256;
    const size_t sphereCounts = 1024;
    const Vector3 areaSize(32.0f, 16.0f, 32.0f);

    // カメラを斜め上から見下ろす位置に配置
    if (auto *transform = mainCamera3D->GetComponent3D<Transform3D>()) {
        transform->SetTranslate(Vector3(0.0f, 16.0f, -32.0f));
        transform->SetRotate(Vector3(ToRadians(30.0f), 0.0f, 0.0f));
    }

    AddSceneComponent(std::make_unique<DebugCameraMovement>(mainCamera3D));
    if (auto *debugCameraMovement = GetSceneComponent<DebugCameraMovement>()) {
        debugCameraMovement->SetEnable(true);
    }

    // エリアの親オブジェクト(適当な三角形ポリゴン)
    auto areaParent = std::make_unique<Triangle3D>();
    areaParent->SetName("AreaParent");
    if (auto *mat = areaParent->GetComponent3D<Material3D>()) {
        mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
    }
    Transform3D *areaParentTr = nullptr;
    if (auto *tr = areaParent->GetComponent3D<Transform3D>()) {
        areaParentTr = tr;
    }
    AddObject3D(std::move(areaParent));

    // エリアのオブジェクトに使用する当たり判定情報
    ColliderInfo3D areaColliderInfo{};
    areaColliderInfo.shape = ColliderInfo3D::BoxShape3D{
        .center = Vector3(0.0f, 0.0f, 0.0f),
        .halfExtents = Vector3(0.5f, 0.5f, 0.5f)
    };
    // ボックスと球の当たり判定情報
    ColliderInfo3D boxColliderInfo{};
    boxColliderInfo.shape = ColliderInfo3D::BoxShape3D{
        .center = Vector3(0.0f, 0.0f, 0.0f),
        .halfExtents = Vector3(0.5f, 0.5f, 0.5f)
    };
    ColliderInfo3D sphereColliderInfo{};
    sphereColliderInfo.shape = ColliderInfo3D::SphereShape3D{
        .center = Vector3(0.0f, 0.0f, 0.0f),
        .radius = 0.5f
    };

    // エリアの床
    auto areaFloor = std::make_unique<Box>();
    areaFloor->SetName("AreaFloor");
    if (auto *tr = areaFloor->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3(areaSize.x, 1.0f, areaSize.z));
        tr->SetParentTransform(areaParentTr);
    }
    areaFloor->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaFloor->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaFloor));
    // エリアの壁 (左)
    auto areaWallLeft = std::make_unique<Box>();
    areaWallLeft->SetName("AreaWallLeft");
    if (auto *tr = areaWallLeft->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3(1.0f, areaSize.y, areaSize.z));
        tr->SetTranslate(Vector3(-areaSize.x * 0.5f, areaSize.y * 0.5f, 0.0f));
        tr->SetParentTransform(areaParentTr);
    }
    areaWallLeft->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaWallLeft->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaWallLeft));
    // エリアの壁 (右)
    auto areaWallRight = std::make_unique<Box>();
    areaWallRight->SetName("AreaWallRight");
    if (auto *tr = areaWallRight->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3(1.0f, areaSize.y, areaSize.z));
        tr->SetTranslate(Vector3(areaSize.x * 0.5f, areaSize.y * 0.5f, 0.0f));
        tr->SetParentTransform(areaParentTr);
    }
    areaWallRight->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaWallRight->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaWallRight));
    // エリアの壁 (奥)
    auto areaWallBack = std::make_unique<Box>();
    areaWallBack->SetName("AreaWallBack");
    if (auto *tr = areaWallBack->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3(areaSize.x, areaSize.y, 1.0f));
        tr->SetTranslate(Vector3(0.0f, areaSize.y * 0.5f, areaSize.z * 0.5f));
        tr->SetParentTransform(areaParentTr);
    }
    areaWallBack->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaWallBack->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaWallBack));
    // エリアの壁 (手前)
    auto areaWallFront = std::make_unique<Box>();
    areaWallFront->SetName("AreaWallFront");
    if (auto *tr = areaWallFront->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3(areaSize.x, areaSize.y, 1.0f));
        tr->SetTranslate(Vector3(0.0f, areaSize.y * 0.5f, -areaSize.z * 0.5f));
        tr->SetParentTransform(areaParentTr);
    }
    areaWallFront->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaWallFront->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaWallFront));

    // ランダムな位置に Box と Sphere を配置
    auto randomTransform = [areaSize](Transform3D *tr) {
        tr->SetTranslate(Vector3(
            GetRandomFloat(-areaSize.x * 0.5f + 1.0f, areaSize.x * 0.5f - 1.0f),
            GetRandomFloat(0.5f, areaSize.y - 0.5f),
            GetRandomFloat(-areaSize.z * 0.5f + 1.0f, areaSize.z * 0.5f - 1.0f)
        ));
    };
    for (size_t i = 0; i < boxCounts; ++i) {
        auto box = std::make_unique<Box>();
        box->SetName("Box" + std::to_string(i));
        if (auto *tr = box->GetComponent3D<Transform3D>()) {
            randomTransform(tr);
        }
        box->RegisterComponent(std::make_unique<RigidBody3D>());
        box->RegisterComponent(std::make_unique<Collision3D>(boxColliderInfo));
        box->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(box));
    }
    for (size_t i = 0; i < sphereCounts; ++i) {
        auto sphere = std::make_unique<Sphere>();
        sphere->SetName("Sphere" + std::to_string(i));
        if (auto *tr = sphere->GetComponent3D<Transform3D>()) {
            randomTransform(tr);
        }
        sphere->RegisterComponent(std::make_unique<RigidBody3D>());
        sphere->RegisterComponent(std::make_unique<Collision3D>(sphereColliderInfo));
        sphere->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(sphere));
    }
}

CollisionTestScene::~CollisionTestScene() {}

void CollisionTestScene::OnUpdate() {
}

} // namespace KashipanEngine
