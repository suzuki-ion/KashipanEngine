#pragma once
#include "Objects/EmptyObject.h"
#include "Objects/SystemObjects/Camera3D.h"

namespace KashipanEngine {

class VelocityBufferCameraBinder final : public EmptyObject {
public:
    struct VelocityCameraConstants {
        Matrix4x4 viewProjection;
        Matrix4x4 prevViewProjection;
    };

    VelocityBufferCameraBinder();
    ~VelocityBufferCameraBinder() override = default;

    void SetCamera3D(Camera3D* camera) {
        camera3D_ = camera;
        prevViewProjectionMatrix_ = camera ? camera->GetViewProjectionMatrix() : Matrix4x4::Identity();
    }

protected:
    bool Render(ShaderVariableBinder& shaderBinder) override;

private:
    Camera3D* camera3D_ = nullptr;
    VelocityCameraConstants constants_{};
    Matrix4x4 prevViewProjectionMatrix_ = Matrix4x4::Identity();

    std::string constantsNameKey_ = "Vertex:gObjectVelocityCamera";
};

} // namespace KashipanEngine
