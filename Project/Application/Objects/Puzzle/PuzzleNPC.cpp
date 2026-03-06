#include "Objects/Puzzle/PuzzleNPC.h"
#include <algorithm>
#include <cmath>

namespace Application {

void PuzzleNPC::Initialize(PuzzlePlayer* player, Difficulty difficulty) {
	player_ = player;
	difficulty_ = difficulty;
	movesThisTurn_ = 0;
	shouldSkip_ = false;

	switch (difficulty_) {
	case Difficulty::Easy:
		thinkInterval_ = 0.5f;
		skipChance_ = 0.1f;
		skipThreshold_ = 0.3f;
		switchChance_ = 0.05f;
		maxMovesPerTurn_ = 32;
		break;
	case Difficulty::Normal:
		thinkInterval_ = 0.25f;
		skipChance_ = 0.3f;
		skipThreshold_ = 0.4f;
		switchChance_ = 0.1f;
		maxMovesPerTurn_ = 64;
		break;
	case Difficulty::Hard:
		thinkInterval_ = 0.1f;
		skipChance_ = 0.5f;
		skipThreshold_ = 0.5f;
		switchChance_ = 0.15f;
		maxMovesPerTurn_ = 128;
		break;
	}

	thinkTimer_ = thinkInterval_;
}

void PuzzleNPC::Update(float deltaTime) {
	if (!player_) return;
	if (player_->IsDefeated()) return;
	if (player_->IsAnimating()) return;

	thinkTimer_ -= deltaTime;
	if (thinkTimer_ > 0.0f) return;

	thinkTimer_ = thinkInterval_;
	DecideNextAction();
}

void PuzzleNPC::DecideNextAction() {
	if (!player_) return;

	// 崩壊度が高い場合、ステージ切り替えを検討
	if (player_->GetActiveCollapseRatio() > 0.5f && KashipanEngine::GetRandomBool(switchChance_)) {
		player_->ForceSwitchBoard();
		return;
	}

	// 時間スキップ判定
	float timerRatio = player_->GetTimer() / player_->GetTimeLimit();
	if (timerRatio < skipThreshold_ && KashipanEngine::GetRandomBool(skipChance_)) {
		player_->ForceTimeSkip();
		movesThisTurn_ = 0;
		return;
	}

	switch (difficulty_) {
	case Difficulty::Easy:
		DoRandomMove();
		break;
	case Difficulty::Normal:
		DoDecentMove();
		break;
	case Difficulty::Hard:
		DoBestMove();
		break;
	}

	movesThisTurn_++;
	if (movesThisTurn_ >= maxMovesPerTurn_) {
		movesThisTurn_ = 0;
	}
}

void PuzzleNPC::DoRandomMove() {
	if (!player_) return;

	int boardSize = player_->GetBoardSize();
	if (boardSize <= 0) return;

	int row = KashipanEngine::GetRandomInt(0, boardSize - 1);
	int col = KashipanEngine::GetRandomInt(0, boardSize - 1);
	player_->SetCursorPosition(row, col);

	int direction = KashipanEngine::GetRandomInt(0, 3);

	for (int attempt = 0; attempt < 4; attempt++) {
		bool blocked = false;
		if (direction == 0 || direction == 1) {
			blocked = player_->IsColLocked(col);
		} else {
			blocked = player_->IsRowLocked(row);
		}
		if (!blocked) break;
		direction = (direction + 1) % 4;
	}

	player_->ForceMove(direction);
}

int PuzzleNPC::SimulateAndScore(int row, int col, int direction) const {
	if (!player_) return 0;

	const auto& config = player_->GetConfig();
	PuzzleBoard simBoard;
	simBoard.SetBoard(player_->GetBoard().GetBoard());

	switch (direction) {
	case 0: simBoard.ShiftColDown(col);   break;
	case 1: simBoard.ShiftColUp(col);     break;
	case 2: simBoard.ShiftRowLeft(row);   break;
	case 3: simBoard.ShiftRowRight(row);  break;
	}

	auto matches = simBoard.DetectAllMatches(config.normalMinCount, config.straightMinCount);
	if (matches.empty()) return 0;

	int score = 0;
	for (const auto& m : matches) {
		if (m.panelType == PuzzleBoard::kGarbageType) {
			// お邪魔パネル除去はボーナス
			score += static_cast<int>(m.cells.size()) * 5;
			continue;
		}
		switch (m.type) {
		case PuzzleBoard::MatchType::Square:   score += 50; break;
		case PuzzleBoard::MatchType::Cross:    score += 30; break;
		case PuzzleBoard::MatchType::Straight: score += 20; break;
		case PuzzleBoard::MatchType::Normal:   score += 10; break;
		}
		score += static_cast<int>(m.cells.size());
	}

	// 複数マッチ（コンボ）ボーナス
	if (matches.size() > 1) {
		score += static_cast<int>(matches.size()) * 15;
	}

	return score;
}

std::vector<PuzzleNPC::ScoredMove> PuzzleNPC::EvaluateAllMoves() const {
	std::vector<ScoredMove> moves;
	if (!player_) return moves;

	int boardSize = player_->GetBoardSize();

	// 全ての行/列に対して全方向のシフトを評価
	for (int r = 0; r < boardSize; r++) {
		for (int dir = 2; dir <= 3; dir++) { // 2=左, 3=右（行のシフト）
			if (player_->IsRowLocked(r)) continue;
			int score = SimulateAndScore(r, 0, dir);
			if (score > 0) {
				moves.push_back({ r, 0, dir, score });
			}
		}
	}

	for (int c = 0; c < boardSize; c++) {
		for (int dir = 0; dir <= 1; dir++) { // 0=上, 1=下（列のシフト）
			if (player_->IsColLocked(c)) continue;
			int score = SimulateAndScore(0, c, dir);
			if (score > 0) {
				moves.push_back({ 0, c, dir, score });
			}
		}
	}

	// スコア降順ソート
	std::sort(moves.begin(), moves.end(), [](const ScoredMove& a, const ScoredMove& b) {
		return a.score > b.score;
	});

	return moves;
}

void PuzzleNPC::DoBestMove() {
	auto moves = EvaluateAllMoves();
	if (moves.empty()) {
		// マッチする移動がなければランダム
		DoRandomMove();
		return;
	}

	// 最もスコアの高い移動を実行
	const auto& best = moves[0];
	player_->SetCursorPosition(best.cursorRow, best.cursorCol);
	player_->ForceMove(best.direction);
}

void PuzzleNPC::DoDecentMove() {
	auto moves = EvaluateAllMoves();
	if (moves.empty()) {
		DoRandomMove();
		return;
	}

	// 上位の移動からランダムに選択（上位3つまたは全体の半分）
	int topCount = std::min(static_cast<int>(moves.size()), 3);
	int idx = KashipanEngine::GetRandomInt(0, topCount - 1);
	const auto& chosen = moves[idx];
	player_->SetCursorPosition(chosen.cursorRow, chosen.cursorCol);
	player_->ForceMove(chosen.direction);
}

} // namespace Application
