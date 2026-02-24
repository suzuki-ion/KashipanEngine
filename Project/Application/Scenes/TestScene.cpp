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

    // 2D用オフスクリーンバッファを取得。描画先として使用するため、以降のオブジェクト生成時に必要になる。
    auto *screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D();

    //--------- テスト用スプライトオブジェクト ---------//
    {
        auto obj = std::make_unique<Sprite>();
    
        // オブジェクト名は任意だが、デバッグ表示などで使用されるため、わかりやすい名前をつけることが望ましい
        obj->SetName("TestSprite");
        
        // GetComponent2D<T>() でコンポーネントを取得して、必要な初期化を行う（Transform2D や Material2D など）
        if (auto *tr = obj->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2(400.0f, 300.0f));
            tr->SetScale(Vector2(200.0f, 200.0f));
        }
        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4(1.0f, 0.5f, 0.5f, 1.0f));
        }

        // オブジェクト生成時は以下のように、必ず最後に AttachToRenderer を呼び出してからシーンに追加すること
        // AttachToRenderer の引数は、描画先と使用するパイプライン名を指定する
        obj->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        // シーンにオブジェクトを追加。これを行わないと更新処理がされないため注意。
        // オブジェクトの所有権はシーンが持つため、std::move して渡すこと。
        AddObject2D(std::move(obj));
    }

    //--------- シーン遷移用アニメーションのシーンコンポーネント ---------//

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    //--------- 音声プレイヤー ---------//

    audioPlayerTestSounds_.clear();
    audioPlayerTestTimer_ = 0.0f;
    audioPlayerTestActive_ = false;

    const std::vector<std::string> soundNames = {
        "test1.mp3",
        "test2.mp3"
    };

    for (const auto& name : soundNames) {
        // 指定のファイルがサウンドマネージャーに存在するか確認して、存在する場合は再生パラメータを作成してリストに追加する
        const auto handle = AudioManager::GetSoundHandleFromFileName(name);
        if (handle != AudioManager::kInvalidSoundHandle) {
            // 以下は再生パラメータの例。必要に応じて変更すること。
            AudioManager::PlayParams params;
            // サウンドハンドルは必須。その他のパラメータはオプションで、デフォルト値がある。
            params.sound = handle;
            // 音量
            params.volume = 1.0f;
            // ピッチ（半音単位。+1.0f で半音上がる。注意：ピッチを変更すると同時に再生速度も変わるため、長さが変わる点に注意）
            params.pitch = 0.0f;
            // ループ再生の有無
            params.loop = true;
            // 再生開始時間（秒）
            params.startTimeSec = 10.0f;
            // 再生終了時間（秒。0以下を指定すると末尾まで再生する）
            params.endTimeSec = 0.0f;
            audioPlayerTestSounds_.push_back(params);
        }
    }

    if (!audioPlayerTestSounds_.empty()) {
        // 追加した音声のリストを音声プレイヤーに渡して、最初の音声を再生する
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