#include "Objects/SystemObjects/ShadowMapBinder.h"

#include "Graphics/Renderer.h"
#include "Graphics/ShadowMapBuffer.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"

namespace KashipanEngine {

namespace {
// シャドウマップ比較用サンプラー
SamplerManager::SamplerHandle sShadowMapSamplerCmpHandle = SamplerManager::kInvalidHandle;
class SamplerInitializer {
public:
    SamplerInitializer() {
        D3D12_SAMPLER_DESC shadowMapSamplerDesc{};
        shadowMapSamplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        shadowMapSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        shadowMapSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        shadowMapSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        shadowMapSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        shadowMapSamplerDesc.MinLOD = 0.0f;
        shadowMapSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        shadowMapSamplerDesc.MipLODBias = 0.0f;
        shadowMapSamplerDesc.MaxAnisotropy = 1;
        sShadowMapSamplerCmpHandle = SamplerManager::CreateSampler(shadowMapSamplerDesc);
    }
};
} // namespace

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
    static SamplerInitializer samplerInitializer;

    if (!shadowMapBuffer_) return false;

    // シャドウマップテクスチャをバインド
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = shadowMapBuffer_->GetSrvHandle();
    if (srvHandle.ptr != 0) {
        shaderBinder.Bind(shadowMapNameKey_, srvHandle);
    }
    SamplerManager::BindSampler(&shaderBinder, shadowSamplerCmpNameKey_, sShadowMapSamplerCmpHandle);

    return true;
}

} // namespace KashipanEngine
