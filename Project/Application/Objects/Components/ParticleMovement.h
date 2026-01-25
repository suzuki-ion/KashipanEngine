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

    /// @brief パーティクルの生成ボックス、速度、寿命、基準スケールを指定して作成する
    /// @param spawnBox 生成位置の範囲
    /// @param speed 速度
    /// @param lifeTimeSec 寿命（秒）
    /// @param baseScale 基本スケール
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

    /// @brief デストラクタ
    ~ParticleMovement() override = default;

    /// @brief コンポーネントを複製して返す
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ParticleMovement>(spawnBox_, speed_, lifeTimeSec_, baseScale_);
        ptr->elapsed_ = elapsed_;
        ptr->velocityDir_ = velocityDir_;
        return ptr;
    }

    /// @brief 初期化処理（初期リスポーンなど）
    /// @return 初期化成功時は true
    std::optional<bool> Initialize() override {
        Respawn_();
        elapsed_ = GetRandomFloat(0.0f, lifeTimeSec_);
        transform_ = GetOwner3DContext()->GetComponent<Transform3D>();
        return true;
    }

    /// @brief 毎フレームの更新（移動・スケール・リスポーン）
    /// @return 更新成功時は true
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

    /// @brief 生成ボックスを設定する
    /// @param b 新しい生成ボックス
    void SetSpawnBox(const SpawnBox &b) { spawnBox_ = b; }
    /// @brief 速度を設定する
    /// @param s 速度
    void SetSpeed(float s) { speed_ = s; }
    /// @brief 寿命を秒単位で設定する
    /// @param s 寿命（秒）
    void SetLifeTimeSec(float s) { lifeTimeSec_ = s; }
    /// @brief 基準スケールを設定する
    /// @param s 基準スケール
    void SetBaseScale(const Vector3 &s) { baseScale_ = s; }

    /// @brief 生成ボックスを取得する
    const SpawnBox &GetSpawnBox() const { return spawnBox_; }
    /// @brief 速度を取得する
    float GetSpeed() const { return speed_; }
    /// @brief 寿命（秒）を取得する
    float GetLifeTimeSec() const { return lifeTimeSec_; }
    /// @brief 基準スケールを取得する
    const Vector3 &GetBaseScale() const { return baseScale_; }
<<<<<<< HEAD:Project/Application/Objects/Components/ParticleMovement.h
    /// @brief 経過秒数を取得する
    float GetElapsedSec() const { return elapsed_; }
    /// @brief 正規化された寿命（0..1）を取得する
=======
    float GetElapsedSec() const { return elapsed_; }
>>>>>>> TD2_3:Application/Objects/Components/ParticleMovement.h
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
