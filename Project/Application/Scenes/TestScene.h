#pragma once
#include <KashipanEngine.h>

#include <vector>

namespace KashipanEngine {

class TestScene final : public SceneBase {
public:
    TestScene();
    ~TestScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
    AudioPlayer audioPlayer_{};
    std::vector<AudioManager::SoundHandle> audioPlayerTestSounds_{};
    float audioPlayerTestTimer_ = 0.0f;
    bool audioPlayerTestActive_ = false;
};

} // namespace KashipanEngine