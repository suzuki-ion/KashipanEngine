#pragma once
#include <cstdint>
#include "Assets/SamplerManager.h"
#include "Objects/Object3DBase.h"

namespace KashipanEngine {

class ShadowMapBuffer;

class ShadowMapBinder final : public Object3DBase {
public:
    ShadowMapBinder();
    ~ShadowMapBinder() override = default;

    void SetShadowMapBuffer(ShadowMapBuffer* buffer) { shadowMapBuffer_ = buffer; }
    ShadowMapBuffer* GetShadowMapBuffer() const { return shadowMapBuffer_; }

    void SetShadowSampler(SamplerManager::SamplerHandle handle) { shadowSampler_ = handle; }
    SamplerManager::SamplerHandle GetShadowSampler() const { return shadowSampler_; }

    void SetLightViewProjectionMatrix(const Matrix4x4 &matrix) { lightViewProjectionMatrix_ = matrix; }
    const Matrix4x4 &GetLightViewProjectionMatrix() const { return lightViewProjectionMatrix_; }

    void SetShadowMapNameKey(std::string nameKey) { shadowMapNameKey_ = std::move(nameKey); }
    void SetShadowSamplerNameKey(std::string nameKey) { shadowSamplerNameKey_ = std::move(nameKey); }

protected:
    bool Render(ShaderVariableBinder& shaderBinder) override;

private:
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;
    SamplerManager::SamplerHandle shadowSampler_ = SamplerManager::kInvalidHandle;
    Matrix4x4 lightViewProjectionMatrix_ = Matrix4x4::Identity();

    std::string lightViewProjectionMatrixNameKey_ = "Pixel:ShadowMapConstants";
    std::string shadowMapNameKey_ = "Pixel:gShadowMap";
    std::string shadowSamplerNameKey_ = "Pixel:gShadowSampler";
};

} // namespace KashipanEngine
