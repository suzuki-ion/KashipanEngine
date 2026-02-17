#pragma once
#include <KashipanEngine.h>
#include <algorithm>

namespace KashipanEngine {

class SmokeParticle final : public IObjectComponent3D {
public:
    struct SpawnBox {
        Vector3 min{ -1.0f, 0.0f, -1.0f };
        Vector3 max{ 1.0f, 1.0f, 1.0f };
    };

    SmokeParticle(
        SpawnBox spawnBox = {},
        float lifeTimeSec = 2.0f,
        Vector3 startScale = Vector3{ 0.5f, 0.5f, 0.5f },
        Vector3 endScale = Vector3{ 0.1f, 0.1f, 0.1f },
        Vector4 startColor = Vector4{ 0.5f, 0.5f, 0.5f, 1.0f },
        Vector4 endColor = Vector4{ 0.2f, 0.2f, 0.2f, 1.0f },
        float speed = 2.0f)
        : IObjectComponent3D("SmokeParticle", 1),
          spawnBox_(spawnBox),
          lifeTimeSec_(lifeTimeSec),
          startScale_(startScale),
          endScale_(endScale),
          startColor_(startColor),
          endColor_(endColor),
          speed_(speed) {
    }

    ~SmokeParticle() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<SmokeParticle>(spawnBox_, lifeTimeSec_, startScale_, endScale_, startColor_, endColor_, speed_);
        ptr->elapsed_ = elapsed_;
        ptr->velocityDir_ = velocityDir_;
        return ptr;
    }

    std::optional<bool> Initialize() override {
        transform_ = GetOwner3DContext()->GetComponent<Transform3D>();
        material_ = GetOwner3DContext()->GetComponent<Material3D>();
        Respawn_();
        if (lifeTimeSec_ > 0.0f) {
            elapsed_ = GetRandomFloat(0.0f, lifeTimeSec_);
        }
        return true;
    }

    std::optional<bool> Update() override {
        if (!transform_ || !material_) return true;
        const float dt = std::max(0.0f, GetDeltaTime());
        elapsed_ += dt;

        {
            const Vector3 pos = transform_->GetTranslate();
            transform_->SetTranslate(pos + velocityDir_ * (speed_ * dt));
        }

        float t = 1.0f;
        if (lifeTimeSec_ > 0.0f) {
            t = std::clamp(elapsed_ / lifeTimeSec_, 0.0f, 1.0f);
        }
        const Vector3 scale = Lerp(startScale_, endScale_, t);
        const Vector4 color = Lerp(startColor_, endColor_, t);
        transform_->SetScale(scale);
        material_->SetColor(color);

        if (elapsed_ >= lifeTimeSec_) {
            Respawn_();
        }
        return true;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

    void SetSpawnBox(const SpawnBox &b) { spawnBox_ = b; }
    void SetLifeTimeSec(float s) { lifeTimeSec_ = s; }

private:
    void Respawn_() {
        if (!transform_) return;
        const float x = GetRandomFloat(spawnBox_.min.x, spawnBox_.max.x);
        const float y = GetRandomFloat(spawnBox_.min.y, spawnBox_.max.y);
        const float z = GetRandomFloat(spawnBox_.min.z, spawnBox_.max.z);
        transform_->SetTranslate(Vector3(x, y, z));
        transform_->SetScale(startScale_);
        if (material_) {
            material_->SetColor(startColor_);
        }

        Vector3 dir{
            GetRandomFloat(-1.0f, 1.0f),
            1.0f,
            GetRandomFloat(-1.0f, 1.0f)
        };
        const float len = dir.Length();
        velocityDir_ = (len > 0.0001f) ? (dir / len) : Vector3(0.0f, 1.0f, 0.0f);
        elapsed_ = 0.0f;
    }

    Transform3D *transform_ = nullptr;
    Material3D *material_ = nullptr;

    SpawnBox spawnBox_{};
    float lifeTimeSec_ = 2.0f;
    Vector3 startScale_{ 0.5f, 0.5f, 0.5f };
    Vector3 endScale_{ 0.1f, 0.1f, 0.1f };
    Vector4 startColor_{ 0.5f, 0.5f, 0.5f, 1.0f };
    Vector4 endColor_{ 0.2f, 0.2f, 0.2f, 1.0f };
    float speed_ = 1.0f;
    float elapsed_ = 0.0f;
    Vector3 velocityDir_{ 0.0f, 1.0f, 0.0f };
};

} // namespace KashipanEngine
