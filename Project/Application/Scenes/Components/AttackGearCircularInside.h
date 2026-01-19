#pragma once

#include "AttackBase.h"

#include <cmath>
#include <string>
#include <vector>

namespace KashipanEngine {

class AttackGearCircularInside final : public AttackBase {
public:
    explicit AttackGearCircularInside(Object3DBase *mover = nullptr, ScreenBuffer *screenBuffer = nullptr)
        : AttackBase("AttackGearCircularInside", mover, screenBuffer) {}
    ~AttackGearCircularInside() override = default;

    /// @brief 攻撃のターゲット位置を設定する
    /// @param p ターゲットの位置
    void SetTargetPosition(const Vector3 &p) { target_ = p; }
    /// @brief 設定されているターゲット位置を取得する
    const Vector3 &GetTargetPosition() const { return target_; }

protected:
    void AttackStartInitialize() override {
        constexpr int kCount = 8;
        constexpr float radius = 10.0f;
        constexpr float twoPi = 6.28318530717958647692f;

        for (int i = 0; i < kCount; ++i) {
            const float a = twoPi * (static_cast<float>(i) / static_cast<float>(kCount));
            const float x = target_.x + std::cos(a) * radius;
            const float z = target_.z + std::sin(a) * radius;

            const Vector3 spawnPos{x, 0.0f, z};

            Vector3 dir = Vector3{x - target_.x, 0.0f, z - target_.z};
            const float len = dir.Length();
            if (len > 0.0001f) dir = dir / len;
            const Vector3 toPos = Vector3{
                target_.x + dir.x * (radius - 9.0f),
                1.0f,
                target_.z + dir.z * (radius - 9.0f)
            };

            std::vector<MoveEntry> extra;
            extra.reserve(1);
            MoveEntry e;
            e.from = Vector3{x, 1.0f, z};
            e.to = toPos;
            e.duration = 2.0f;
            e.easing = [](Vector3 a0, Vector3 b0, float t) { return EaseInOutBack(a0, b0, t); };
            extra.push_back(std::move(e));

            SpawnXZPlaneWithMoves(
                std::string("AttackGearCircularInside_") + std::to_string(i),
                spawnPos,
                extra);
        }
    }

private:
    Vector3 target_{0.0f, 0.0f, 0.0f};
};

} // namespace KashipanEngine