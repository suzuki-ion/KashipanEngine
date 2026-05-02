#pragma once

#include <KashipanEngine.h>

namespace KashipanEngine {

class IPlayerMovementControllerAccess {
public:
    virtual ~IPlayerMovementControllerAccess() = default;

    virtual Vector3 GetGravityDirectionValue() const = 0;
    virtual Vector3 GetForwardDirectionValue() const = 0;
    virtual void SetGravityDirection(const Vector3 &direction) = 0;

    virtual Vector3 &GravityVelocityRef() = 0;
    virtual Vector3 &LateralVelocityRef() = 0;
    virtual float &ForwardSpeedRef() = 0;

    virtual float GetMinForwardSpeed() const = 0;
    virtual float GetMaxForwardSpeed() const = 0;
    virtual float GetForwardAcceleration() const = 0;
    virtual float GetForwardAccelPerFallSpeed() const = 0;
    virtual float GetGroundDeceleration() const = 0;

    virtual float GetLateralMaxSpeed() const = 0;
    virtual float GetLateralAcceleration() const = 0;
    virtual float GetLateralSpeedPerForward() const = 0;
    virtual float GetLateralInput() const = 0;
    virtual void ClearLateralInput() = 0;

    virtual float GetJumpPower() const = 0;

    virtual void MarkGrounded() = 0;
    virtual bool ConsumeGrounded() = 0;
};

} // namespace KashipanEngine
