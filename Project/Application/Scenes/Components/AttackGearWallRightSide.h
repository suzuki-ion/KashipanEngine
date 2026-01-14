#pragma once

#include "AttackBase.h"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

namespace KashipanEngine {

class AttackGearWallRightSide final : public AttackBase {
public:
    explicit AttackGearWallRightSide(Object3DBase *mover = nullptr, ScreenBuffer *screenBuffer = nullptr)
        : AttackBase("AttackGearWallRightSide", mover, screenBuffer) {}
    ~AttackGearWallRightSide() override = default;

protected:
    void AttackStartInitialize() override {
        constexpr int kCount = 10;
        constexpr float minZ = -5.0f;
        constexpr float maxZ = 5.0f;
        const float step = (maxZ - minZ) / static_cast<float>(kCount - 1);

        std::vector<int> indices;
        indices.reserve(kCount);
        for (int i = 0; i < kCount; ++i) indices.push_back(i);

        std::mt19937 rng{34567u};
        std::shuffle(indices.begin(), indices.end(), rng);
        const int removeCount = 1 + (rng() % 2);
        indices.resize(static_cast<size_t>(kCount - removeCount));
        std::sort(indices.begin(), indices.end());

        for (int i : indices) {
            const float z = minZ + step * static_cast<float>(i);
            const Vector3 spawnPos{5.0f, 0.0f, z};

            std::vector<MoveEntry> extra;
            extra.reserve(1);
            MoveEntry e;
            e.from = Vector3{5.0f, 1.0f, spawnPos.z};
            e.to = Vector3{-5.0f, 1.0f, spawnPos.z};
            e.duration = 2.0f;
            e.easing = [](Vector3 a, Vector3 b, float t) { return EaseInOutBack(a, b, t); };
            extra.push_back(std::move(e));

            SpawnXZPlaneWithMoves(
                std::string("AttackGearWallRightSide_") + std::to_string(i),
                spawnPos,
                extra);
        }
    }
};

} // namespace KashipanEngine