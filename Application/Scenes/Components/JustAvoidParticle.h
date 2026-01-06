#pragma once

#include "Scene/Components/ISceneComponent.h"

#include "Graphics/ScreenBuffer.h"
#include "Math/Vector3.h"

#include <array>

namespace KashipanEngine {

class Camera3D;
class Object3DBase;
class Billboard;

class JustAvoidParticle final : public ISceneComponent {
public:
    JustAvoidParticle(ScreenBuffer* screenBuffer, Camera3D* camera3D, Object3DBase* mover)
        : ISceneComponent("JustAvoidParticle", 1)
        , screenBuffer_(screenBuffer)
        , camera3D_(camera3D)
        , mover_(mover) {}

    ~JustAvoidParticle() override = default;

    void Initialize() override;
    void Update() override;

    void Spawn(const Vector3& pos, const Vector3& dir);

private:
    ScreenBuffer* screenBuffer_ = nullptr;
    Camera3D* camera3D_ = nullptr;
    Object3DBase* mover_ = nullptr;

    std::array<Billboard*, 16> billboards_{};
    Billboard* textBillboard_ = nullptr;
};

} // namespace KashipanEngine
