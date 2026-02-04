#include "WallBreakParticleManager.h"
#include "WallBreakParticle.h"

namespace KashipanEngine {

    WallBreakParticleManager::WallBreakParticleManager()
        : ISceneComponent("WallBreakParticleManager")
    {
        config_.initialSpeed = 30.0f;
        config_.speedVariation = 5.0f;
        config_.lifeTimeSec = 0.5f;
        config_.gravity = 0.0f;
        config_.damping = 0.98f;
        config_.baseScale = Vector3{ 1.0f, 1.0f, 1.0f };
        config_.color = Vector4{ 0.5f, 0.5f, 0.5f, 1.0f };
    }

    void WallBreakParticleManager::Initialize() {
        auto* ctx = GetOwnerContext();
        if (!ctx) return;

        particles_.clear();
        particles_.reserve(particlePoolSize_);

        for (int i = 0; i < particlePoolSize_; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("BombExplosionParticle_" + std::to_string(i));

            if (auto* mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(true);
                mat->SetColor(config_.color);
            }

            if (screenBuffer_) {
                obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            }

            Object3DBase* objPtr = obj.get();
            ctx->AddObject3D(std::move(obj));

            objPtr->RegisterComponent<WallBreakParticle>(config_);

            ParticleInfo info;
            info.object = objPtr;
            info.component = objPtr->GetComponent3D<WallBreakParticle>();
            particles_.push_back(std::move(info));
        }
    }

    void WallBreakParticleManager::Update() {}

    void WallBreakParticleManager::SpawnParticles(const Vector3& position, int particleCount) {
        int spawnedCount = 0;
        for (auto& particle : particles_) {
            if (spawnedCount >= particleCount) break;

            if (particle.component && !particle.component->IsAlive()) {
                particle.component->Spawn(position);
                ++spawnedCount;
            }
        }
    }

#if defined(USE_IMGUI)
    void WallBreakParticleManager::ShowImGui() {
        ImGui::DragInt("Particle Pool Size", &particlePoolSize_, 1, 10, 200);
        ImGui::DragInt("Particles Per Explosion", &particleCount_, 1, 1, 50);

        if (ImGui::TreeNode("Particle Config")) {
            ImGui::DragFloat("Initial Speed", &config_.initialSpeed, 0.1f, 0.0f, 20.0f);
            ImGui::DragFloat("Speed Variation", &config_.speedVariation, 0.1f, 0.0f, 10.0f);
            ImGui::DragFloat("Life Time (sec)", &config_.lifeTimeSec, 0.05f, 0.1f, 5.0f);
            ImGui::DragFloat("Gravity", &config_.gravity, 0.1f, 0.0f, 30.0f);
            ImGui::DragFloat("Damping", &config_.damping, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat3("Base Scale", &config_.baseScale.x, 0.01f, 0.01f, 2.0f);
            ImGui::ColorEdit4("Color", &config_.color.x);

            for (auto& particle : particles_) {
                if (particle.component) {
                    particle.component->SetConfig(config_);
                }
            }

            ImGui::TreePop();
        }
    }
#endif

} // namespace KashipanEngine
