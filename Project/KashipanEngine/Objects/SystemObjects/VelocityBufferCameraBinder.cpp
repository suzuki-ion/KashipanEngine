#include "Objects/SystemObjects/VelocityBufferCameraBinder.h"

#include "Graphics/Renderer.h"
#include <cstring>

namespace KashipanEngine {

VelocityBufferCameraBinder::VelocityBufferCameraBinder()
    : Object3DBase("VelocityBufferCameraBinder") {
    SetRenderType(RenderType::Standard);
    SetConstantBufferRequirements({{ constantsNameKey_, sizeof(VelocityCameraConstants) }}); 
    SetUpdateConstantBuffersFunction(
        [this](void* constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
            if (!constantBufferMaps) return false;
            auto** maps = static_cast<void**>(constantBufferMaps);

            constants_.prevViewProjection = prevViewProjectionMatrix_;
            constants_.viewProjection = Matrix4x4::Identity();
            if (camera3D_) {
                constants_.viewProjection = camera3D_->GetViewProjectionMatrix();
            }

            prevViewProjectionMatrix_ = constants_.viewProjection;

            std::memcpy(maps[0], &constants_, sizeof(VelocityCameraConstants));
            return true;
        });
}

bool VelocityBufferCameraBinder::Render(ShaderVariableBinder& shaderBinder) {
    (void)shaderBinder;
    return true;
}

} // namespace KashipanEngine
