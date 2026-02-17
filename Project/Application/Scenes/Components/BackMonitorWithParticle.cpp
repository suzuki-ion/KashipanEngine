#include "BackMonitorWithParticle.h"
#include "Objects/Components/ParticleMovement.h"
#include "BackMonitor.h"

namespace KashipanEngine {

BackMonitorWithParticle::BackMonitorWithParticle(ScreenBuffer* target)
    : BackMonitorRenderer("BackMonitorWithParticle", target) {}

BackMonitorWithParticle::~BackMonitorWithParticle() {}

void BackMonitorWithParticle::Initialize() {
    //EnsureParticlePool();
}

void BackMonitorWithParticle::EnsureParticlePool() {
    auto *ctx = GetOwnerContext();
    auto *target = GetTargetScreenBuffer();
    if (!ctx || !target) return;
    if (!particles_.empty()) return;

    const int poolSize = 32;
    for (int i = 0; i < poolSize; ++i) {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName(std::string("BackMonitor.Particle.") + std::to_string(i));
        if (auto *tr = obj->GetComponent2D<Transform2D>()) {
            float w = static_cast<float>(target->GetWidth());
            float h = static_cast<float>(target->GetHeight());
            tr->SetTranslate(Vector2{w * 0.5f, h * 0.5f});
            tr->SetScale(Vector2{32.0f, 32.0f});
        }
        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4{1.0f, 0.8f, 0.2f, 1.0f});
        }
        obj->AttachToRenderer(target, "Object2D.DoubleSidedCulling.BlendNormal");
        particles_.push_back(obj.get());
        ctx->AddObject2D(std::move(obj));
    }
}

void BackMonitorWithParticle::Update() {
    if (!IsActive()) return;
    auto bm = GetBackMonitor();
    if (!bm) return;
    if (!bm->IsReady()) return;

    if (particles_.empty()) return;

    elapsedTime_ += GetDeltaTime();

    auto target = GetTargetScreenBuffer();
    if (!target) return;

    float w = static_cast<float>(target->GetWidth());
    float h = static_cast<float>(target->GetHeight());
    Vector2 center{w * 0.5f, h * 0.5f};

    for (size_t i = 0; i < particles_.size(); ++i) {
        auto *p = particles_[i];
        if (!p) continue;
        if (auto *tr = p->GetComponent2D<Transform2D>()) {
            float angle = (static_cast<float>(i) / static_cast<float>(particles_.size())) * 6.28318548f + elapsedTime_ * 2.0f; // speed adjusted
            float radius = 50.0f + 30.0f * std::sin(elapsedTime_ * 1.0f + static_cast<float>(i));
            Vector2 pos = center + Vector2{std::cos(angle) * radius, std::sin(angle) * radius};
            tr->SetTranslate(pos);
        }
    }
}

} // namespace KashipanEngine
