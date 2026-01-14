#pragma once

#include "Objects/Object3DBase.h"
#include "Objects/GameObjects/3D/VertexData3D.h"

namespace KashipanEngine {

class Camera3D;

class Billboard : public Object3DBase {
public:
    enum class FacingMode {
        MatchCameraRotation,
        LookAtCamera,
    };

    Billboard();
    ~Billboard() override = default;

    void SetCamera(Camera3D *camera) { camera_ = camera; }
    Camera3D *GetCamera() const { return camera_; }

    void SetFacingMode(FacingMode mode) { facingMode_ = mode; }
    FacingMode GetFacingMode() const { return facingMode_; }

    void SetAutoRotateX(bool enabled) { isAutoRotateX_ = enabled; }
    void SetAutoRotateY(bool enabled) { isAutoRotateY_ = enabled; }
    void SetAutoRotateZ(bool enabled) { isAutoRotateZ_ = enabled; }

    bool GetAutoRotateX() const { return isAutoRotateX_; }
    bool GetAutoRotateY() const { return isAutoRotateY_; }
    bool GetAutoRotateZ() const { return isAutoRotateZ_; }

protected:
    void OnUpdate() override;

    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

    void ApplyBillboardRotation();

private:
    using Vertex = VertexData3D;
    using Index = std::uint32_t;

    Camera3D *camera_ = nullptr;
    FacingMode facingMode_ = FacingMode::MatchCameraRotation;
    bool isAutoRotateX_ = true;
    bool isAutoRotateY_ = true;
    bool isAutoRotateZ_ = true;
};

} // namespace KashipanEngine
