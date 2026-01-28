#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class BackMonitor;

class BackMonitorRenderer : public ISceneComponent {
public:
    BackMonitorRenderer(const char* name, ScreenBuffer* target)
        : ISceneComponent(name, 1), target_(target), isActive_(false) {}

    virtual ~BackMonitorRenderer() override = default;

    void Initialize() override {}
    void Finalize() override {}
    void Update() override {}

    BackMonitor* GetBackMonitor();

    ScreenBuffer* GetTargetScreenBuffer();

    void SetActive(bool v) { isActive_ = v; }
    bool IsActive() const { return isActive_; }

protected:
    ScreenBuffer* target_ = nullptr;
    BackMonitor* backMonitor_ = nullptr;

private:
    bool isActive_ = true;
};

} // namespace KashipanEngine
