#pragma once
#pragma once

#include <memory>

#include "Objects/SystemObjects/LightCountBinder.h"
#include "Objects/SystemObjects/PointLight.h"
#include "Objects/SystemObjects/SpotLight.h"
#include "Scene/Components/ISceneComponent.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

class LightManager final : public ISceneComponent {
public:
    LightManager()
        : ISceneComponent("LightManager", 1) {
    }
    ~LightManager() override = default;

    void SetLightCountBinder(LightCountBinder *binder) { lightCountBinder_ = binder; }
    LightCountBinder *GetLightCountBinder() const { return lightCountBinder_; }

    PointLight *CreatePointLight() {
        auto *context = GetOwnerContext();
        if (!context) return nullptr;

        if (!lightCountBinder_) {
            lightCountBinder_ = TryGetBinderFromScene();
        }

        auto light = std::make_unique<PointLight>();
        auto *lightPtr = light.get();
        if (!context->AddObject3D(std::move(light))) return nullptr;

        if (lightCountBinder_) {
            lightCountBinder_->AddPointLight(lightPtr);
        }
        return lightPtr;
    }

    SpotLight *CreateSpotLight() {
        auto *context = GetOwnerContext();
        if (!context) return nullptr;

        if (!lightCountBinder_) {
            lightCountBinder_ = TryGetBinderFromScene();
        }

        auto light = std::make_unique<SpotLight>();
        auto *lightPtr = light.get();
        if (!context->AddObject3D(std::move(light))) return nullptr;

        if (lightCountBinder_) {
            lightCountBinder_->AddSpotLight(lightPtr);
        }
        return lightPtr;
    }

    bool RemovePointLight(PointLight *light) {
        if (!light) return false;

        if (!lightCountBinder_) {
            lightCountBinder_ = TryGetBinderFromScene();
        }
        if (lightCountBinder_) {
            lightCountBinder_->RemovePointLight(light);
        }

        auto *context = GetOwnerContext();
        if (!context) return false;
        return context->RemoveObject3D(light);
    }

    bool RemoveSpotLight(SpotLight *light) {
        if (!light) return false;

        if (!lightCountBinder_) {
            lightCountBinder_ = TryGetBinderFromScene();
        }
        if (lightCountBinder_) {
            lightCountBinder_->RemoveSpotLight(light);
        }

        auto *context = GetOwnerContext();
        if (!context) return false;
        return context->RemoveObject3D(light);
    }

    void Initialize() override {
        if (!lightCountBinder_) {
            lightCountBinder_ = TryGetBinderFromScene();
        }
    }

    void Update() override {}

private:
    LightCountBinder *TryGetBinderFromScene() const {
        auto *context = GetOwnerContext();
        if (!context) return nullptr;

        if (auto *obj = context->GetObject3D("LightCountBinder")) {
            return dynamic_cast<LightCountBinder *>(obj);
        }

        return nullptr;
    }

    LightCountBinder *lightCountBinder_ = nullptr;
};

} // namespace KashipanEngine
