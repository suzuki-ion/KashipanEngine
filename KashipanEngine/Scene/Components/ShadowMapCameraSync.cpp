#include "Scene/Components/ShadowMapCameraSync.h"

#include <algorithm>
#include <array>
#include <cfloat>

#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"
#include "Objects/SystemObjects/ShadowMapBinder.h"

namespace KashipanEngine {

namespace {

struct Float3 { float x, y, z; };

static Vector3 TransformPoint(const Matrix4x4& m, const Vector3& p) {
    const float x = p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + 1.0f * m.m[3][0];
    const float y = p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + 1.0f * m.m[3][1];
    const float z = p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + 1.0f * m.m[3][2];
    const float w = p.x * m.m[0][3] + p.y * m.m[1][3] + p.z * m.m[2][3] + 1.0f * m.m[3][3];

    if (w != 0.0f) {
        return Vector3(x / w, y / w, z / w);
    }
    return Vector3(x, y, z);
}

static std::array<Vector3, 8> ComputeCameraFrustumCornersWorld(const Camera3D& cam) {
    const Matrix4x4 invVP = cam.GetViewProjectionMatrix().Inverse();

    const std::array<Vector3, 8> ndc = {
        Vector3(-1.0f, -1.0f, 0.0f),
        Vector3(-1.0f,  1.0f, 0.0f),
        Vector3( 1.0f,  1.0f, 0.0f),
        Vector3( 1.0f, -1.0f, 0.0f),
        Vector3(-1.0f, -1.0f, 1.0f),
        Vector3(-1.0f,  1.0f, 1.0f),
        Vector3( 1.0f,  1.0f, 1.0f),
        Vector3( 1.0f, -1.0f, 1.0f),
    };

    std::array<Vector3, 8> world{};
    for (size_t i = 0; i < world.size(); ++i) {
        world[i] = TransformPoint(invVP, ndc[i]);
    }
    return world;
}

static Vector3 Average(const std::array<Vector3, 8>& v) {
    Vector3 s(0.0f, 0.0f, 0.0f);
    for (const auto& p : v) {
        s += p;
    }
    return s * (1.0f / static_cast<float>(v.size()));
}

} // namespace

ShadowMapCameraSync::ShadowMapCameraSync()
    : ISceneComponent("ShadowMapCameraSync", 0xFF) {
    SetUpdatePriority(0);
}

void ShadowMapCameraSync::Update() {
    SyncOnce();
}

void ShadowMapCameraSync::SyncOnce() {
    if (!mainCamera_ || !lightCamera_ || !light_) return;

    auto* lightTr = lightCamera_->GetComponent3D<Transform3D>();
    if (!lightTr) return;

    const auto cornersWS = ComputeCameraFrustumCornersWorld(*mainCamera_);
    const Vector3 centerWS = Average(cornersWS);

    const Vector3 lightDir = light_->GetDirection().Normalize();
    const Vector3 lightPos = centerWS - lightDir * distanceFromTarget_;

    Vector3 rot(0.0f, 0.0f, 0.0f);
    rot.x = std::asin(-lightDir.y);
    rot.y = std::atan2(lightDir.x, lightDir.z);

    lightTr->SetTranslate(lightPos);
    lightTr->SetRotate(rot);

    const Matrix4x4 lightView = lightCamera_->GetViewMatrix();

    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float minZ = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;
    float maxZ = -FLT_MAX;

    for (const auto& pWS : cornersWS) {
        const Vector3 pLS = TransformPoint(lightView, pWS);
        minX = std::min(minX, pLS.x);
        minY = std::min(minY, pLS.y);
        minZ = std::min(minZ, pLS.z);
        maxX = std::max(maxX, pLS.x);
        maxY = std::max(maxY, pLS.y);
        maxZ = std::max(maxZ, pLS.z);
    }

    const float nearClip = std::max(0.001f, minZ - depthMargin_);
    const float farClip = maxZ + depthMargin_;

    lightCamera_->SetCameraType(Camera3D::CameraType::Orthographic);
    lightCamera_->SetOrthographicParams(minX, maxY, maxX, minY, nearClip, farClip);

    if (shadowMapBinder_) {
        shadowMapBinder_->SetLightViewProjectionMatrix(lightCamera_->GetViewProjectionMatrix());
    }
}

} // namespace KashipanEngine
