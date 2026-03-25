#pragma once
#include <KashipanEngine.h>
#include <Objects/TitleSystem/TitleSelectManager.h>
#include <Objects/TitleSystem/TitleSpriteManager.h>

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

	// * ユーティリティ * //
	// ゲームにスプライトを生成、追加する関数
	std::function<KashipanEngine::Sprite* (const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunction_;
	// ゲームにスプライトを特定のテクスチャで生成、追加する関数
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteWithTextureFunction_;
	// 画面の中心
	Vector2 screenCenter_;

	// * タイトル画面のオブジェクトたち * //
	// タイトル画面のセクション管理(データのみ)
	Application::TitleSelectManager titleSelectManager_;
	// タイトル画面のスプライト管理
	Application::TitleSpriteManager titleSpriteManager_;

	bool transitionStarted_ = false;

	// 音声プレイヤー
	AudioPlayer audioPlayer_;
};

} // namespace KashipanEngine