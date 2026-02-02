#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class TutorialConnect final : public ISceneComponent {
public:
    TutorialConnect();
    ~TutorialConnect() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

private:
    // Intentionally left blank: placeholder component
};

} // namespace KashipanEngine
