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
        { lightViewProjectionMatrixNameKey_, sizeof(Matrix4x4) }
    });
    SetUpdateConstantBuffersFunction(
        [this](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
            if (!constantBufferMaps) return false;
            auto **maps = static_cast<void **>(constantBufferMaps);
            std::memcpy(maps[0], &lightViewProjectionMatrix_, sizeof(Matrix4x4));
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
    if (shadowSampler_ != SamplerManager::kInvalidHandle) {
        SamplerManager::BindSampler(&shaderBinder, shadowSamplerNameKey_, shadowSampler_);
    }

    return true;
}

} // namespace KashipanEngine
