#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class TutorialMove final : public ISceneComponent {
public:
    TutorialMove();
    ~TutorialMove() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

private:
    // Intentionally left blank: placeholder component
};

} // namespace KashipanEngine
