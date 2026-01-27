#include "BackMonitorWithParticle.h"
#include "Objects/Components/ParticleMovement.h"
#include <cmath>

namespace KashipanEngine {

BackMonitorWithParticle::BackMonitorWithParticle(ScreenBuffer* target)
    : BackMonitorRenderer("BackMonitorWithParticle", target) {}

BackMonitorWithParticle::~BackMonitorWithParticle() {}

void BackMonitorWithParticle::Initialize() {
    EnsureParticlePool();
}

void BackMonitorWithParticle::EnsureParticlePool() {
    auto ctx = GetOwnerContext();
    if (!ctx || !target_) return;
    if (!particles_.empty()) return;

    const int poolSize = 32;
    for (int i = 0; i < poolSize; ++i) {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName(std::string("BackMonitor_Particle_") + std::to_string(i));
        if (auto *tr = obj->GetComponent2D<Transform2D>()) {
            float w = static_cast<float>(target_->GetWidth());
            float h = static_cast<float>(target_->GetHeight());
            tr->SetTranslate(Vector2{w * 0.5f, h * 0.5f});
            tr->SetScale(Vector2{32.0f, 32.0f});
        }
        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4{1.0f, 0.8f, 0.2f, 1.0f});
        }
        obj->AttachToRenderer(target_, "Object2D.DoubleSidedCulling.Additive");
        particles_.push_back(obj.get());
        ctx->AddObject2D(std::move(obj));
    }
}

void BackMonitorWithParticle::Update() {
    if (particles_.empty()) return;

    // increment frame counter
    ++frameCounter_;

    // simple animation: move particles outward from center
    float w = static_cast<float>(target_->GetWidth());
    float h = static_cast<float>(target_->GetHeight());
    Vector2 center{w * 0.5f, h * 0.5f};

    for (size_t i = 0; i < particles_.size(); ++i) {
        auto *p = particles_[i];
        if (!p) continue;
        if (auto *tr = p->GetComponent2D<Transform2D>()) {
            float angle = (static_cast<float>(i) / static_cast<float>(particles_.size())) * 6.28318548f + static_cast<float>(frameCounter_) * 0.02f;
            float radius = 50.0f + 30.0f * std::sin(static_cast<float>(frameCounter_) * 0.01f + static_cast<float>(i));
            Vector2 pos = center + Vector2{std::cos(angle) * radius, std::sin(angle) * radius};
            tr->SetTranslate(pos);
        }
    }
}

} // namespace KashipanEngine
