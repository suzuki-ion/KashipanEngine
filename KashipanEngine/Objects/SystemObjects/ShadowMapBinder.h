#pragma once
#include <cstdint>
#include "Assets/SamplerManager.h"
#include "Objects/Object3DBase.h"
#include "Objects/SystemObjects/Camera3D.h"

namespace KashipanEngine {

class ShadowMapBuffer;

class ShadowMapBinder final : public Object3DBase {
public:
    struct ShadowMapConstants {
        Matrix4x4 lightViewProjectionMatrix;
        float lightNear;
        float lightFar;
    };

    ShadowMapBinder();
    ~ShadowMapBinder() override = default;

    void SetShadowMapBuffer(ShadowMapBuffer* buffer) { shadowMapBuffer_ = buffer; }
    ShadowMapBuffer* GetShadowMapBuffer() const { return shadowMapBuffer_; }

    void SetShadowSampler(SamplerManager::SamplerHandle handle) { shadowSampler_ = handle; }
    SamplerManager::SamplerHandle GetShadowSampler() const { return shadowSampler_; }

    void SetCamera3D(Camera3D *camera) { camera3D_ = camera; }

    void SetShadowMapNameKey(std::string nameKey) { shadowMapNameKey_ = std::move(nameKey); }
    void SetShadowSamplerNameKey(std::string nameKey) { shadowSamplerNameKey_ = std::move(nameKey); }

protected:
    bool Render(ShaderVariableBinder& shaderBinder) override;

private:
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;
    SamplerManager::SamplerHandle shadowSampler_ = SamplerManager::kInvalidHandle;
    Camera3D *camera3D_ = nullptr;

    ShadowMapBinder::ShadowMapConstants shadowMapConstants_{};

    std::string shadowMapConstantsNameKey_ = "Pixel:ShadowMapConstants";
    std::string shadowMapNameKey_ = "Pixel:gShadowMap";
    std::string shadowSamplerNameKey_ = "Pixel:gShadowSampler";
};

} // namespace KashipanEngine
