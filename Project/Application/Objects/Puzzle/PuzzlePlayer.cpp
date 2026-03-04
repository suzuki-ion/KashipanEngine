#include "Objects/Puzzle/PuzzlePlayer.h"
#include <algorithm>
#include <set>
#include <cmath>

namespace Application {

// ================================================================
// 蠎ｧ讓吝､画鋤
// ================================================================

Vector2 PuzzlePlayer::BoardToScreen(int row, int col) const {
	return BoardToScreen(static_cast<float>(row), static_cast<float>(col));
}

Vector2 PuzzlePlayer::BoardToScreen(float row, float col) const {
	float cellSize = config_.panelScale + config_.panelGap;
	float halfN = static_cast<float>(config_.stageSize) * 0.5f;
	float x = (col - halfN + 0.5f) * cellSize;
	float y = (static_cast<float>(config_.stageSize - 1) - row - halfN + 0.5f) * cellSize;
	return Vector2(x, y);
}

// ================================================================
// 蛻晄悄蛹・
// ================================================================

void PuzzlePlayer::Initialize(
	const PuzzleGameConfig& config,
	KashipanEngine::ScreenBuffer* screenBuffer2D,
	KashipanEngine::Window* window,
	AddObject2DFunc addObject2DFunc,
	KashipanEngine::Transform2D* parentTransform,
	const std::string& playerName,
	const std::string& commandPrefix) {

	config_ = config;
	screenBuffer2D_ = screenBuffer2D;
	window_ = window;
	addObject2DFunc_ = addObject2DFunc;
	parentTransform_ = parentTransform;
	playerName_ = playerName;
	commandPrefix_ = commandPrefix;
	cmdTimeSkip_ = commandPrefix + "TimeSkip";

	board_.Initialize(config_.stageSize, config_.panelTypeCount);

	int center = config_.stageSize / 2;
	cursor_.Initialize(center, center, config_.stageSize, config_.cursorEasingDuration, commandPrefix);

	combo_.Initialize();

	hp_ = config_.playerHP;
	timer_ = config_.timeLimit;
	timerActive_ = true;
	phase_ = Phase::Idle;

	rowLocks_.clear();
	colLocks_.clear();

	CreateSprites();
	SyncAllPanelVisuals();
	UpdateCursorSprite();
}

// ================================================================
// 繧ｹ繝励Λ繧､繝育函謌・
// ================================================================

void PuzzlePlayer::CreateSprites() {
	if (!addObject2DFunc_) return;
	int n = config_.stageSize;
	auto whiteTexture = KashipanEngine::TextureManager::GetTextureFromFileName("white1x1.png");

	auto attachSprite = [&](KashipanEngine::Sprite* s) {
		if (screenBuffer2D_) {
			s->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
		} else if (window_) {
			s->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
		}
	};

	float cellSize = config_.panelScale + config_.panelGap;
	float stageWidth = static_cast<float>(n) * cellSize;
	float stageHeight = static_cast<float>(n) * cellSize;

	// ================================================================
	// 1. ステージ背景パネル（最背面）
	// ================================================================
	stagePanelSprites_.resize(n * n);
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			auto sprite = std::make_unique<KashipanEngine::Sprite>();
			sprite->SetUniqueBatchKey();
			sprite->SetName(playerName_ + "_Stage_" + std::to_string(r) + "_" + std::to_string(c));
			sprite->SetAnchorPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(config_.stageBackgroundColor);
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(parentTransform_);
				Vector2 pos = BoardToScreen(r, c);
				tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
				tr->SetScale(Vector3(config_.panelScale, config_.panelScale, 1.0f));
			}
			attachSprite(sprite.get());
			stagePanelSprites_[r * n + c] = sprite.get();
			addObject2DFunc_(std::move(sprite));
		}
	}

	// ================================================================
	// 2. パズルパネル
	// ================================================================
	puzzlePanelSprites_.resize(n * n);
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			auto sprite = std::make_unique<KashipanEngine::Sprite>();
			sprite->SetUniqueBatchKey();
			sprite->SetName(playerName_ + "_Panel_" + std::to_string(r) + "_" + std::to_string(c));
			sprite->SetAnchorPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(parentTransform_);
				Vector2 pos = BoardToScreen(r, c);
				tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
				tr->SetScale(Vector3(config_.panelScale, config_.panelScale, 1.0f));
			}
			attachSprite(sprite.get());
			puzzlePanelSprites_[r * n + c] = sprite.get();
			addObject2DFunc_(std::move(sprite));
		}
	}

	// ================================================================
	// 3. タイマーゲージ（UI背景）
	// ================================================================
	float gaugeY = stageHeight * 0.5f + 20.0f;
	{
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_TimerGaugeBG");
		sprite->SetAnchorPoint(0.5f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(Vector4(0.15f, 0.15f, 0.15f, 1.0f));
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(0.0f, gaugeY, 0.0f));
			tr->SetScale(Vector3(stageWidth, 16.0f, 1.0f));
		}
		attachSprite(sprite.get());
		timerGaugeBgSprite_ = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}
	{
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_TimerGaugeFill");
		sprite->SetAnchorPoint(0.0f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(-stageWidth * 0.5f, gaugeY, 0.0f));
			tr->SetScale(Vector3(stageWidth, 12.0f, 1.0f));
		}
		attachSprite(sprite.get());
		timerGaugeFillSprite_ = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}

	// ================================================================
	// 4. HPゲージ（UI背景）
	// ================================================================
	float gaugeX = -stageWidth * 0.5f - 20.0f;
	{
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_HPGaugeBG");
		sprite->SetAnchorPoint(0.5f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(Vector4(0.15f, 0.15f, 0.15f, 1.0f));
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(gaugeX, 0.0f, 0.0f));
			tr->SetScale(Vector3(16.0f, stageHeight, 1.0f));
		}
		attachSprite(sprite.get());
		hpGaugeBgSprite_ = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}
	{
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_HPGaugeFill");
		sprite->SetAnchorPoint(0.5f, 1.0f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(Vector4(0.2f, 0.8f, 0.3f, 1.0f));
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(gaugeX, -stageHeight * 0.5f, 0.0f));
			tr->SetScale(Vector3(12.0f, stageHeight, 1.0f));
		}
		attachSprite(sprite.get());
		hpGaugeFillSprite_ = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}

	// ================================================================
	// 5. ロックオーバーレイ（行）— パネルより上面
	// ================================================================
	rowLockSprites_.resize(n);
	for (int r = 0; r < n; r++) {
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_RowLock_" + std::to_string(r));
		sprite->SetAnchorPoint(0.5f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			Vector2 pos = BoardToScreen(r, n / 2);
			tr->SetTranslate(Vector3(0.0f, pos.y, 0.0f));
			tr->SetScale(Vector3(stageWidth, config_.panelScale, 1.0f));
		}
		attachSprite(sprite.get());
		rowLockSprites_[r] = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}

	// ================================================================
	// 6. ロックオーバーレイ（列）— パネルより上面
	// ================================================================
	colLockSprites_.resize(n);
	for (int c = 0; c < n; c++) {
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_ColLock_" + std::to_string(c));
		sprite->SetAnchorPoint(0.5f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			Vector2 pos = BoardToScreen(n / 2, c);
			tr->SetTranslate(Vector3(pos.x, 0.0f, 0.0f));
			tr->SetScale(Vector3(config_.panelScale, stageHeight, 1.0f));
		}
		attachSprite(sprite.get());
		colLockSprites_[c] = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}

	// ================================================================
	// 7. カーソル（最前面）
	// ================================================================
	{
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_Cursor");
		sprite->SetAnchorPoint(0.5f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(config_.cursorColor);
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			auto [cr, cc] = cursor_.GetPosition();
			Vector2 pos = BoardToScreen(cr, cc);
			float cursorScale = config_.panelScale + config_.panelGap * 2.0f;
			tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
			tr->SetScale(Vector3(cursorScale, cursorScale, 1.0f));
		}
		attachSprite(sprite.get());
		cursorSprite_ = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}

	// ================================================================
	// 8. マッチテキスト
	// ================================================================
	{
		auto text = std::make_unique<KashipanEngine::Text>(64);
		text->SetName(playerName_ + "_MatchText");
		if (auto* tr = text->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(stageWidth * 0.5f + 30.0f, stageHeight * 0.5f, 0.0f));
		}
		text->SetFont("Assets/Application/test.fnt");
		text->SetText("");
		text->SetTextAlign(KashipanEngine::TextAlignX::Left, KashipanEngine::TextAlignY::Top);
		if (screenBuffer2D_) {
			text->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
		} else if (window_) {
			text->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
		}
		matchText_ = text.get();
		addObject2DFunc_(std::move(text));
	}

	// ================================================================
	// 9. コンボテキスト
	// ================================================================
	{
		auto text = std::make_unique<KashipanEngine::Text>(32);
		text->SetName(playerName_ + "_ComboText");
		if (auto* tr = text->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(stageWidth * 0.5f + 30.0f, stageHeight * 0.5f - 80.0f, 0.0f));
		}
		text->SetFont("Assets/Application/test.fnt");
		text->SetText("");
		text->SetTextAlign(KashipanEngine::TextAlignX::Left, KashipanEngine::TextAlignY::Top);
		if (screenBuffer2D_) {
			text->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
		} else if (window_) {
			text->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
		}
		comboText_ = text.get();
		addObject2DFunc_(std::move(text));
	}
}

// ================================================================
// 笧ｯ繧ｶ繝ｳ隕九◆逶ｮ
// ================================================================

void PuzzlePlayer::ApplyPanelColor(int row, int col) {
	int n = config_.stageSize;
	int idx = row * n + col;
	if (idx < 0 || idx >= static_cast<int>(puzzlePanelSprites_.size())) return;
	auto* panel = puzzlePanelSprites_[idx];
	if (!panel) return;

	int type = board_.GetPanel(row, col);
	if (type > 0 && type <= PuzzleGameConfig::kMaxPanelTypes) {
		if (auto* mat = panel->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(config_.panelColors[type - 1]);
		}
	}
}

void PuzzlePlayer::SyncAllPanelVisuals() {
	int n = config_.stageSize;
	float scale = config_.panelScale;
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			int idx = r * n + c;
			if (idx >= static_cast<int>(puzzlePanelSprites_.size())) continue;
			auto* panel = puzzlePanelSprites_[idx];
			if (!panel) continue;
			ApplyPanelColor(r, c);
			if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
				Vector2 pos = BoardToScreen(r, c);
				tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
				tr->SetScale(Vector3(scale, scale, 1.0f));
			}
		}
	}
}

void PuzzlePlayer::UpdateCursorSprite() {
	if (!cursorSprite_) return;
	auto [interpRow, interpCol] = cursor_.GetInterpolatedPosition();
	if (auto* tr = cursorSprite_->GetComponent2D<KashipanEngine::Transform2D>()) {
		Vector2 pos = BoardToScreen(interpRow, interpCol);
		float cursorScale = config_.panelScale + config_.panelGap * 2.0f;
		tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
		tr->SetScale(Vector3(cursorScale, cursorScale, 1.0f));
	}
}

void PuzzlePlayer::UpdateLockOverlays() {
	int n = config_.stageSize;
	for (int r = 0; r < n; r++) {
		if (r >= static_cast<int>(rowLockSprites_.size())) break;
		auto* sprite = rowLockSprites_[r];
		if (!sprite) continue;
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			bool locked = rowLocks_.count(r) > 0;
			mat->SetColor(locked ? config_.lockColor : Vector4(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
	for (int c = 0; c < n; c++) {
		if (c >= static_cast<int>(colLockSprites_.size())) break;
		auto* sprite = colLockSprites_[c];
		if (!sprite) continue;
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			bool locked = colLocks_.count(c) > 0;
			mat->SetColor(locked ? config_.lockColor : Vector4(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
}

void PuzzlePlayer::UpdateTimerGauge() {
	if (!timerGaugeFillSprite_) return;
	float ratio = std::clamp(timer_ / config_.timeLimit, 0.0f, 1.0f);
	float cellSize = config_.panelScale + config_.panelGap;
	float stageWidth = static_cast<float>(config_.stageSize) * cellSize;
	if (auto* tr = timerGaugeFillSprite_->GetComponent2D<KashipanEngine::Transform2D>()) {
		tr->SetScale(Vector3(stageWidth * ratio, 12.0f, 1.0f));
	}
}

void PuzzlePlayer::UpdateHPGauge() {
	if (!hpGaugeFillSprite_) return;
	float ratio = std::clamp(static_cast<float>(hp_) / static_cast<float>(config_.playerHP), 0.0f, 1.0f);
	float cellSize = config_.panelScale + config_.panelGap;
	float stageHeight = static_cast<float>(config_.stageSize) * cellSize;
	if (auto* tr = hpGaugeFillSprite_->GetComponent2D<KashipanEngine::Transform2D>()) {
		tr->SetScale(Vector3(12.0f, stageHeight * ratio, 1.0f));
	}
}

void PuzzlePlayer::UpdateMatchText() {
	if (matchTextTimer_ > 0.0f) {
		matchTextTimer_ -= 0.016f; // approximate
		if (matchTextTimer_ <= 0.0f) {
			matchTextTimer_ = 0.0f;
			if (matchText_) matchText_->SetText("");
			if (comboText_) comboText_->SetText("");
		}
	}
}

// ================================================================
// 繝ｭ繝・け
// ================================================================

bool PuzzlePlayer::IsRowLocked(int row) const {
	return rowLocks_.count(row) > 0;
}

bool PuzzlePlayer::IsColLocked(int col) const {
	return colLocks_.count(col) > 0;
}

void PuzzlePlayer::UpdateLocks(float deltaTime) {
	for (auto it = rowLocks_.begin(); it != rowLocks_.end(); ) {
		it->second.remainingTime -= deltaTime;
		if (it->second.remainingTime <= 0.0f) {
			it = rowLocks_.erase(it);
		} else {
			++it;
		}
	}
	for (auto it = colLocks_.begin(); it != colLocks_.end(); ) {
		it->second.remainingTime -= deltaTime;
		if (it->second.remainingTime <= 0.0f) {
			it = colLocks_.erase(it);
		} else {
			++it;
		}
	}
}

void PuzzlePlayer::ApplyDamage(int damage) {
	hp_ -= damage;
	if (hp_ < 0) hp_ = 0;

	// ダメージを受けたらシェイク開始
	if (damage > 0) {
		shakeTimer_ = shakeDuration_;
		if (parentTransform_) {
			parentOriginalPos_ = parentTransform_->GetTranslate();
		}
	}
}

void PuzzlePlayer::ApplyLock(bool isRow, int index, float seconds) {
	// 既にロック済みの場合は時間を加算
	if (isRow) {
		if (rowLocks_.count(index)) {
			rowLocks_[index].remainingTime += seconds;
			return;
		}
	} else {
		if (colLocks_.count(index)) {
			colLocks_[index].remainingTime += seconds;
			return;
		}
	}

	// ロック合計数が上限に達している場合は新規ロックを追加しない
	if (GetTotalLockCount() >= kMaxTotalLocks) return;

	if (isRow) {
		rowLocks_[index] = { seconds };
	} else {
		colLocks_[index] = { seconds };
	}
}

void PuzzlePlayer::ClearAllLocks() {
	rowLocks_.clear();
	colLocks_.clear();
}

void PuzzlePlayer::AddToExistingLocks(float seconds) {
	for (auto& [k, v] : rowLocks_) v.remainingTime += seconds;
	for (auto& [k, v] : colLocks_) v.remainingTime += seconds;
}

void PuzzlePlayer::SetCursorPosition(int row, int col) {
	cursor_.Initialize(row, col, config_.stageSize, config_.cursorEasingDuration);
}

void PuzzlePlayer::ForceMove(int direction) {
	if (IsAnimating()) return;
	auto [row, col] = cursor_.GetPosition();
	if (direction == 0 || direction == 1) {
		if (IsColLocked(col)) return;
	} else {
		if (IsRowLocked(row)) return;
	}
	StartMoveAction(direction);
}

void PuzzlePlayer::ForceTimeSkip() {
	if (IsAnimating()) return;
	OnTimeSkip();
}

// ================================================================
// 笧ｯ繧ｶ繝ｳ繝輔ぉ繝ｼ繧ｺ邂｡逅・
// ================================================================

void PuzzlePlayer::StartMoveAction(int direction) {
	auto [row, col] = cursor_.GetPosition();
	int n = config_.stageSize;
	float scale = config_.panelScale;

	switch (direction) {
	case 0: board_.ShiftColDown(col);   break;
	case 1: board_.ShiftColUp(col);     break;
	case 2: board_.ShiftRowLeft(row);   break;
	case 3: board_.ShiftRowRight(row);  break;
	}

	phaseAnims_.clear();

	auto addAnim = [&](int r, int c, int fromR, int fromC) {
		int idx = r * n + c;
		auto* panel = puzzlePanelSprites_[idx];
		if (!panel) return;
		ApplyPanelColor(r, c);
		Vector2 startP = BoardToScreen(fromR, fromC);
		Vector2 endP = BoardToScreen(r, c);
		if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetTranslate(Vector3(startP.x, startP.y, 0.0f));
			tr->SetScale(Vector3(scale, scale, 1.0f));
		}
		PanelAnim a;
		a.row = r; a.col = c;
		a.startPos = startP; a.endPos = endP;
		a.startScale = Vector2(scale, scale);
		a.endScale = Vector2(scale, scale);
		phaseAnims_.push_back(a);
	};

	switch (direction) {
	case 0: for (int r = 0; r < n; r++) addAnim(r, col, (r == n - 1) ? n : (r + 1), col); break;
	case 1: for (int r = 0; r < n; r++) addAnim(r, col, (r == 0) ? -1 : (r - 1), col); break;
	case 2: for (int c = 0; c < n; c++) addAnim(row, c, row, (c == n - 1) ? n : (c + 1)); break;
	case 3: for (int c = 0; c < n; c++) addAnim(row, c, row, (c == 0) ? -1 : (c - 1)); break;
	}

	std::set<int> animIndices;
	for (auto& a : phaseAnims_) animIndices.insert(a.row * n + a.col);
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			int idx = r * n + c;
			if (animIndices.count(idx)) continue;
			ApplyPanelColor(r, c);
			if (auto* panel = puzzlePanelSprites_[idx]) {
				if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
					Vector2 pos = BoardToScreen(r, c);
					tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
				 tr->SetScale(Vector3(scale, scale, 1.0f));
				}
			}
		}
	}

	phase_ = Phase::Moving;
	phaseTimer_ = 0.0f;
	phaseDuration_ = config_.panelMoveEasingDuration;
}

void PuzzlePlayer::OnMoveFinished() {
	SyncAllPanelVisuals();
	// マッチ評価はタイマー切れ or 時間スキップ時にのみ行う
	phase_ = Phase::Idle;
}

bool PuzzlePlayer::StartClearingPhase() {
	pendingMatches_ = board_.DetectAllMatches(config_.normalMinCount, config_.straightMinCount);
	if (pendingMatches_.empty()) return false;

	combo_.AddCombo(static_cast<int>(pendingMatches_.size()));

	int n = config_.stageSize;
	float scale = config_.panelScale;
	std::vector<std::vector<bool>> toBeClear(n, std::vector<bool>(n, false));
	for (const auto& m : pendingMatches_) {
		for (auto& [r, c] : m.cells) {
			toBeClear[r][c] = true;
		}
	}

	phaseAnims_.clear();
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			if (!toBeClear[r][c]) continue;
			Vector2 pos = BoardToScreen(r, c);
			PanelAnim a;
			a.row = r; a.col = c;
			a.startPos = pos; a.endPos = pos;
			a.startScale = Vector2(scale, scale);
			a.endScale = Vector2(scale * 1.3f, scale * 1.3f);
			phaseAnims_.push_back(a);
		}
	}

	// 謾ｻ謦・ｨ育ｮ・
	CalculateAttackFromMatches();

	// 繝槭ャ繝√ユ繧ｭ繧ｹ繝郁｡ｨ遉ｺ
	{
		std::string text;
		if (lastMatchSummary_.squareCount > 0)
			text += "Square x" + std::to_string(lastMatchSummary_.squareCount) + "\n";
		if (lastMatchSummary_.crossCount > 0)
			text += "Cross x" + std::to_string(lastMatchSummary_.crossCount) + "\n";
		if (lastMatchSummary_.straightCount > 0)
			text += "Straight x" + std::to_string(lastMatchSummary_.straightCount) + "\n";
		if (lastMatchSummary_.normalCount > 0)
			text += "Normal x" + std::to_string(lastMatchSummary_.normalCount) + "\n";
		if (matchText_) matchText_->SetText(text);

		if (combo_.GetCurrentCombo() > 1) {
			if (comboText_) comboText_->SetTextFormat("{} Combo!", combo_.GetCurrentCombo());
		} else {
			if (comboText_) comboText_->SetText("");
		}
		matchTextTimer_ = kMatchTextDuration;
	}

	phase_ = Phase::Clearing;
	phaseTimer_ = 0.0f;
	phaseDuration_ = config_.panelClearEasingDuration;
	return true;
}

void PuzzlePlayer::OnClearFinished() {
	int n = config_.stageSize;
	for (auto& a : phaseAnims_) {
		int idx = a.row * n + a.col;
		if (idx >= 0 && idx < static_cast<int>(puzzlePanelSprites_.size())) {
			if (auto* panel = puzzlePanelSprites_[idx]) {
				if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetScale(Vector3(0.0f, 0.0f, 1.0f));
				}
			}
		}
	}
	board_.ClearAndFillMatches(pendingMatches_);
	StartFillingPhase();
}

void PuzzlePlayer::StartFillingPhase() {
	int n = config_.stageSize;
	float scale = config_.panelScale;

	std::map<int, int> hRowClearCount;
	std::map<int, int> vColClearCount;
	for (const auto& m : pendingMatches_) {
		if (m.type == PuzzleBoard::MatchType::Normal || m.type == PuzzleBoard::MatchType::Straight) {
			if (m.isHorizontal) hRowClearCount[m.fixedIndex] += static_cast<int>(m.cells.size());
			else vColClearCount[m.fixedIndex] += static_cast<int>(m.cells.size());
		} else {
			for (auto& [r, c] : m.cells) hRowClearCount[r]++;
		}
	}
	for (auto& [row, cnt] : hRowClearCount) cnt = std::min(cnt, n);
	for (auto& [col, cnt] : vColClearCount) cnt = std::min(cnt, n);

	pendingMatches_.clear();
	phaseAnims_.clear();

	for (int r = 0; r < n; r++)
		for (int c = 0; c < n; c++)
			ApplyPanelColor(r, c);

	for (auto& [row, clearCount] : hRowClearCount) {
		for (int c = 0; c < n; c++) {
			Vector2 endP = BoardToScreen(row, c);
			Vector2 startP = BoardToScreen(row, c - clearCount);
			PanelAnim a;
			a.row = row; a.col = c;
			a.startPos = startP; a.endPos = endP;
			a.startScale = Vector2(scale, scale);
			a.endScale = Vector2(scale, scale);
			phaseAnims_.push_back(a);
			int idx = row * n + c;
			if (auto* panel = puzzlePanelSprites_[idx]) {
				if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetTranslate(Vector3(startP.x, startP.y, 0.0f));
					tr->SetScale(Vector3(scale, scale, 1.0f));
				}
			}
		}
	}

	std::set<int> hRowSet;
	for (auto& [row, cnt] : hRowClearCount) hRowSet.insert(row);
	for (auto& [col, clearCount] : vColClearCount) {
		for (int r = 0; r < n; r++) {
			if (hRowSet.count(r)) continue;
			Vector2 endP = BoardToScreen(r, col);
			Vector2 startP = BoardToScreen(r + clearCount, col);
			PanelAnim a;
			a.row = r; a.col = col;
			a.startPos = startP; a.endPos = endP;
			a.startScale = Vector2(scale, scale);
			a.endScale = Vector2(scale, scale);
			phaseAnims_.push_back(a);
			int idx = r * n + col;
			if (auto* panel = puzzlePanelSprites_[idx]) {
				if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetTranslate(Vector3(startP.x, startP.y, 0.0f));
					tr->SetScale(Vector3(scale, scale, 1.0f));
				}
			}
		}
	}

	std::set<int> animSet;
	for (auto& a : phaseAnims_) animSet.insert(a.row * n + a.col);
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			int idx = r * n + c;
			if (animSet.count(idx)) continue;
			if (auto* panel = puzzlePanelSprites_[idx]) {
				if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
					Vector2 pos = BoardToScreen(r, c);
					tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
					tr->SetScale(Vector3(scale, scale, 1.0f));
				}
			}
		}
	}

	if (phaseAnims_.empty()) {
		SyncAllPanelVisuals();
		OnFillFinished();
		return;
	}

	phase_ = Phase::Filling;
	phaseTimer_ = 0.0f;
	phaseDuration_ = config_.panelSpawnEasingDuration;
}

void PuzzlePlayer::OnFillFinished() {
	SyncAllPanelVisuals();
	if (!StartClearingPhase()) {
		combo_.ResetCombo();
		phase_ = Phase::Idle;
	}
}

// ================================================================
// 蛻ｶ髯先凾髢・
// ================================================================

void PuzzlePlayer::OnTimerExpired() {
	// ロックを全解除
	ClearAllLocks();

	if (!IsAnimating()) {
		if (!StartClearingPhase()) {
			combo_.ResetCombo();
		}
	}
	timer_ = config_.timeLimit;
}

void PuzzlePlayer::OnTimeSkip() {
	remainingTimeAtSkip_ = timer_;
	// ロックを全解除
	ClearAllLocks();
	// タイマーリセット＆マッチ評価
	if (!IsAnimating()) {
		if (!StartClearingPhase()) {
			combo_.ResetCombo();
		}
	}
	timer_ = config_.timeLimit;
}

// ================================================================
// 笝ｭ繝・け
// ================================================================

void PuzzlePlayer::CalculateAttackFromMatches() {
	lastMatchSummary_ = {};

	for (const auto& m : pendingMatches_) {
		switch (m.type) {
		case PuzzleBoard::MatchType::Normal:   lastMatchSummary_.normalCount++; break;
		case PuzzleBoard::MatchType::Straight: lastMatchSummary_.straightCount++; break;
		case PuzzleBoard::MatchType::Cross:    lastMatchSummary_.crossCount++; break;
		case PuzzleBoard::MatchType::Square:   lastMatchSummary_.squareCount++; break;
		}
	}

	lastMatchSummary_.comboCount = combo_.GetCurrentCombo();
	lastMatchSummary_.isBreak = board_.IsEmpty();

	// 矢ｺ繝・け譎る俣險育ｮ・
	float baseDamage = 0.0f;
	baseDamage += static_cast<float>(lastMatchSummary_.normalCount * config_.normalDamage);
	baseDamage += static_cast<float>(lastMatchSummary_.straightCount * config_.straightDamage);
	baseDamage += static_cast<float>(lastMatchSummary_.crossCount * config_.crossDamage);
	baseDamage += static_cast<float>(lastMatchSummary_.squareCount * config_.squareDamage);

	float comboMult = 1.0f;
	if (lastMatchSummary_.comboCount > 1) {
		comboMult = std::pow(config_.comboDamageMultiplier, static_cast<float>(lastMatchSummary_.comboCount - 1));
	}
	float breakMult = lastMatchSummary_.isBreak ? config_.breakDamageMultiplier : 1.0f;

	pendingDamage_ = static_cast<int>(std::round(baseDamage * comboMult * breakMult));

	// 笝ｭ繝・け譎る俣險育ｮ・
	float baseLock = 0.0f;
	baseLock += static_cast<float>(lastMatchSummary_.normalCount) * config_.normalLockTime;
	baseLock += static_cast<float>(lastMatchSummary_.straightCount) * config_.straightLockTime;
	baseLock += static_cast<float>(lastMatchSummary_.crossCount) * config_.crossLockTime;
	baseLock += static_cast<float>(lastMatchSummary_.squareCount) * config_.squareLockTime;

	float lockComboMult = 1.0f;
	if (lastMatchSummary_.comboCount > 1) {
		lockComboMult = std::pow(config_.comboLockMultiplier, static_cast<float>(lastMatchSummary_.comboCount - 1));
	}
	float lockBreakMult = lastMatchSummary_.isBreak ? config_.breakLockMultiplier : 1.0f;

	pendingLockTime_ = baseLock * lockComboMult * lockBreakMult;

	hasPendingAttack_ = (pendingDamage_ > 0 || pendingLockTime_ > 0.0f);
}

// ================================================================
// 笝ｭ繝・け繧ｸ
// ================================================================

void PuzzlePlayer::UpdatePhase(float deltaTime) {
	if (phase_ == Phase::Idle) return;

	phaseTimer_ += deltaTime;
	float t = std::clamp(phaseTimer_ / phaseDuration_, 0.0f, 1.0f);
	float easedT = Apply(t, EaseType::EaseOutCubic);

	int n = config_.stageSize;
	for (auto& a : phaseAnims_) {
		int idx = a.row * n + a.col;
		if (idx < 0 || idx >= static_cast<int>(puzzlePanelSprites_.size())) continue;
		auto* panel = puzzlePanelSprites_[idx];
		if (!panel) continue;
		if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
			float px = Lerp(a.startPos.x, a.endPos.x, easedT);
			float py = Lerp(a.startPos.y, a.endPos.y, easedT);
			tr->SetTranslate(Vector3(px, py, 0.0f));
			float sx = Lerp(a.startScale.x, a.endScale.x, easedT);
			float sy = Lerp(a.startScale.y, a.endScale.y, easedT);
			tr->SetScale(Vector3(sx, sy, 1.0f));
		}
		if (phase_ == Phase::Clearing) {
			if (auto* mat = panel->GetComponent2D<KashipanEngine::Material2D>()) {
				Vector4 color = mat->GetColor();
				color.w = 1.0f - easedT;
				mat->SetColor(color);
			}
		}
	}

	if (t >= 1.0f) {
		Phase finished = phase_;
		phaseAnims_.clear();
		switch (finished) {
		case Phase::Moving:  OnMoveFinished();  break;
		case Phase::Clearing: OnClearFinished(); break;
		case Phase::Filling:  OnFillFinished();  break;
		default: break;
		}
	}
}

// ================================================================
// 豈弱ヵ繝ｬ繝ｼ繝譖ｴ譁ｰ
// ================================================================

void PuzzlePlayer::Update(float deltaTime, KashipanEngine::InputCommand* inputCommand) {
	// 笧ｯ繧ｶ繝ｳ繝・け譖ｴ譁ｰ
	UpdateLocks(deltaTime);

	// 蛻ｶ髯先凾髢・
	if (timerActive_ && !IsAnimating()) {
		timer_ -= deltaTime;
		if (timer_ <= 0.0f) {
			timer_ = 0.0f;
			OnTimerExpired();
		}
	}

	// 譎る俣繧ｹ繧ｭ繝・・
	if (inputCommand) {
		if (inputCommand->Evaluate(cmdTimeSkip_).Triggered() && !IsAnimating()) {
			OnTimeSkip();
		}
	}

	// 笧ｯ繧ｶ繝ｳ繝輔ぉ繝ｼ繧ｺ譖ｴ譁ｰ
	UpdatePhase(deltaTime);

	// 秘医Ο繝・け縺輔ｌ縺溯｡・蛻励〒縺ｮ遘ｻ蜍輔ｒ繝√ぉ繝・け・・
	cursor_.Update(inputCommand, deltaTime, IsAnimating());
	UpdateCursorSprite();

	// 遘ｻ蜍輔い繧ｯ繧ｷ繝ｧ繝ｳ
	if (cursor_.HasMoveAction() && !IsAnimating()) {
		int dir = cursor_.GetMoveActionDirection();
		auto [row, col] = cursor_.GetPosition();
		bool blocked = false;
		if (dir == 0 || dir == 1) {
			blocked = IsColLocked(col);
		} else {
			blocked = IsRowLocked(row);
		}
		if (!blocked) {
			StartMoveAction(dir);
		}
	}

	// UI譖ｴ譁ｰ
	UpdateLockOverlays();
	UpdateTimerGauge();
	UpdateHPGauge();
	UpdateMatchText();
	UpdateShake(deltaTime);
}

// ================================================================
// 蛻ｶ髯先凾髢・
// ================================================================

void PuzzlePlayer::UpdateShake(float deltaTime) {
	if (shakeTimer_ <= 0.0f) return;
	if (!parentTransform_) return;

	shakeTimer_ -= deltaTime;
	if (shakeTimer_ <= 0.0f) {
		shakeTimer_ = 0.0f;
		parentTransform_->SetTranslate(parentOriginalPos_);
		return;
	}

	float t = shakeTimer_ / shakeDuration_;
	float offsetX = KashipanEngine::GetRandomFloat(-shakeIntensity_, shakeIntensity_) * t;
	float offsetY = KashipanEngine::GetRandomFloat(-shakeIntensity_, shakeIntensity_) * t;
	parentTransform_->SetTranslate(Vector3(
		parentOriginalPos_.x + offsetX,
		parentOriginalPos_.y + offsetY,
		parentOriginalPos_.z));
}

} // namespace Application
