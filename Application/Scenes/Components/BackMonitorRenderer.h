#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

// Base class for components that render into BackMonitor's ScreenBuffer
class BackMonitorRenderer : public ISceneComponent {
public:
    BackMonitorRenderer(const char* name, ScreenBuffer* target)
        : ISceneComponent(name, 1), target_(target) {}

    virtual ~BackMonitorRenderer() override = default;

    void Initialize() override {}
    void Finalize() override {}
    void Update() override {
        // Default does nothing. Derived classes should override.
    }

protected:
    ScreenBuffer* target_ = nullptr;
};

} // namespace KashipanEngine
