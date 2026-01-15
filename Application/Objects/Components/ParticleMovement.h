#pragma once
#include <KashipanEngine.h>

#include <algorithm>
#include <memory>

namespace KashipanEngine {

class ParticleMovement final : public IObjectComponent3D {
public:
    struct SpawnBox {
        Vector3 min{ -5.0f, 0.0f, -5.0f };
        Vector3 max{ 5.0f, 5.0f, 5.0f };
    };

    ParticleMovement(
        SpawnBox spawnBox = {},
        float speed = 2.0f,
        float lifeTimeSec = 2.0f,
        Vector3 baseScale = Vector3{ 0.5f, 0.5f, 0.5f })
        : IObjectComponent3D("ParticleMovement", 1),
          spawnBox_(spawnBox),
          speed_(speed),
          lifeTimeSec_(lifeTimeSec),
          baseScale_(baseScale) {
    }

    ~ParticleMovement() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ParticleMovement>(spawnBox_, speed_, lifeTimeSec_, baseScale_);
        ptr->elapsed_ = elapsed_;
        ptr->velocityDir_ = velocityDir_;
        return ptr;
    }

    std::optional<bool> Initialize() override {
        Respawn_();
        elapsed_ = GetRandomFloat(0.0f, lifeTimeSec_);
        transform_ = GetOwner3DContext()->GetComponent<Transform3D>();
        return true;
    }

    std::optional<bool> Update() override {
        if (!transform_) return true;
        const float dt = std::max(0.0f, GetDeltaTime());
        elapsed_ += dt;

        if (lifeTimeSec_ <= 0.0f) {
            Respawn_();
            return true;
        }

        // movement
        {
            const Vector3 pos = transform_->GetTranslate();
            transform_->SetTranslate(pos + velocityDir_ * (speed_ * dt));
        }

        // scale animation
        {
            const float half = lifeTimeSec_ * 0.5f;
            float s = 0.0f;
            if (elapsed_ < half) {
                const float t = (half > 0.0f) ? std::clamp(elapsed_ / half, 0.0f, 1.0f) : 1.0f;
                s = EaseOutCubic(0.0f, 1.0f, t);
            } else {
                const float t = (half > 0.0f) ? std::clamp((elapsed_ - half) / half, 0.0f, 1.0f) : 1.0f;
                s = EaseInCubic(1.0f, 0.0f, t);
            }
            transform_->SetScale(baseScale_ * s);
        }

        if (elapsed_ >= lifeTimeSec_) {
            Respawn_();
        }

        return true;
    }

    void SetSpawnBox(const SpawnBox &b) { spawnBox_ = b; }
    void SetSpeed(float s) { speed_ = s; }
    void SetLifeTimeSec(float s) { lifeTimeSec_ = s; }
    void SetBaseScale(const Vector3 &s) { baseScale_ = s; }

    const SpawnBox &GetSpawnBox() const { return spawnBox_; }
    float GetSpeed() const { return speed_; }
    float GetLifeTimeSec() const { return lifeTimeSec_; }
    const Vector3 &GetBaseScale() const { return baseScale_; }
    float GetElapsedSec() const { return elapsed_; }
    float GetNormalizedLife() const {
        if (lifeTimeSec_ <= 0.0f) return 0.0f;
        return std::clamp(elapsed_ / lifeTimeSec_, 0.0f, 1.0f);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    void Respawn_() {
        if (!transform_) return;
        const float x = GetRandomFloat(spawnBox_.min.x, spawnBox_.max.x);
        const float y = GetRandomFloat(spawnBox_.min.y, spawnBox_.max.y);
        const float z = GetRandomFloat(spawnBox_.min.z, spawnBox_.max.z);
        transform_->SetTranslate(Vector3(x, y, z));

        // Random direction (avoid near-zero)
        Vector3 dir{
            GetRandomFloat(-1.0f, 1.0f),
            GetRandomFloat(-1.0f, 1.0f),
            GetRandomFloat(-1.0f, 1.0f)
        };
        const float len = dir.Length();
        velocityDir_ = (len > 0.0001f) ? (dir / len) : Vector3(0.0f, 1.0f, 0.0f);

        elapsed_ = 0.0f;

        transform_->SetScale(Vector3(0.0f, 0.0f, 0.0f));
    }

    Transform3D *transform_ = nullptr;

    SpawnBox spawnBox_{};
    float speed_ = 2.0f;
    float lifeTimeSec_ = 2.0f;
    Vector3 baseScale_{ 0.5f, 0.5f, 0.5f };

    float elapsed_ = 0.0f;
    Vector3 velocityDir_{ 0.0f, 1.0f, 0.0f };
};

} // namespace KashipanEngine
