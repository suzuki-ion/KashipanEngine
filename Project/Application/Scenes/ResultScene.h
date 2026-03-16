#pragma once
#include <KashipanEngine.h>
#include <Objects/ResultSystem/ResultSelector.h>
#include "Scenes/Components/ResultSceneAnimator.h"
#include "Assets/AudioPlayer.h"

namespace KashipanEngine {

    class ResultScene final : public SceneBase {
    public:
        ResultScene();
        ~ResultScene() override;

        void Initialize() override;

    protected:
        void OnUpdate() override;

    private:
        SceneDefaultVariables* sceneDefaultVariables_ = nullptr;

        // * ユーティリティ * //
        // ゲームにスプライトを生成、追加する関数
        std::function<KashipanEngine::Sprite* (const std::string&)> createSpriteFunction_;
        // ゲームにスプライトを特定のテクスチャで生成、追加する関数
        std::function<KashipanEngine::Sprite* (const std::string&, const std::string&)> createSpriteWithTextureFunction_;
        // 画面の中心
        Vector2 screenCenter_;
        // タイマー
		float timer_ = 0.0f;

		// * ゲームで使うオブジェクトたち * //
		// 結果セレクタの管理クラス
        Application::ResultSelector resultSelector_;
		// スプライトの管理用マップ
        std::map<std::string, Sprite*> spriteMap_;
		// BGMプレイヤー
        AudioPlayer bgmPlayer_;
        int prevSelectedNumber_ = -1;
		// 結果の画像が表示されたら選択できるようにするためのフラグ
		bool isReadyToSelect_ = false;
		// 勝利したプレイヤーの番号（0または1）を管理する変数（-1は未設定）
		int winnerPlayerNumber_ = -1; // 勝利したプレイヤーの番号（0または1）
		bool isNpcMode_ = false; // NPCモードかどうか
    };

}
