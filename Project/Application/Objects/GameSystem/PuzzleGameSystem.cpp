#include "PuzzleGameSystem.h"
using namespace Application;
#include <MatsumotoUtility.h>

void Application::PuzzleGameSystem::Initialize(
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc,
	PuzzlePlayer* puzzlePalyer) {

	// プレイヤーオブジェクトの設定
	player_ = puzzlePalyer;

	// * ゲームのシステムの初期化 * //
	// 盤面の初期化
	board_.Initialize(6, 6);
	board_.Reset();
	// カーソルの初期化と入力コマンドの設定
	cursor_.Initialize(board_.GetWidth(), board_.GetHeight());
	cursor_.SetMoveUpFunction([puzzlePalyer]() { return puzzlePalyer->IsMoveUp(); });
	cursor_.SetMoveDownFunction([puzzlePalyer]() { return puzzlePalyer->IsMoveDown(); });
	cursor_.SetMoveLeftFunction([puzzlePalyer]() { return puzzlePalyer->IsMoveLeft(); });
	cursor_.SetMoveRightFunction([puzzlePalyer]() { return puzzlePalyer->IsMoveRight(); });
	cursor_.SetSelectFunction([puzzlePalyer]() { return puzzlePalyer->IsSelect(); });
	// マッチファインダーの初期化
	matchFinder_.Initialize(board_.GetWidth(),board_.GetHeight());

	// * ゲームのビジュアルの初期化 * //
	// 盤面のスプライトを初期化
	boardSprite_.Initialize(createSpriteFunc, board_.GetWidth(), board_.GetHeight());
	// カーソルのスプライトを初期化
	cursorSprite_.Initialize(createSpriteFunc,boardSprite_.GetAnchorSprite());
	cursorSprite_.SetPosition(boardSprite_.GetCellPosition(cursor_.GetMoveX(),cursor_.GetMoveY()));
}

void PuzzleGameSystem::Update() {
	SystemUpdate();
	VisualUpdate();
}

void Application::PuzzleGameSystem::SystemUpdate() {
	// カーソルの状態を更新
	cursor_.Update();
	// カーソルが起動していたら盤面をずらす動きに変える
	if(cursor_.IsSelected()) {
		board_.ShiftRow(cursor_.GetY(), cursor_.GetMoveX());
		board_.ShiftColumn(cursor_.GetX(), -cursor_.GetMoveY());
		boardSprite_.ShiftRow(cursor_.GetY(), -cursor_.GetMoveX() * boardSprite_.GetCellSize());
		boardSprite_.ShiftColumn(cursor_.GetX(), cursor_.GetMoveY() * boardSprite_.GetCellSize());
	}
	// プレイヤーが発火したら盤面から同じ色が3つ以上並んでいる場所を探す
	if (player_->IsSend()) {
		std::vector<std::vector<std::pair<int, int>>> matches =
			matchFinder_.FindThreeColorMatch(board_.GetBoardData());

		boardSprite_.RegisterMatchCells(matches);
	}

}

void Application::PuzzleGameSystem::VisualUpdate() {
	// 盤面のスプライトを更新
	boardSprite_.Update(board_.GetBoardData(),KashipanEngine::GetDeltaTime());
	// カーソルのスプライトを更新
	Vector2 cursorPosition = boardSprite_.GetCellPosition(cursor_.GetX(), cursor_.GetY());
	cursorSprite_.Update(cursorPosition,cursor_.IsSelected());
}
