#include "BombExplosionParticleManager.h"
#include "BombExplosionParticle.h"

namespace KashipanEngine {

BombExplosionParticleManager::BombExplosionParticleManager()
    : ISceneComponent("BombExplosionParticleManager")
{
    config_.initialSpeed = 8.0f;
    config_.speedVariation = 3.0f;
    config_.lifeTimeSec = 1.2f;
    config_.gravity = 15.0f;
    config_.damping = 0.96f;
    config_.baseScale = Vector3{ 2.0f, 2.0f, 2.0f };
    config_.color = Vector4{ 1.0f, 0.6f, 0.2f, 1.0f };
}

void BombExplosionParticleManager::Initialize() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    particles_.clear();
    particles_.reserve(particlePoolSize_);

    for (int i = 0; i < particlePoolSize_; ++i) {
        auto obj = std::make_unique<Box>();
        obj->SetName("BombExplosionParticle_" + std::to_string(i));

        if (auto* mat = obj->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetColor(config_.color);
        }

        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        }

        Object3DBase* objPtr = obj.get();
        ctx->AddObject3D(std::move(obj));

        objPtr->RegisterComponent<BombExplosionParticle>(config_);
        
        ParticleInfo info;
        info.object = objPtr;
        info.component = objPtr->GetComponent3D<BombExplosionParticle>();
        particles_.push_back(std::move(info));
    }
}

void BombExplosionParticleManager::Update() {
}

void BombExplosionParticleManager::SpawnParticles(const Vector3& position, int particleCount) {
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
void BombExplosionParticleManager::ShowImGui() {
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
