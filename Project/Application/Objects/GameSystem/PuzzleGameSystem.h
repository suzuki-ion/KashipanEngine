#pragma once
#include <KashipanEngine.h>

#include "PuzzlePlayer.h"

#include "Board.h"
#include "Cursor.h"
#include "MatchFinder.h"

#include "BoardSprite.h"
#include "CursorSprite.h"
#include "GaugeSprite.h"
#include "AttackSprite.h"

namespace Application {
	// パズルゲームのシステムを管理するクラス
	class PuzzleGameSystem {
	public:
		/// @brief ゲームを初期化する
		void Initialize(
			std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc,
			PuzzlePlayer* puzzlePalyer);
		/// @brief ゲームの状態を更新する
		void Update();

		/// @brief ゲームの状態を更新する
		void SystemUpdate();
		/// @brief ゲームの状態を描画する
		void VisualUpdate();
		/// @brief アンカースプライトの位置を設定する関数
		void SetAnchorSpritePosition(const Vector3& position);
		/// @brief アンカースプライトの回転を設定する関数
		void SetAnchorSpriteRotation(const Vector3& rotation);

		void DeathAnimation();

		int SendDamage();
		void TakeDamage(int damage);

		void Setup2P();
		void SetupNpc();

	private:
		// * ゲームのシステム * //
		// パズルの操作を管理するプレイヤーオブジェクト
		PuzzlePlayer* player_;
		
		// 盤面のデータを管理するオブジェクト
		Board board_;
		// カーソルの操作、状態を管理するオブジェクト
		Cursor cursor_;
		// 盤面内の同じ色が3つ以上並んでいる場所を見つけるオブジェクト
		MatchFinder matchFinder_;

		// * ゲームのビジュアル * //
		// 盤面をスプライトで描画するオブジェクト
		BoardSprite boardSprite_;
		// カーソルをスプライトで描画するオブジェクト
		CursorSprite cursorSprite_;
		// HPゲージをスプライトで描画するオブジェクト
		GaugeSprite hpGaugeSprite_;
		KashipanEngine::Sprite* hpGaugeFrameSprite_ = nullptr;
		// 攻撃演出をスプライトで描画するオブジェクト
		AttackSprite attackSprite_;
		// 勝敗がついた後の演出用スプライト
		KashipanEngine::Sprite* resultSprite_ = nullptr;

		float test = 1.0f;
		float test2 = 1.0f;

		float attackTimer_ = 0.0f;
		float attackCooldown_ = 1.0f;

		Vector3 anchorPosition_ = Vector3(0.0f, 0.0f, 0.0f);
		float deathAnimationTimer_ = 0.0f;
	};
}