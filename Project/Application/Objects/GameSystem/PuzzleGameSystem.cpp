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
}

void PuzzleGameSystem::Update() {
	SystemUpdate();
	VisualUpdate();
}

void Application::PuzzleGameSystem::SystemUpdate() {
	// カーソルの状態を更新
	cursor_.Update();
	// カーソルが起動していたら盤面をずらす動きに変える
	if (cursor_.IsSelected()) {
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
				int damage = board_.GetNoiseEraseCount(); // ノイズ1つにつき5ダメージ
				player_->TakeDamage(damage);
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
	hpGaugeSprite_.Update(test,test2);

	ImGui::Begin("Debug Info");
	ImGui::Text("Cursor Position: (%d, %d)", cursor_.GetX(), cursor_.GetY());
	ImGui::Text("Cursor Selected: %s", cursor_.IsSelected() ? "Yes" : "No");
	ImGui::Spacing();

	ImGui::Text("Player HP: %d / %d", player_->GetHp(), player_->GetMaxHp());
	ImGui::DragFloat("Test Float", &test, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Test Float 2", &test2, 0.01f, 0.0f, 1.0f);

	// デバッグ用に盤面の状態を表示
	ImGui::Text("Board State:");
	const std::vector<int>& boardData = board_.GetBoardData();

	// 幅に合わせてテーブルを作成（枠線付き、サイズ固定）
	if (ImGui::BeginTable("BoardTable", board_.GetWidth(), ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
		// ImGuiの描画順に合わせてYを反転（上から下へ描画）
		for (int y = board_.GetHeight() - 1; y >= 0; --y) {
			ImGui::TableNextRow();
			for (int x = 0; x < board_.GetWidth(); ++x) {
				ImGui::TableNextColumn();
				int cellValue = boardData[y * board_.GetWidth() + x];

				// 0（空きマス）の場合は目立たない色にするなどの工夫
				if (cellValue == 0) {
					ImGui::TextDisabled(" %d ", cellValue);
				}
				else {
					ImGui::Text(" %d ", cellValue);
				}
			}
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

void Application::PuzzleGameSystem::SetAnchorSpritePosition(const Vector3& position) {
	if (auto* anchorSprite = boardSprite_.GetAnchorSprite()) {
		Application::MatsumotoUtility::SetTranslateToSprite(anchorSprite, position);
	}
}
