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
    BreakParticleGenerator(ScreenBuffer *screenBuffer, Object3DBase *mover)
        : ISceneComponent("BreakParticleGenerator", 1), screenBuffer_(screenBuffer), mover_(mover) {}

    ~BreakParticleGenerator() override = default;

    void SetScreenBuffer(ScreenBuffer *sb) { screenBuffer_ = sb; }
    void SetMover(Object3DBase *m) { mover_ = m; }

    void Generate(const Vector3 &pos);

    void Update() override;

private:
    ScreenBuffer *screenBuffer_ = nullptr;
    Object3DBase *mover_ = nullptr;
    std::uint64_t serial_ = 0;

    std::vector<Object3DBase *> particles_;
};

} // namespace KashipanEngine
