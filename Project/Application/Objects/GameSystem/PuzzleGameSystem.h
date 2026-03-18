#pragma once
#include <KashipanEngine.h>

#include "Board.h"
#include "Cursor.h"

#include "BoardSprite.h"
#include "CursorSprite.h"

namespace Application {
	// パズルゲームのシステムを管理するクラス
	class PuzzleGameSystem {
	public:
		/// @brief ゲームを初期化する
		void Initialize(
			std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc,
			KashipanEngine::InputCommand* ic);
		/// @brief ゲームの状態を更新する
		void Update();

		/// @brief ゲームの状態を更新する
		void SystemUpdate();
		/// @brief ゲームの状態を描画する
		void VisualUpdate();

	private:
		// * ゲームのシステム * //
		// 盤面のデータを管理するオブジェクト
		Board board_;
		// カーソルの操作、状態を管理するオブジェクト
		Cursor cursor_;

		// * ゲームのビジュアル * //
		// 盤面をスプライトで描画するオブジェクト
		BoardSprite boardSprite_;
		// カーソルをスプライトで描画するオブジェクト
		CursorSprite cursorSprite_;
	};
}