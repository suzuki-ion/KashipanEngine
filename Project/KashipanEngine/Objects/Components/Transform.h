#pragma once
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4x4.h"
#include "Utilities/EntityComponentSystem.h"

namespace KashipanEngine {

struct Transform {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Quaternion rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
};

struct TransformParent {
    Entity parentEntity{ Entity(-1, 0) };
    size_t lastKnownVersion{ 0 };
};

struct TransformVersion {
    size_t version{ 1 }; // 初回の行列更新のために1からスタート
};

struct TransformWorldMatrix {
    Matrix4x4 matrix{ Matrix4x4::Identity() };
    size_t lastKnownVersion{ 0 };
};

//--------- ヘルパー関数 ---------//

void RegisterTransformComponents(const Entity &entity, ComponentStorage &componentStorage) {
    componentStorage.AddComponent<Transform>(entity);
    componentStorage.AddComponent<TransformWorldMatrix>(entity);
    componentStorage.AddComponent<TransformVersion>(entity);
}

void UnregisterTransformComponents(const Entity &entity, ComponentStorage &componentStorage) {
    componentStorage.RemoveComponent<Transform>(entity);
    componentStorage.RemoveComponent<TransformWorldMatrix>(entity);
    componentStorage.RemoveComponent<TransformVersion>(entity);
}

void UpdateTransformVersion(const Entity &entity, ComponentStorage &componentStorage) {
    if (auto *version = componentStorage.GetComponent<TransformVersion>(entity)) {
        version->version++;
    }
}

void SetParent(const Entity &entity, const Entity &parentEntity, ComponentStorage &componentStorage) {
    auto *parent = componentStorage.GetComponent<TransformParent>(entity);
    if (!parent) {
        parent = componentStorage.AddComponent<TransformParent>(entity);
    }
    parent->parentEntity = parentEntity;
    parent->lastKnownVersion = 0; // 親のバージョンをリセット
    UpdateTransformVersion(entity, componentStorage);
}

void ClearParent(const Entity &entity, ComponentStorage &componentStorage) {
    componentStorage.RemoveComponent<TransformParent>(entity);
}

} // namespace KashipanEngine