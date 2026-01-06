#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/ObjectContext.h"

#include "Math/Vector3.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Utilities/TimeUtils.h"

#include <algorithm>
#include <memory>

namespace KashipanEngine {

class AlwaysRotate final : public IObjectComponent3D {
public:
    explicit AlwaysRotate(const Vector3 &rotateSpeedRadPerSec = Vector3{0.0f, 1.0f, 0.0f})
        : IObjectComponent3D("AlwaysRotate", 1), rotateSpeed_(rotateSpeedRadPerSec) {
    }

    ~AlwaysRotate() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<AlwaysRotate>(rotateSpeed_);
    }

    std::optional<bool> Update() override {
        Transform3D *tr = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!tr) return false;

        const float dt = std::max(0.0f, GetDeltaTime());
        Vector3 r = tr->GetRotate();
        r += rotateSpeed_ * dt;
        tr->SetRotate(r);
        return true;
    }

    void SetRotateSpeed(const Vector3 &v) { rotateSpeed_ = v; }
    const Vector3 &GetRotateSpeed() const { return rotateSpeed_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Vector3 rotateSpeed_{0.0f, 1.0f, 0.0f};
};

} // namespace KashipanEngine
