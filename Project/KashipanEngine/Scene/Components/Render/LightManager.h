#pragma once
#pragma once

#include <memory>

#include "Scene/Components/SceneComponentHeader.h"
#include "Objects/SystemObjects/LightCountBinder.h"
#include "Objects/SystemObjects/PointLight.h"
#include "Objects/SystemObjects/SpotLight.h"

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
        if (!context->AddObject(std::move(light))) return nullptr;

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
        if (!context->AddObject(std::move(light))) return nullptr;

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
        return context->RemoveObject(light);
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
        return context->RemoveObject(light);
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

        if (auto *obj = context->GetObject("LightCountBinder")) {
            return dynamic_cast<LightCountBinder *>(obj);
        }

        return nullptr;
    }

    LightCountBinder *lightCountBinder_ = nullptr;
};

REGISTER_COMPONENT_SCENE(LightManager)

} // namespace KashipanEngine
