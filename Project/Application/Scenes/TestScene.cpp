#include "Scenes/TestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <algorithm>

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D();

    {
        auto obj = std::make_unique<Sprite>();
        obj->SetName("TestSprite");
        if (auto *tr = obj->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2(400.0f, 300.0f));
            tr->SetScale(Vector2(200.0f, 200.0f));
        }
        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4(1.0f, 0.5f, 0.5f, 1.0f));
        }
        obj->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        AddObject2D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    audioPlayerTestSounds_.clear();
    audioPlayerTestTimer_ = 0.0f;
    audioPlayerTestActive_ = false;

    const std::vector<std::string> soundNames = {
        "test1.mp3",
        "test2.mp3"
    };

    for (const auto& name : soundNames) {
        const auto handle = AudioManager::GetSoundHandleFromFileName(name);
        if (handle != AudioManager::kInvalidSoundHandle) {
            AudioManager::PlayParams params;
            params.sound = handle;
            params.volume = 1.0f;
            params.pitch = 0.0f;
            params.loop = true;
            params.startTimeSec = 10.0f;
            params.endTimeSec = 0.0f;
            audioPlayerTestSounds_.push_back(params);
        }
    }

    if (!audioPlayerTestSounds_.empty()) {
        audioPlayer_.AddAudios(audioPlayerTestSounds_);
        audioPlayer_.ChangeAudio(0.0, 0);
        audioPlayerTestActive_ = true;
    }
}

TestScene::~TestScene() {
}

void TestScene::OnUpdate() {
    if (auto *ic = GetInputCommand()) {
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("MenuScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
    }

    if (!GetNextSceneName().empty()) {
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

    if (audioPlayerTestActive_ && audioPlayerTestSounds_.size() > 1) {
        const float dt = std::max(0.0f, GetDeltaTime());
        audioPlayerTestTimer_ += dt;
        if (audioPlayerTestTimer_ >= 10.0f) {
            audioPlayerTestTimer_ = 0.0f;
            audioPlayer_.ChangeAudio(3.0);
        }
    }
}

} // namespace KashipanEngine