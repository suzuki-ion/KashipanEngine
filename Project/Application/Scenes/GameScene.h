#pragma once
#include <KashipanEngine.h>

#include <Objects/Container/BlockContainer.h>
#include <Objects/Container/BlockSpriteContainer.h>

#include <Objects/GameSystem/BlockScroller.h>
#include <Objects/GameSystem/BlockFaller.h>
#include <Objects/GameSystem/Thermometer.h>
#include <Objects/GameSystem/MatchResolver.h>
#include <Objects/GameSystem/Cursor.h>

namespace KashipanEngine {

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    // ゲームで使うデータのまとまりクラス
	Application::BlockContainer blockContainer_;

	// ゲームシステムのまとまりクラス
	Application::BlockScroller blockScroller_;
	Application::BlockFaller blockFaller_;
	Application::Thermometer thermometer_;
	Application::MatchResolver matchResolver_;
	Application::Cursor cursor_;
	Sprite* cursorSprite_ = nullptr;

	// 描画用のオブジェクトのまとまりクラス
    Vector2 blockSpriteBasePos_;
	Application::BlockSpriteContainer blockSpriteContainer_;

    // 関数
	void UpdateBlockColor();
};

} // namespace KashipanEngine