#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix4x4.h"
#include "Utilities/EntityComponentSystem/EntityDefinition.h"
#include "Graphics/Resources/VertexBufferResource.h"
#include "Graphics/Resources/IndexBufferResource.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"
#include "Core/Window.h"

namespace KashipanEngine {

struct TransformComponent {
    Vector3 translate{0.0f, 0.0f, 0.0f};
    Vector3 rotate{0.0f, 0.0f, 0.0f};
    Vector3 scale{1.0f, 1.0f, 1.0f};
    Entity parent = Entity(-1);

    Matrix4x4 world = Matrix4x4::Identity();
    bool dirty = true;
    std::uint64_t version = 0;

    void MarkDirty() { dirty = true; }
};

struct MeshComponent {
    std::shared_ptr<VertexBufferResource> vertexBuffer;
    std::shared_ptr<IndexBufferResource> indexBuffer;
    UINT vertexCount = 0;
    UINT indexCount = 0;
    UINT vertexStride = 0;
    D3D12_VERTEX_BUFFER_VIEW vertexView{};
    D3D12_INDEX_BUFFER_VIEW indexView{};
};

struct MaterialComponent {
    struct InstanceData {
        float enableLighting = 1.0f;
        float enableShadowMapProjection = 1.0f;
        Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
        Matrix4x4 uvTransform = Matrix4x4::Identity();
        float shininess = 32.0f;
        Vector4 specularColor{1.0f, 1.0f, 1.0f, 1.0f};
    };

    InstanceData instanceData{};
    TextureManager::TextureHandle textureHandle = TextureManager::kInvalidHandle;
    SamplerManager::SamplerHandle samplerHandle = SamplerManager::kInvalidHandle;
};

struct RenderPipelineComponent {
    std::string pipelineName;
};

struct RenderTargetComponent {
    Window *window = nullptr;
};

struct CameraComponent {
    enum class CameraType {
        Perspective,
        Orthographic
    };

    struct CameraBuffer {
        Matrix4x4 view{};
        Matrix4x4 projection{};
        Matrix4x4 viewProjection{};
        Vector4 eyePosition{};
        float fov = 0.0f;
        float padding[3] = {0.0f, 0.0f, 0.0f};
    };

    CameraType type = CameraType::Perspective;
    bool isActive = true;

    float fovY = 0.45f;
    float aspectRatio = 1.0f;
    float nearClip = 0.1f;
    float farClip = 2048.0f;

    float orthoLeft = -10.0f;
    float orthoTop = 10.0f;
    float orthoRight = 10.0f;
    float orthoBottom = -10.0f;

    CameraBuffer buffer{};
};

struct DirectionalLightComponent {
    std::uint32_t enabled = 1;
    Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 direction{0.0f, -1.0f, 0.0f};
    float intensity = 1.0f;
};

struct PointLightComponent {
    std::uint32_t enabled = 1;
    Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 position{0.0f, 0.0f, 0.0f};
    float radius = 10.0f;
    float intensity = 1.0f;
    float decay = 1.0f;
    float padding = 0.0f;
};

struct SpotLightComponent {
    std::uint32_t enabled = 1;
    Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 position{0.0f, 0.0f, 0.0f};
    float distance = 10.0f;
    Vector3 direction{0.0f, -1.0f, 0.0f};
    float innerAngle = 0.5f;
    float outerAngle = 0.7f;
    float intensity = 1.0f;
    float decay = 1.0f;
    float padding = 0.0f;
};

} // namespace KashipanEngine
