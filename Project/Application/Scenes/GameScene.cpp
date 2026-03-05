#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

	// メニューで使う入力を登録
    menuActionManager_.Initialize(
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Menu").Triggered(); },
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Submit").Triggered(); },
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Cancel").Triggered(); },
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Up").Triggered(); },
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Down").Triggered(); }
    );
	// メニューのスプライトを初期化
	menuSpriteContainer_.Initialize();
	menuPosition_ = Vector2(400.0f, 300.0f);
	menuSpriteContainer_.SetPosition(menuPosition_);
	std::vector<Sprite*> menuSprites;
    for(int i = 0; i < 3; ++i) {
        auto obj = std::make_unique<Sprite>();
        obj->SetName("MenuSprite" + std::to_string(i));
        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            //mat->SetTexture(TextureManager::GetTextureFromFileName("MenuIcon.png"));
        }
		menuSprites.push_back(obj.get());
        obj->AttachToRenderer(sceneDefaultVariables_->GetMainWindow(), "Object2D.DoubleSidedCulling.BlendNormal");
        AddObject2D(std::move(obj));
	}
	menuSpriteContainer_.SetMenuSprite(menuSprites);
}

GameScene::~GameScene() {}

void GameScene::OnUpdate() {
    menuSpriteContainer_.SetPosition(menuPosition_);

	menuActionManager_.Update();
	menuSpriteContainer_.SetSelectedIndex(menuActionManager_.GetSelectedIndex());
	menuSpriteContainer_.SetMenuOpen(menuActionManager_.IsMenuOpen()) ;
	menuSpriteContainer_.Update(KashipanEngine::GetDeltaTime());

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
}

} // namespace KashipanEngine
