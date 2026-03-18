#include "PuzzleGameSystem.h"
using namespace Application;
#include <MatsumotoUtility.h>

void Application::PuzzleGameSystem::Initialize(
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc,
	KashipanEngine::InputCommand* ic) {

	// * ゲームのシステムの初期化 * //
	// 盤面の初期化
	board_.Initialize(5, 5);
	board_.Reset();
	// カーソルの初期化と入力コマンドの設定
	cursor_.Initialize(board_.GetWidth(), board_.GetHeight());
	cursor_.SetMoveUpFunction([ic]() { return ic->Evaluate("PuzzleUp").Triggered(); });
	cursor_.SetMoveDownFunction([ic]() { return ic->Evaluate("PuzzleDown").Triggered(); });
	cursor_.SetMoveLeftFunction([ic]() { return ic->Evaluate("PuzzleLeft").Triggered(); });
	cursor_.SetMoveRightFunction([ic]() { return ic->Evaluate("PuzzleRight").Triggered(); });
	cursor_.SetSelectFunction([ic]() { return ic->Evaluate("PuzzleActionHold").Triggered(); });

	// * ゲームのビジュアルの初期化 * //
	// 盤面のスプライトを初期化
	boardSprite_.Initialize(createSpriteFunc, board_.GetWidth(), board_.GetHeight());
	// カーソルのスプライトを初期化
	cursorSprite_.Initialize(createSpriteFunc,boardSprite_.GetAnchorSprite());
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
		board_.ShiftColumn(cursor_.GetX(), cursor_.GetMoveY());
		boardSprite_.ShiftRow(cursor_.GetY(), -cursor_.GetMoveX() * boardSprite_.GetCellSize());
		boardSprite_.ShiftColumn(cursor_.GetX(), -cursor_.GetMoveY() * boardSprite_.GetCellSize());
	}
}

void Application::PuzzleGameSystem::VisualUpdate() {
	// 盤面のスプライトを更新
	boardSprite_.Update(board_.GetBoardData());
	// カーソルのスプライトを更新
	Vector2 cursorPosition = boardSprite_.GetCellPosition(cursor_.GetX(), cursor_.GetY());
	cursorSprite_.Update(cursorPosition);

	ImGui::Begin("PuzzleGameSystem Debug");
	ImGui::Text("Cursor Position: (%d, %d)", cursor_.GetX(), cursor_.GetY());
	ImGui::Text("Cursor Selected: %s", cursor_.IsSelected() ? "Yes" : "No");
	ImGui::Text("Board State:");
	if (ImGui::BeginTable("BoardTable", board_.GetWidth(), ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
		for (int y = 0; y < board_.GetHeight(); ++y) {
			ImGui::TableNextRow();
			for (int x = 0; x < board_.GetWidth(); ++x) {
				ImGui::TableNextColumn();
				// セルの値をそのまま表示し、桁数に応じたズレを防止
				ImGui::Text("%3d", board_.GetCell(x, y));
			}
		}
		ImGui::EndTable();
	}

	ImGui::End();
}
