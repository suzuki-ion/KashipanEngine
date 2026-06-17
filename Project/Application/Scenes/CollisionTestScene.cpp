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

    const size_t boxCounts = 128;
    const size_t sphereCounts = 256;

    // カメラを斜め上から見下ろす位置に配置
    if (auto *transform = mainCamera3D->GetComponent3D<Transform3D>()) {
        transform->SetTranslate(Vector3(-32.0f, 16.0f, -32.0f));
        transform->SetRotate(Vector3(ToRadians(30.0f), ToRadians(30.0f), 0.0f));
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
    Object3DBase *areaParentPtr = areaParent.get();
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
        tr->SetScale(Vector3(areaSize_.x, 1.0f, areaSize_.z));
        tr->SetParentObject(areaParentPtr);
    }
    areaFloor->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaFloor->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaFloor));
    // エリアの壁 (左)
    auto areaWallLeft = std::make_unique<Box>();
    areaWallLeft->SetName("AreaWallLeft");
    if (auto *tr = areaWallLeft->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3(1.0f, areaSize_.y, areaSize_.z));
        tr->SetTranslate(Vector3(-areaSize_.x * 0.5f, areaSize_.y * 0.5f, 0.0f));
        tr->SetParentObject(areaParentPtr);
    }
    areaWallLeft->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaWallLeft->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaWallLeft));
    // エリアの壁 (右)
    auto areaWallRight = std::make_unique<Box>();
    areaWallRight->SetName("AreaWallRight");
    if (auto *tr = areaWallRight->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3(1.0f, areaSize_.y, areaSize_.z));
        tr->SetTranslate(Vector3(areaSize_.x * 0.5f, areaSize_.y * 0.5f, 0.0f));
        tr->SetParentObject(areaParentPtr);
    }
    areaWallRight->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaWallRight->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaWallRight));
    // エリアの壁 (奥)
    auto areaWallBack = std::make_unique<Box>();
    areaWallBack->SetName("AreaWallBack");
    if (auto *tr = areaWallBack->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3(areaSize_.x, areaSize_.y, 1.0f));
        tr->SetTranslate(Vector3(0.0f, areaSize_.y * 0.5f, areaSize_.z * 0.5f));
        tr->SetParentObject(areaParentPtr);
    }
    areaWallBack->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaWallBack->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaWallBack));
    // エリアの壁 (手前)
    auto areaWallFront = std::make_unique<Box>();
    areaWallFront->SetName("AreaWallFront");
    if (auto *tr = areaWallFront->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3(areaSize_.x, areaSize_.y, 1.0f));
        tr->SetTranslate(Vector3(0.0f, areaSize_.y * 0.5f, -areaSize_.z * 0.5f));
        tr->SetParentObject(areaParentPtr);
    }
    areaWallFront->RegisterComponent(std::make_unique<Collision3D>(areaColliderInfo));
    areaWallFront->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(areaWallFront));

    // テスト用の地面
    /*auto groundObj = ModelManager::GetModelDataFromFileName("ground.obj");
    auto ground = std::make_unique<Model>(groundObj);
    ground->SetName("Ground");
    if (auto *tr = ground->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3(0.0f, 2.0f, 0.0f));
        tr->SetScale(Vector3(8.0f, 8.0f, 8.0f));
    }
    ColliderInfo3D groundColliderInfo{};
    auto groundModelData = ModelManager::GetModelDataFromFileName("ground.obj");
    std::vector<Vector3> groundVerticesForCollider;
    std::vector<uint32_t> groundIndicesForCollider;
    for (const auto &v : groundModelData.GetVertices()) {
        groundVerticesForCollider.emplace_back(v.px, v.py, v.pz);
    }
    for (const auto &idx : groundModelData.GetIndices()) {
        groundIndicesForCollider.push_back(idx);
    }
    groundColliderInfo.shape = ColliderInfo3D::ConcaveMeshShape3D {
        .vertices = groundVerticesForCollider,
        .indices = groundIndicesForCollider,
        .scale = Vector3(8.0f, 8.0f, 8.0f)
    };
    ground->RegisterComponent(std::make_unique<Collision3D>(groundColliderInfo));
    ground->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
    AddObject3D(std::move(ground));*/

    // ランダムな位置に Box と Sphere を配置
    for (size_t i = 0; i < boxCounts; ++i) {
        auto box = std::make_unique<Box>();
        box->SetName("Box" + std::to_string(i));
        box->RegisterComponent(std::make_unique<RigidBody3D>());
        box->RegisterComponent(std::make_unique<Collision3D>(boxColliderInfo));
        box->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        auto *ptr = box.get();
        AddObject3D(std::move(box));
        RespawnAreaObject(ptr);
        areaObjects_.push_back(ptr);
    }
    for (size_t i = 0; i < sphereCounts; ++i) {
        auto sphere = std::make_unique<Sphere>();
        sphere->SetName("Sphere" + std::to_string(i));
        sphere->RegisterComponent(std::make_unique<RigidBody3D>());
        sphere->RegisterComponent(std::make_unique<Collision3D>(sphereColliderInfo));
        sphere->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        auto *ptr = sphere.get();
        AddObject3D(std::move(sphere));
        RespawnAreaObject(ptr);
        areaObjects_.push_back(ptr);
    }
}

CollisionTestScene::~CollisionTestScene() {}

void CollisionTestScene::OnUpdate() {
    //// エリアのオブジェクトがエリア外に出たらランダムな位置に再配置
    //for (auto *obj : areaObjects_) {
    //    auto *tr = obj->GetComponent3D<Transform3D>();
    //    if (!tr) continue;
    //    const Vector3 &pos = tr->GetTranslate();
    //    if (pos.x < -areaSize_.x * 0.5f || pos.x > areaSize_.x * 0.5f ||
    //        pos.y < 0.0f || pos.y > areaSize_.y ||
    //        pos.z < -areaSize_.z * 0.5f || pos.z > areaSize_.z * 0.5f) {
    //        RespawnAreaObject(obj);
    //    }
    //}
}

void CollisionTestScene::RespawnAreaObject(Object3DBase *obj) {
    auto *areaObj = GetObject3D(obj);
    if (!areaObj) return;
    auto *tr = areaObj->GetComponent3D<Transform3D>();
    if (!tr) return;

    tr->SetTranslate(Vector3(
        GetRandomFloat(-areaSize_.x * 0.5f + 1.0f, areaSize_.x * 0.5f - 1.0f),
        GetRandomFloat(0.5f, areaSize_.y - 0.5f),
        GetRandomFloat(-areaSize_.z * 0.5f + 1.0f, areaSize_.z * 0.5f - 1.0f)
    ));
    tr->SetRotate(Vector3(
        GetRandomFloat(0.0f, ToRadians(360.0f)),
        GetRandomFloat(0.0f, ToRadians(360.0f)),
        GetRandomFloat(0.0f, ToRadians(360.0f))
    ));
}

} // namespace KashipanEngine
