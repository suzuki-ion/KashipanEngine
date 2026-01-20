#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class TitleScene final : public SceneBase {
public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
};

} // namespace KashipanEngine