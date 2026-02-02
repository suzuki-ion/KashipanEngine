#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class TutorialBomb final : public ISceneComponent {
public:
    TutorialBomb();
    ~TutorialBomb() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

private:
    // Intentionally left blank: placeholder component
};

} // namespace KashipanEngine
