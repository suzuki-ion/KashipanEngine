#pragma once

#include "AttackBase.h"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

namespace KashipanEngine {

/**
 * @brief フォワードギアの壁攻撃クラス
 *
 * このクラスは、フォワードギアを使用した壁攻撃を表現します。
 */
class AttackGearWallForward final : public AttackBase {
public:
    /**
     * @brief コンストラクタ
     *
     * @param mover オプションのMoverオブジェクト
     * @param screenBuffer オプションのScreenBufferオブジェクト
     */
    explicit AttackGearWallForward(Object3DBase *mover = nullptr, ScreenBuffer *screenBuffer = nullptr)
        : AttackBase("AttackGearWallForward", mover, screenBuffer) {}
    ~AttackGearWallForward() override = default;

protected:
    void AttackStartInitialize() override {
        // -5～5 を 10 分割（=11点）想定だと「10個等間隔」とズレるので、ここでは 10個を等間隔に並べる
        constexpr int kCount = 10;
        constexpr float minX = -5.0f;
        constexpr float maxX = 5.0f;
        const float step = (maxX - minX) / static_cast<float>(kCount - 1);

        std::vector<int> indices;
        indices.reserve(kCount);
        for (int i = 0; i < kCount; ++i) indices.push_back(i);

        // 1～2個を削るイメージ（固定シードで再現性）
        std::mt19937 rng{12345u};
        std::shuffle(indices.begin(), indices.end(), rng);
        const int removeCount = 1 + (rng() % 2);
        indices.resize(static_cast<size_t>(kCount - removeCount));
        std::sort(indices.begin(), indices.end());

        for (int i : indices) {
            const float x = minX + step * static_cast<float>(i);
            const Vector3 spawnPos{x, 0.0f, 5.0f};

            std::vector<MoveEntry> extra;
            extra.reserve(1);
            MoveEntry e;
            e.from = Vector3{spawnPos.x, 1.0f, 5.0f};
            e.to = Vector3{spawnPos.x, 1.0f, -5.0f};
            e.duration = 2.0f;
            e.easing = [](Vector3 a, Vector3 b, float t) { return EaseInOutBack(a, b, t); };
            extra.push_back(std::move(e));

            SpawnXZPlaneWithMoves(
                std::string("AttackGearWallForward_") + std::to_string(i),
                spawnPos,
                extra);
        }
    }
};

} // namespace KashipanEngine