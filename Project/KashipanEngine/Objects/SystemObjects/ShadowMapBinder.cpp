#include "Objects/SystemObjects/ShadowMapBinder.h"

#include "Graphics/Renderer.h"
#include "Graphics/ShadowMapBuffer.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"

namespace KashipanEngine {

ShadowMapBinder::ShadowMapBinder()
    : Object3DBase("ShadowMapBinder") {
    SetRenderType(RenderType::Standard);
    SetConstantBufferRequirements({
        { shadowMapConstantsNameKey_, sizeof(ShadowMapConstants) },
    });
    SetUpdateConstantBuffersFunction(
        [this](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
            if (!constantBufferMaps) return false;
            auto **maps = static_cast<void **>(constantBufferMaps);
            shadowMapConstants_.lightViewProjectionMatrix = Matrix4x4::Identity();
            if (camera3D_) {
                shadowMapConstants_.lightViewProjectionMatrix = camera3D_->GetViewProjectionMatrix();
                shadowMapConstants_.lightNear = camera3D_->GetNearClip();
                shadowMapConstants_.lightFar = camera3D_->GetFarClip();
            }
            memcpy(maps[0], &shadowMapConstants_, sizeof(ShadowMapConstants));
            return true;
        });
}

bool ShadowMapBinder::Render(ShaderVariableBinder& shaderBinder) {
    if (!shadowMapBuffer_) return false;

    // シャドウマップテクスチャをバインド
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = shadowMapBuffer_->GetSrvHandle();
    if (srvHandle.ptr != 0) {
        shaderBinder.Bind(shadowMapNameKey_, srvHandle);
    }
    SamplerManager::BindSampler(&shaderBinder, shadowSamplerNameKey_, DefaultSampler::LinearClamp);

    return true;
}

} // namespace KashipanEngine
