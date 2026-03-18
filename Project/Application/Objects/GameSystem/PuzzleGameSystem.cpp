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
	matchFinder_.Initialize(board_.GetWidth(), board_.GetHeight());

	// * ゲームのビジュアルの初期化 * //
	// 盤面のスプライトを初期化
	boardSprite_.Initialize(createSpriteFunc, board_.GetWidth(), board_.GetHeight());
	// カーソルのスプライトを初期化
	cursorSprite_.Initialize(createSpriteFunc, boardSprite_.GetAnchorSprite());
	cursorSprite_.SetPosition(boardSprite_.GetCellPosition(cursor_.GetMoveX(), cursor_.GetMoveY()));
	// HPゲージのスプライトを初期化
	hpGaugeSprite_.Initialize(createSpriteFunc);
	hpGaugeSprite_.SetParent(boardSprite_.GetAnchorSprite());
	hpGaugeSprite_.SetPosition(Vector3(280.0f, -200.0f, 0.0f));
	hpGaugeSprite_.SetSize(Vector3(300.0f, 100.0f, 1.0f));
	hpGaugeSprite_.SetRotation(Vector3(0.0f, 0.0f, 3.14f * 0.5f));
}

void PuzzleGameSystem::Update() {
	SystemUpdate();
	VisualUpdate();
}

void Application::PuzzleGameSystem::SystemUpdate() {
	if (!player_->IsAlive()) {
		return; // プレイヤーが死んでいる場合は更新をスキップ
	}

	// カーソルの状態を更新
	cursor_.Update();
	// カーソルが起動していたら盤面をずらす動きに変える
	if (cursor_.IsSelected()) {
		board_.ShiftRow(cursor_.GetY(), cursor_.GetMoveX());
		board_.ShiftColumn(cursor_.GetX(), -cursor_.GetMoveY());
		boardSprite_.ShiftRow(cursor_.GetY(), -cursor_.GetMoveX() * boardSprite_.GetCellSize());
		boardSprite_.ShiftColumn(cursor_.GetX(), cursor_.GetMoveY() * boardSprite_.GetCellSize());

		if(cursor_.GetMoveX() != 0 || cursor_.GetMoveY() != 0){
			Application::MatsumotoUtility::PlaySE("panelMove.mp3");
		}
	}
	else {
		if (cursor_.GetMoveX() != 0 || cursor_.GetMoveY() != 0) {
			Application::MatsumotoUtility::PlaySE("cursorMove.mp3");
		}
	}
	// プレイヤーが発火したら盤面から同じ色が3つ以上並んでいる場所を探す
	if (player_->IsSend()) {
		std::vector<std::vector<std::pair<int, int>>> matches =
			matchFinder_.FindThreeColorMatch(board_.GetBoardData());

		boardSprite_.RegisterMatchCells(matches);
		board_.RegisterEreaseCells(matches);
	}
	else {
		// マッチアニメーションが進行中でなければ盤面を更新する
		if (!boardSprite_.IsAnimatingMatch()) {
			if (!board_.IsEreaseCellsEmpty()) {
				board_.EreaseCells();
				boardSprite_.ShrinkCells(board_.FindEmptyCells(), 0.0f);
				board_.FillEmptyCells();

				// プレイヤーにダメージを与える(ノイズを消した数)
				int damage = board_.GetNoiseEraseCount() * 5; // ノイズ1つにつき5ダメージ
				player_->TakeDamage(damage);
				board_.ResetNoiseEraseCount();

				if (damage > 0) {
					Application::MatsumotoUtility::PlaySE("noiseSpawn.mp3");
				}
			}
		}
	}

	// プレイヤーの最大体力を盤面の4番(ノイズ)の比率と同期する
	player_->SetMaxHp(
		static_cast<int>(static_cast<float>(player_->GetDefaultMaxHp()) * (1.0f - board_.NoiseRatio())));
}

void Application::PuzzleGameSystem::VisualUpdate() {
	// 盤面のスプライトを更新
	boardSprite_.Update(board_.GetBoardData(), KashipanEngine::GetDeltaTime());
	// カーソルのスプライトを更新
	Vector2 cursorPosition = boardSprite_.GetCellPosition(cursor_.GetX(), cursor_.GetY());
	cursorSprite_.Update(cursorPosition, cursor_.IsSelected());
	// HPゲージのスプライトを更新
	hpGaugeSprite_.Update(player_->GetHpRatio(), 1.0f - board_.NoiseRatio());
}

void Application::PuzzleGameSystem::SetAnchorSpritePosition(const Vector3& position) {
	if (auto* anchorSprite = boardSprite_.GetAnchorSprite()) {
		Application::MatsumotoUtility::SetTranslateToSprite(anchorSprite, position);
	}
}

void Application::PuzzleGameSystem::DeathAnimation()
{
	if (!player_->IsAlive()) {
		Vector3 currentPos = MatsumotoUtility::GetTranslateFromSprite(boardSprite_.GetAnchorSprite());
		currentPos.y = -1000.0f; // 画面外に移動させる
		MatsumotoUtility::SimpleEaseSpriteMove(boardSprite_.GetAnchorSprite(), currentPos, 0.1f);
	}
}
