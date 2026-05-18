#pragma once
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix4x4.h"
#include "Assets/TextureManager.h"
#include "Utilities/EntityComponentSystem.h"

namespace KashipanEngine {

struct MaterialGPUBuffer {
    float enableLighting{ 1.0f };
    float enableEnvironmentMapping{ 1.0f };
    float enableShadowMapProjection{ 1.0f };
    float shininess{ 32.0f };
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 specularColor;
    Matrix4x4 uvTransform{ Matrix4x4::Identity() };
    uint32_t textureIndex{ 0 };
    uint32_t environmentTextureIndex{ 0 };
    float environmentCoefficient;
    float pad0;
};

struct Material {
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t textureHandle{ TextureManager::kInvalidHandle };
    bool enableLighting = true;
    bool enableEnvironmentMapping = true;
    bool enableShadowMapProjection = true;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation{ 0.0f, 0.0f, 0.0f };
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
    float shininess{ 32.0f };
    Vector4 specularColor;
    float environmentCoefficient;
    uint32_t environmentTextureHandle{ TextureManager::kInvalidHandle };
};

//--------- ヘルパー関数 ---------//

void RegisterMaterialComponents(const Entity &entity, ComponentStorage &componentStorage) {
    componentStorage.AddComponent<MaterialGPUBuffer>(entity);
    componentStorage.AddComponent<Material>(entity);
}

void UnregisterMaterialComponents(const Entity &entity, ComponentStorage &componentStorage) {
    componentStorage.RemoveComponent<MaterialGPUBuffer>(entity);
    componentStorage.RemoveComponent<Material>(entity);
}

} // namespace KashipanEngine