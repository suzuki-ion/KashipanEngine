#pragma once

#include "Scene/Components/ISceneComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Objects/Object3DBase.h"
#include "Objects/GameObjects/3D/Plane3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace KashipanEngine {

class BreakParticleGenerator final : public ISceneComponent {
public:
    /// @brief スクリーンバッファと親オブジェクトを指定して作成する
    BreakParticleGenerator(ScreenBuffer *screenBuffer, Object3DBase *mover)
        : ISceneComponent("BreakParticleGenerator", 1), screenBuffer_(screenBuffer), mover_(mover) {}

    /// @brief デストラクタ
    ~BreakParticleGenerator() override = default;

    /// @brief 使用するスクリーンバッファを設定する
    /// @param sb スクリーンバッファポインタ
    void SetScreenBuffer(ScreenBuffer *sb) { screenBuffer_ = sb; }
    /// @brief 使用する親オブジェクトを設定する
    /// @param m 親オブジェクト
    void SetMover(Object3DBase *m) { mover_ = m; }

    /// @brief 指定位置で破片パーティクルを生成する
    /// @param pos 生成位置（ワールド座標）
    void Generate(const Vector3 &pos);

    /// @brief 毎フレーム更新処理
    void Update() override;

private:
    ScreenBuffer *screenBuffer_ = nullptr;
    Object3DBase *mover_ = nullptr;
    std::uint64_t serial_ = 0;

    std::vector<Object3DBase *> particles_;
};

} // namespace KashipanEngine
