#pragma once

#include "AttackBase.h"

#include <cmath>
#include <string>
#include <vector>

namespace KashipanEngine {

class AttackGearCircularOutside final : public AttackBase {
public:
    explicit AttackGearCircularOutside(Object3DBase *mover = nullptr, ScreenBuffer *screenBuffer = nullptr)
        : AttackBase("AttackGearCircularOutside", mover, screenBuffer) {}
    ~AttackGearCircularOutside() override = default;

    void SetTargetPosition(const Vector3 &p) { target_ = p; }
    const Vector3 &GetTargetPosition() const { return target_; }

protected:
    void AttackStartInitialize() override {
        constexpr int kCount = 8;
        constexpr float radius = 10.0f;

        // ちょっと外側にズレてスポーン（半径を少しだけ大きく）
        constexpr float spawnRadius = 1.0f;
        constexpr float twoPi = 6.28318530717958647692f;

        for (int i = 0; i < kCount; ++i) {
            const float a = twoPi * (static_cast<float>(i) / static_cast<float>(kCount));
            const float sx = target_.x + std::cos(a) * spawnRadius;
            const float sz = target_.z + std::sin(a) * spawnRadius;

            const float tx = target_.x + std::cos(a) * radius;
            const float tz = target_.z + std::sin(a) * radius;

            const Vector3 spawnPos{sx, 0.0f, sz};

            std::vector<MoveEntry> extra;
            extra.reserve(1);
            MoveEntry e;
            e.from = Vector3{sx, 1.0f, sz};
            e.to = Vector3{tx, 1.0f, tz};
            e.duration = 2.0f;
            e.easing = [](Vector3 a0, Vector3 b0, float t) { return EaseInOutBack(a0, b0, t); };
            extra.push_back(std::move(e));

            SpawnXZPlaneWithMoves(
                std::string("AttackGearCircularOutside_") + std::to_string(i),
                spawnPos,
                extra);
        }
    }

private:
    Vector3 target_{0.0f, 0.0f, 0.0f};
};

} // namespace KashipanEngine