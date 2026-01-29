#include "StageLighting.h"

namespace KashipanEngine {

StageLighting::StageLighting()
    : ISceneComponent("StageLighting", 1), lightManager_(nullptr) {}

void StageLighting::Initialize() {
    auto *ctx = GetOwnerContext();
    if (!ctx) return;
    auto *sceneDefaults = ctx->GetComponent<SceneDefaultVariables>();
    lightManager_ = sceneDefaults ? sceneDefaults->GetLightManager() : nullptr;
    directionalLight_ = sceneDefaults ? sceneDefaults->GetDirectionalLight() : nullptr;
    auto *screenBuffer3D = sceneDefaults ? sceneDefaults->GetScreenBuffer3D() : nullptr;

    directionalLightOriginalColor_ = directionalLight_ ? directionalLight_->GetColor() : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightOriginalIntensity_ = directionalLight_ ? directionalLight_->GetIntensity() : 1.0f;

    // --- centerRotateSpotLight_ (3 lights) ---
    const size_t centerCount = 3;
    const float centerRadius = 4.0f;
    for (size_t i = 0; i < centerCount; ++i) {
        auto spotLight = std::make_unique<SpotLight>();
        spotLight->SetName("StageLighting.CenterRotateSpotLight." + std::to_string(i));
        RandomizeAndEnableSpotLight(spotLight.get());

        // position at stage center top
        spotLight->SetPosition(Vector3{ 10.0f, 10.0f, 10.0f });

        // direction: point to circle around origin at radius centerRadius
        float angle = (2.0f * 3.14159265f * static_cast<float>(i)) / static_cast<float>(centerCount);
        float dx = centerRadius * std::cos(angle);
        float dz = centerRadius * std::sin(angle);
        Vector3 target{ dx, 0.0f, dz };
        Vector3 dir = Vector3(target.x - 0.0f, target.y - 10.0f, target.z - 0.0f).Normalize();
        spotLight->SetDirection(dir);

        spotLight->SetRange(64.0f);
        spotLight->SetInnerAngle(0.0f);
        spotLight->SetOuterAngle(0.3f);
        spotLight->SetIntensity(1.0f);
        spotLight->SetDecay(0.0f);
        spotLight->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        lightManager_->AddSpotLight(spotLight.get());
        centerRotateSpotLight_.push_back(spotLight.get());
        ctx->AddObject3D(std::move(spotLight));
    }

    // --- rhythmicalSpotLight_ (grid covering 0..20 in XZ) ---
    const float stageMin = 0.0f;
    const float stageMax = 20.0f;
    const int gridCount = 4; // 4x4 = 16
    const float step = (stageMax - stageMin) / static_cast<float>(gridCount - 1);
    for (int ix = 0; ix < gridCount; ++ix) {
        for (int iz = 0; iz < gridCount; ++iz) {
            auto spotLight = std::make_unique<SpotLight>();
            int index = ix * gridCount + iz;
            spotLight->SetName("StageLighting.RhythmicalSpotLight." + std::to_string(index));
            RandomizeAndEnableSpotLight(spotLight.get());

            float x = stageMin + step * static_cast<float>(ix);
            float z = stageMin + step * static_cast<float>(iz);
            spotLight->SetPosition(Vector3{ x, 10.0f, z });
            spotLight->SetDirection(Vector3{ 0.0f, -1.0f, 0.0f });

            spotLight->SetRange(64.0f);
            spotLight->SetInnerAngle(0.0f);
            spotLight->SetOuterAngle(0.3f);
            spotLight->SetIntensity(1.0f);
            spotLight->SetDecay(0.0f);
            spotLight->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            lightManager_->AddSpotLight(spotLight.get());
            rhythmicalSpotLight_.push_back(spotLight.get());
            ctx->AddObject3D(std::move(spotLight));
        }
    }

    // --- stageOutsideSpotLight_ (around perimeter) ---
    // We will place 16 lights around the perimeter from -8..28 equally spaced
    const int outsideCount = 16;
    const float outerMin = -8.0f;
    const float outerMax = 28.0f;
    // perimeter positions: place along four edges evenly
    for (int i = 0; i < outsideCount; ++i) {
        auto spotLight = std::make_unique<SpotLight>();
        spotLight->SetName("StageLighting.OutsideSpotLight." + std::to_string(i));
        RandomizeAndEnableSpotLight(spotLight.get());

        float t = static_cast<float>(i) / static_cast<float>(outsideCount);
        // map t to perimeter coordinate
        float perimeter = outerMax - outerMin;
        float perimPos = t * (4.0f * perimeter);
        float x = 0.0f;
        float z = 0.0f;
        if (perimPos < perimeter) {
            // top edge (-4..24, z = -4)
            x = outerMin + perimPos;
            z = outerMin;
        } else if (perimPos < 2.0f * perimeter) {
            // right edge (x = 24, -4..24)
            x = outerMax;
            z = outerMin + (perimPos - perimeter);
        } else if (perimPos < 3.0f * perimeter) {
            // bottom edge (24..-4, z = 24)
            x = outerMax - (perimPos - 2.0f * perimeter);
            z = outerMax;
        } else {
            // left edge (x = -4, 24..-4)
            x = outerMin;
            z = outerMax - (perimPos - 3.0f * perimeter);
        }
        spotLight->SetPosition(Vector3{ x, 10.0f, z });

        // direction: point towards nearest point on stage [-4..24]
        float clampedX = std::clamp(x, -4.0f, 24.0f);
        float clampedZ = std::clamp(z, -4.0f, 24.0f);
        Vector3 target{ clampedX, 0.0f, clampedZ };
        Vector3 dir = Vector3(target.x - x, target.y - 10.0f, target.z - z).Normalize();
        spotLight->SetDirection(dir);

        spotLight->SetRange(64.0f);
        spotLight->SetInnerAngle(0.0f);
        spotLight->SetOuterAngle(0.3f);
        spotLight->SetIntensity(4.0f);
        spotLight->SetDecay(0.0f);
        spotLight->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        lightManager_->AddSpotLight(spotLight.get());
        stageOutsideSpotLight_.push_back(spotLight.get());
        ctx->AddObject3D(std::move(spotLight));
    }
}

void StageLighting::Finalize() {
}

void StageLighting::Update() {
    float deltaTime = GetDeltaTime();
    if (isDeadLightingActive_) {
        UpdateDeadLighting(deltaTime);
        return;
    }

    UpdateCenterRotateSpotLights(deltaTime);
    UpdateRhythmicalSpotLights(deltaTime);
    UpdateStageOutsideSpotLights(deltaTime);
}

void StageLighting::UpdateCenterRotateSpotLights(float deltaTime) {
    // rotate around Y axis
    static float accumulated = 0.0f;
    accumulated += deltaTime; // radians per second approximation
    const float speed = 1.0f; // rotation speed
    float angleOffset = accumulated * speed;
    const float radius = 8.0f;
    for (size_t i = 0; i < centerRotateSpotLight_.size(); ++i) {
        float angle = angleOffset + (2.0f * 3.14159265f * static_cast<float>(i)) / static_cast<float>(centerRotateSpotLight_.size());
        float dx = radius * std::cos(angle);
        float dz = radius * std::sin(angle);
        Vector3 target{ dx, 0.0f, dz };
        SpotLight* sl = centerRotateSpotLight_[i];
        if (!sl) continue;
        // update direction to point from light position (0,10,0) to target
        Vector3 dir = Vector3(target.x - 0.0f, target.y - 10.0f, target.z - 0.0f).Normalize();
        sl->SetDirection(dir);
    }
}

void StageLighting::UpdateRhythmicalSpotLights(float /*deltaTime*/) {
    // intentionally empty
}

void StageLighting::UpdateStageOutsideSpotLights(float /*deltaTime*/) {
    // intentionally empty
}

void StageLighting::RandomizeAndEnableSpotLight(SpotLight* spot, bool enabled) {
    if (!spot) return;
    float r, g, b;
    do {
        r = GetRandomFloat(0.2f, 1.0f);
        g = GetRandomFloat(0.2f, 1.0f);
        b = GetRandomFloat(0.2f, 1.0f);
    } while ((r + g + b > 2.0f) && (r + g + b < 1.5f));
    spot->SetColor(Vector4{ r, g, b, 1.0f });
    spot->SetEnabled(enabled);
}

void StageLighting::StartDeadLighting() {
    if (isDeadLightingActive_) return;
    isDeadLightingActive_ = true;
    deadLightingElapsed_ = 0.0f;

    // disable all spot lights
    for (auto* s : centerRotateSpotLight_) {
        if (s) s->SetEnabled(false);
    }
    for (auto* s : rhythmicalSpotLight_) {
        if (s) s->SetEnabled(false);
    }
    for (auto* s : stageOutsideSpotLight_) {
        if (s) s->SetEnabled(false);
    }

    // set directional light to dead start color
    if (directionalLight_) {
        directionalLight_->SetColor(directionalLightDeadStartColor_);
        directionalLight_->SetIntensity(directionalLightDeadIntensity_);
    }
}

void StageLighting::ResetLighting() {
    // stop dead lighting if active
    isDeadLightingActive_ = false;
    deadLightingElapsed_ = 0.0f;

    // restore directional light color
    if (directionalLight_) {
        directionalLight_->SetColor(directionalLightOriginalColor_);
        directionalLight_->SetIntensity(directionalLightOriginalIntensity_);
    }

    // re-enable spot lights and randomize their colors similar to Initialize
    for (size_t i = 0; i < centerRotateSpotLight_.size(); ++i) {
        RandomizeAndEnableSpotLight(centerRotateSpotLight_[i], true);
    }
    for (size_t i = 0; i < rhythmicalSpotLight_.size(); ++i) {
        RandomizeAndEnableSpotLight(rhythmicalSpotLight_[i], true);
    }
    for (size_t i = 0; i < stageOutsideSpotLight_.size(); ++i) {
        RandomizeAndEnableSpotLight(stageOutsideSpotLight_[i], true);
    }
}

void StageLighting::UpdateDeadLighting(float deltaTime) {
    if (!isDeadLightingActive_) return;
    deadLightingElapsed_ += deltaTime;
    float t = deadLightingElapsed_ / deadLightingDuration_;
    if (t >= 1.0f) {
        deadLightingElapsed_ = deadLightingDuration_;
        t = 1.0f;
    }

    Vector4 newColor = Lerp(directionalLightDeadStartColor_, directionalLightDeadEndColor_, t);
    if (directionalLight_) {
        directionalLight_->SetColor(newColor);
    }
}

} // namespace KashipanEngine
