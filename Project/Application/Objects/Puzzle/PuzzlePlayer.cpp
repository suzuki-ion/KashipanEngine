#include "Objects/Puzzle/PuzzlePlayer.h"
#include <algorithm>
#include <set>
#include <cmath>

namespace Application {

// ================================================================
// 座標変換
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
// 初期化
// ================================================================

void PuzzlePlayer::Initialize(
	const PuzzleGameConfig& config,
	KashipanEngine::ScreenBuffer* screenBuffer2D,
	KashipanEngine::Window* window,
	AddObject2DFunc addObject2DFunc,
	KashipanEngine::Transform2D* parentTransform,
	const std::string& playerName,
	const std::string& commandPrefix,
	bool isPlayer2) {

	config_ = config;
	screenBuffer2D_ = screenBuffer2D;
	window_ = window;
	addObject2DFunc_ = addObject2DFunc;
	parentTransform_ = parentTransform;
	playerName_ = playerName;
	commandPrefix_ = commandPrefix;
	isPlayer2_ = isPlayer2;
	cmdTimeSkip_ = commandPrefix + "TimeSkip";
	cmdSwitchBoard_ = commandPrefix + "SwitchBoard";

	boards_[0].Initialize(config_.stageSize, config_.panelTypeCount);
	boards_[1].Initialize(config_.stageSize, config_.panelTypeCount);
	activeBoard_ = 0;

	int center = config_.stageSize / 2;
	cursor_.Initialize(center, center, config_.stageSize, config_.cursorEasingDuration, commandPrefix);

	combo_.Initialize();

	timer_ = config_.timeLimit;
	timerActive_ = true;
	phase_ = Phase::Idle;
	moveCount_ = 0;
	inactiveDecayTimer_ = 0.0f;

	rowLocks_[0].clear();
	colLocks_[0].clear();
	rowLocks_[1].clear();
	colLocks_[1].clear();

	pendingGarbagePositions_.clear();
	nextMoveGarbagePositions_.clear();

	CreateSprites();
	SyncAllPanelVisuals();
	UpdateCursorSprite();

	// 初回の移動時お邪魔パネル予告位置を計算
	PreCalculateMoveGarbagePositions();
}

// ================================================================
// スプライト生成
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

	// 1. ステージ背景パネル
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

	// 2. パズルパネル
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

	// 3. お邪魔パネル予告オーバーレイ
	garbageWarningSprites_.resize(n * n);
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			auto sprite = std::make_unique<KashipanEngine::Sprite>();
			sprite->SetUniqueBatchKey();
			sprite->SetName(playerName_ + "_GarbWarn_" + std::to_string(r) + "_" + std::to_string(c));
			sprite->SetAnchorPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(parentTransform_);
				Vector2 pos = BoardToScreen(r, c);
				tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
				tr->SetScale(Vector3(config_.panelScale, config_.panelScale, 1.0f));
			}
			attachSprite(sprite.get());
			garbageWarningSprites_[r * n + c] = sprite.get();
			addObject2DFunc_(std::move(sprite));
		}
	}

	// 3.5. 移動時お邪魔パネル予告オーバーレイ
	moveGarbageWarningSprites_.resize(n * n);
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			auto sprite = std::make_unique<KashipanEngine::Sprite>();
			sprite->SetUniqueBatchKey();
			sprite->SetName(playerName_ + "_MoveGarbWarn_" + std::to_string(r) + "_" + std::to_string(c));
			sprite->SetAnchorPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(parentTransform_);
				Vector2 pos = BoardToScreen(r, c);
				tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
				tr->SetScale(Vector3(config_.panelScale, config_.panelScale, 1.0f));
			}
			attachSprite(sprite.get());
			moveGarbageWarningSprites_[r * n + c] = sprite.get();
			addObject2DFunc_(std::move(sprite));
		}
	}

	// 4. タイマーゲージ
	float gaugeY = -(stageHeight * 0.5f + 20.0f);
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

	// 5. ロックオーバーレイ（行）
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

	// 6. ロックオーバーレイ（列）
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

	// 7. カーソル
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

	// 8. 崩壊度テキスト（アクティブ）
	{
		auto text = std::make_unique<KashipanEngine::Text>(32);
		text->SetName(playerName_ + "_ActiveCollapse");
		if (auto* tr = text->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(0.0f, stageHeight * 0.5f + 20.0f, 0.0f));
		}
		text->SetFont("Assets/Application/test.fnt");
		text->SetText("0%");
		text->SetTextAlign(KashipanEngine::TextAlignX::Center, KashipanEngine::TextAlignY::Center);
		if (screenBuffer2D_) {
			text->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
		} else if (window_) {
			text->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
		}
		activeCollapseText_ = text.get();
		addObject2DFunc_(std::move(text));
	}

	// 9. 非アクティブボードプレビュー背景
	float previewScale = 0.3f;
	float previewCellSize = (config_.panelScale + config_.panelGap) * previewScale;
	float previewWidth = static_cast<float>(n) * previewCellSize;
	float previewHeight = static_cast<float>(n) * previewCellSize;
	float previewX = isPlayer2_ ? (stageWidth * 0.5f + 30.0f + previewWidth * 0.5f)
	                             : (-stageWidth * 0.5f - 30.0f - previewWidth * 0.5f);
	float previewY = -stageHeight * 0.5f + previewHeight * 0.5f;
	{
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_InactivePreviewBG");
		sprite->SetAnchorPoint(0.5f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(Vector4(0.1f, 0.1f, 0.1f, 0.8f));
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(previewX, previewY, 0.0f));
			tr->SetScale(Vector3(previewWidth + 8.0f, previewHeight + 8.0f, 1.0f));
		}
		attachSprite(sprite.get());
		inactivePreviewBg_ = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}

	// 10. 非アクティブボードプレビューパネル
	inactivePreviewSprites_.resize(n * n);
	float halfPN = static_cast<float>(n) * 0.5f;
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			auto sprite = std::make_unique<KashipanEngine::Sprite>();
			sprite->SetUniqueBatchKey();
			sprite->SetName(playerName_ + "_InactivePreview_" + std::to_string(r) + "_" + std::to_string(c));
			sprite->SetAnchorPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(parentTransform_);
				float px = previewX + (c - halfPN + 0.5f) * previewCellSize;
				float py = previewY + (static_cast<float>(n - 1) - r - halfPN + 0.5f) * previewCellSize;
				tr->SetTranslate(Vector3(px, py, 0.0f));
				tr->SetScale(Vector3(config_.panelScale * previewScale, config_.panelScale * previewScale, 1.0f));
			}
			attachSprite(sprite.get());
			inactivePreviewSprites_[r * n + c] = sprite.get();
			addObject2DFunc_(std::move(sprite));
		}
	}

	// 10.5. 非アクティブボードプレビュー用ロックオーバーレイ（行）
	inactiveRowLockSprites_.resize(n);
	for (int r = 0; r < n; r++) {
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_InactiveRowLock_" + std::to_string(r));
		sprite->SetAnchorPoint(0.5f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			float py = previewY + (static_cast<float>(n - 1) - r - halfPN + 0.5f) * previewCellSize;
			tr->SetTranslate(Vector3(previewX, py, 0.0f));
			tr->SetScale(Vector3(previewWidth, config_.panelScale * previewScale, 1.0f));
		}
		attachSprite(sprite.get());
		inactiveRowLockSprites_[r] = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}

	// 10.6. 非アクティブボードプレビュー用ロックオーバーレイ（列）
	inactiveColLockSprites_.resize(n);
	for (int c = 0; c < n; c++) {
		auto sprite = std::make_unique<KashipanEngine::Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName(playerName_ + "_InactiveColLock_" + std::to_string(c));
		sprite->SetAnchorPoint(0.5f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			mat->SetTexture(whiteTexture);
		}
		if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			float px = previewX + (c - halfPN + 0.5f) * previewCellSize;
			tr->SetTranslate(Vector3(px, previewY, 0.0f));
			tr->SetScale(Vector3(config_.panelScale * previewScale, previewHeight, 1.0f));
		}
		attachSprite(sprite.get());
		inactiveColLockSprites_[c] = sprite.get();
		addObject2DFunc_(std::move(sprite));
	}

	// 11. 崩壊度テキスト（非アクティブ）
	{
		auto text = std::make_unique<KashipanEngine::Text>(32);
		text->SetName(playerName_ + "_InactiveCollapse");
		if (auto* tr = text->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(previewX, previewY - previewHeight * 0.5f - 12.0f, 0.0f));
		}
		text->SetFont("Assets/Application/test.fnt");
		text->SetText("0%");
		text->SetTextAlign(KashipanEngine::TextAlignX::Center, KashipanEngine::TextAlignY::Center);
		if (screenBuffer2D_) {
			text->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
		} else if (window_) {
			text->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
		}
		inactiveCollapseText_ = text.get();
		addObject2DFunc_(std::move(text));
	}

	// 12. マッチテキスト
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

	// 13. コンボテキスト
	{
		auto text = std::make_unique<KashipanEngine::Text>(32);
		text->SetName(playerName_ + "_ComboText");
		if (auto* tr = text->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(parentTransform_);
			tr->SetTranslate(Vector3(stageWidth * 0.5f + 30.0f, stageHeight * 0.5f - 160.0f, 0.0f));
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
// パネル色適用
// ================================================================

void PuzzlePlayer::ApplyPanelColor(int row, int col) {
	int n = config_.stageSize;
	int idx = row * n + col;
	if (idx < 0 || idx >= static_cast<int>(puzzlePanelSprites_.size())) return;
	auto* panel = puzzlePanelSprites_[idx];
	if (!panel) return;

	int type = GetActiveBoard().GetPanel(row, col);
	if (auto* mat = panel->GetComponent2D<KashipanEngine::Material2D>()) {
		if (type == PuzzleBoard::kGarbageType) {
			mat->SetColor(config_.garbageColor);
		} else if (type > 0 && type <= PuzzleGameConfig::kMaxPanelTypes) {
			mat->SetColor(config_.panelColors[type - 1]);
		} else {
			mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
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
	auto& rLocks = rowLocks_[activeBoard_];
	auto& cLocks = colLocks_[activeBoard_];
	for (int r = 0; r < n; r++) {
		if (r >= static_cast<int>(rowLockSprites_.size())) break;
		auto* sprite = rowLockSprites_[r];
		if (!sprite) continue;
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			bool locked = rLocks.count(r) > 0;
			mat->SetColor(locked ? config_.lockColor : Vector4(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
	for (int c = 0; c < n; c++) {
		if (c >= static_cast<int>(colLockSprites_.size())) break;
		auto* sprite = colLockSprites_[c];
		if (!sprite) continue;
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			bool locked = cLocks.count(c) > 0;
			mat->SetColor(locked ? config_.lockColor : Vector4(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
}

void PuzzlePlayer::UpdateInactiveLockOverlays() {
	int n = config_.stageSize;
	int ib = 1 - activeBoard_;
	auto& rLocks = rowLocks_[ib];
	auto& cLocks = colLocks_[ib];
	for (int r = 0; r < n; r++) {
		if (r >= static_cast<int>(inactiveRowLockSprites_.size())) break;
		auto* sprite = inactiveRowLockSprites_[r];
		if (!sprite) continue;
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			bool locked = rLocks.count(r) > 0;
			mat->SetColor(locked ? config_.lockColor : Vector4(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
	for (int c = 0; c < n; c++) {
		if (c >= static_cast<int>(inactiveColLockSprites_.size())) break;
		auto* sprite = inactiveColLockSprites_[c];
		if (!sprite) continue;
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			bool locked = cLocks.count(c) > 0;
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

void PuzzlePlayer::UpdateCollapseGauge() {
	if (activeCollapseText_) {
		int pct = static_cast<int>(std::round(GetActiveCollapseRatio() * 100.0f));
		activeCollapseText_->SetTextFormat("{}%", pct);
	}
	if (inactiveCollapseText_) {
		int pct = static_cast<int>(std::round(GetInactiveCollapseRatio() * 100.0f));
		inactiveCollapseText_->SetTextFormat("{}%", pct);
	}
}

void PuzzlePlayer::UpdateMatchText() {
	if (matchTextTimer_ > 0.0f) {
		matchTextTimer_ -= 0.016f;
		if (matchTextTimer_ <= 0.0f) {
			matchTextTimer_ = 0.0f;
			if (matchText_) matchText_->SetText("");
			if (comboText_) comboText_->SetText("");
		}
	}
}

void PuzzlePlayer::UpdateInactivePreview(float /*deltaTime*/) {
	int n = config_.stageSize;
	const auto& inactiveBoard = GetInactiveBoard();
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			int idx = r * n + c;
			if (idx >= static_cast<int>(inactivePreviewSprites_.size())) continue;
			auto* sprite = inactivePreviewSprites_[idx];
			if (!sprite) continue;
			int type = inactiveBoard.GetPanel(r, c);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				if (type == PuzzleBoard::kGarbageType) {
					mat->SetColor(config_.garbageColor);
				} else if (type > 0 && type <= PuzzleGameConfig::kMaxPanelTypes) {
					mat->SetColor(config_.panelColors[type - 1]);
				} else {
					mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
				}
			}
		}
	}
}

void PuzzlePlayer::UpdateGarbageWarnings() {
	int n = config_.stageSize;
	std::set<std::pair<int, int>> warningSet(pendingGarbagePositions_.begin(), pendingGarbagePositions_.end());
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			int idx = r * n + c;
			if (idx >= static_cast<int>(garbageWarningSprites_.size())) continue;
			auto* sprite = garbageWarningSprites_[idx];
			if (!sprite) continue;
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				bool warn = warningSet.count({ r, c }) > 0;
				mat->SetColor(warn ? config_.garbageWarningColor : Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			}
		}
	}
}

void PuzzlePlayer::UpdateMoveGarbageWarnings() {
	int n = config_.stageSize;
	std::set<std::pair<int, int>> warningSet(nextMoveGarbagePositions_.begin(), nextMoveGarbagePositions_.end());
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			int idx = r * n + c;
			if (idx >= static_cast<int>(moveGarbageWarningSprites_.size())) continue;
			auto* sprite = moveGarbageWarningSprites_[idx];
			if (!sprite) continue;
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				bool warn = warningSet.count({ r, c }) > 0;
				mat->SetColor(warn ? config_.garbageWarningColor : Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			}
		}
	}
}

void PuzzlePlayer::PreCalculateMoveGarbagePositions() {
	nextMoveGarbagePositions_.clear();
	auto& board = GetActiveBoard();
	int n = config_.stageSize;
	std::vector<std::pair<int, int>> normalCells;
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			if (board.GetPanel(r, c) > 0) normalCells.push_back({ r, c });
		}
	}
	for (int i = static_cast<int>(normalCells.size()) - 1; i > 0; i--) {
		int j = KashipanEngine::GetRandomInt(0, i);
		std::swap(normalCells[i], normalCells[j]);
	}
	int count = 1; // 1個ずつ出現
	for (int i = 0; i < count && i < static_cast<int>(normalCells.size()); i++) {
		nextMoveGarbagePositions_.push_back(normalCells[i]);
	}
}

// ================================================================
// 崩壊度
// ================================================================

float PuzzlePlayer::GetActiveCollapseRatio() const {
	int total = config_.stageSize * config_.stageSize;
	if (total <= 0) return 0.0f;
	return static_cast<float>(GetActiveBoard().CountGarbage()) / static_cast<float>(total);
}

float PuzzlePlayer::GetInactiveCollapseRatio() const {
	int total = config_.stageSize * config_.stageSize;
	if (total <= 0) return 0.0f;
	return static_cast<float>(GetInactiveBoard().CountGarbage()) / static_cast<float>(total);
}

bool PuzzlePlayer::IsDefeated() const {
	float threshold = config_.defeatCollapseRatio;
	return GetActiveCollapseRatio() >= threshold && GetInactiveCollapseRatio() >= threshold;
}

// ================================================================
// ロック
// ================================================================

bool PuzzlePlayer::IsRowLocked(int row) const {
	return rowLocks_[activeBoard_].count(row) > 0;
}

bool PuzzlePlayer::IsColLocked(int col) const {
	return colLocks_[activeBoard_].count(col) > 0;
}

void PuzzlePlayer::UpdateLocks(float deltaTime) {
	// アクティブボードのロック
	for (auto it = rowLocks_[activeBoard_].begin(); it != rowLocks_[activeBoard_].end(); ) {
		it->second.remainingTime -= deltaTime;
		if (it->second.remainingTime <= 0.0f) it = rowLocks_[activeBoard_].erase(it);
		else ++it;
	}
	for (auto it = colLocks_[activeBoard_].begin(); it != colLocks_[activeBoard_].end(); ) {
		it->second.remainingTime -= deltaTime;
		if (it->second.remainingTime <= 0.0f) it = colLocks_[activeBoard_].erase(it);
		else ++it;
	}
	// 非アクティブボードのロックも時間進行
	int ib = 1 - activeBoard_;
	for (auto it = rowLocks_[ib].begin(); it != rowLocks_[ib].end(); ) {
		it->second.remainingTime -= deltaTime;
		if (it->second.remainingTime <= 0.0f) it = rowLocks_[ib].erase(it);
		else ++it;
	}
	for (auto it = colLocks_[ib].begin(); it != colLocks_[ib].end(); ) {
		it->second.remainingTime -= deltaTime;
		if (it->second.remainingTime <= 0.0f) it = colLocks_[ib].erase(it);
		else ++it;
	}
}

void PuzzlePlayer::ApplyGarbage(int count) {
	if (count <= 0) return;
	// 予告位置を決定
	auto& board = GetActiveBoard();
	std::vector<std::pair<int, int>> normalCells;
	int n = config_.stageSize;
	for (int r = 0; r < n; r++) {
		for (int c = 0; c < n; c++) {
			if (board.GetPanel(r, c) > 0) normalCells.push_back({ r, c });
		}
	}
	for (int i = static_cast<int>(normalCells.size()) - 1; i > 0; i--) {
		int j = KashipanEngine::GetRandomInt(0, i);
		std::swap(normalCells[i], normalCells[j]);
	}
	pendingGarbagePositions_.clear();
	for (int i = 0; i < count && i < static_cast<int>(normalCells.size()); i++) {
		pendingGarbagePositions_.push_back(normalCells[i]);
	}
}

void PuzzlePlayer::ApplyLock(bool isRow, int index, float seconds) {
	auto& rLocks = rowLocks_[activeBoard_];
	auto& cLocks = colLocks_[activeBoard_];
	if (isRow) {
		if (rLocks.count(index)) { rLocks[index].remainingTime += seconds; return; }
	} else {
		if (cLocks.count(index)) { cLocks[index].remainingTime += seconds; return; }
	}
	if (static_cast<int>(rLocks.size() + cLocks.size()) >= kMaxTotalLocks) return;
	if (isRow) rLocks[index] = { seconds };
	else cLocks[index] = { seconds };
}

void PuzzlePlayer::ClearAllLocks() {
	rowLocks_[activeBoard_].clear();
	colLocks_[activeBoard_].clear();
}

void PuzzlePlayer::AddToExistingLocks(float seconds) {
	for (auto& [k, v] : rowLocks_[activeBoard_]) v.remainingTime += seconds;
	for (auto& [k, v] : colLocks_[activeBoard_]) v.remainingTime += seconds;
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

void PuzzlePlayer::ForceSwitchBoard() {
	if (IsAnimating()) return;
	SwitchBoard();
}

// ================================================================
// ステージ切り替え
// ================================================================

void PuzzlePlayer::SwitchBoard() {
	activeBoard_ = 1 - activeBoard_;
	inactiveDecayTimer_ = 0.0f;
	SyncAllPanelVisuals();
	UpdateCursorSprite();
}

void PuzzlePlayer::AutoSwitchBoardIfNeeded() {
	float threshold = config_.defeatCollapseRatio;
	if (GetActiveCollapseRatio() >= threshold) {
		// 非アクティブボードも崩壊度が閾値以上なら切り替えない（敗北判定に任せる）
		if (GetInactiveCollapseRatio() >= threshold) return;
		SwitchBoard();
	}
}

// ================================================================
// アニメーションフェーズ
// ================================================================

void PuzzlePlayer::StartMoveAction(int direction) {
	auto [row, col] = cursor_.GetPosition();
	int n = config_.stageSize;
	float scale = config_.panelScale;
	auto& board = GetActiveBoard();

	switch (direction) {
	case 0: board.ShiftColDown(col);   break;
	case 1: board.ShiftColUp(col);     break;
	case 2: board.ShiftRowLeft(row);   break;
	case 3: board.ShiftRowRight(row);  break;
	}

	// 移動回数カウント
	moveCount_++;
	if (config_.movesPerGarbage > 0 && moveCount_ >= config_.movesPerGarbage) {
		moveCount_ = 0;
		// 予告していた位置にお邪魔パネルを配置
		for (auto& [r, c] : nextMoveGarbagePositions_) {
			if (board.GetPanel(r, c) > 0) {
				board.SetPanel(r, c, PuzzleBoard::kGarbageType);
			}
		}
		nextMoveGarbagePositions_.clear();
		// 次回の予告位置を計算
		PreCalculateMoveGarbagePositions();
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
	phase_ = Phase::Idle;
}

bool PuzzlePlayer::StartClearingPhase() {
	pendingMatches_ = GetActiveBoard().DetectAllMatches(config_.normalMinCount, config_.straightMinCount);
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

	CalculateAttackFromMatches();

	{
		std::string text;
		if (lastMatchSummary_.normalCount > 0)
			text += "Normal x" + std::to_string(lastMatchSummary_.normalCount) + "\n";
		if (lastMatchSummary_.straightCount > 0)
			text += "Straight x" + std::to_string(lastMatchSummary_.straightCount) + "\n";
		if (lastMatchSummary_.crossCount > 0)
			text += "Cross x" + std::to_string(lastMatchSummary_.crossCount) + "\n";
		if (lastMatchSummary_.squareCount > 0)
			text += "Square x" + std::to_string(lastMatchSummary_.squareCount) + "\n";
		if (lastMatchSummary_.isBreak)
			text += "Break!\n";
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
	GetActiveBoard().ClearAndFillMatches(pendingMatches_);
	StartFillingPhase();
}

void PuzzlePlayer::StartFillingPhase() {
	int n = config_.stageSize;
	float scale = config_.panelScale;

	std::map<int, int> hRowClearCount;
	std::map<int, int> vColClearCount;
	for (const auto& m : pendingMatches_) {
		if (m.type == PuzzleBoard::MatchType::Normal || m.type == PuzzleBoard::MatchType::Straight) {
			if (m.panelType == PuzzleBoard::kGarbageType) {
				// お邪魔パネルのダミーマッチ → セルごとに行に追加
				for (auto& [r, c] : m.cells) hRowClearCount[r]++;
			} else if (m.isHorizontal) {
				hRowClearCount[m.fixedIndex] += static_cast<int>(m.cells.size());
			} else {
				vColClearCount[m.fixedIndex] += static_cast<int>(m.cells.size());
			}
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
		// コンボ終了 → 予告お邪魔パネルを実際に配置
		if (!pendingGarbagePositions_.empty()) {
			for (auto& [r, c] : pendingGarbagePositions_) {
				if (GetActiveBoard().GetPanel(r, c) > 0) {
					GetActiveBoard().SetPanel(r, c, PuzzleBoard::kGarbageType);
				}
			}
			pendingGarbagePositions_.clear();
			SyncAllPanelVisuals();
		}
		combo_.ResetCombo();
		phase_ = Phase::Idle;
	}
}

// ================================================================
// 制限時間
// ================================================================

void PuzzlePlayer::OnTimerExpired() {
	if (!IsAnimating()) {
		if (!StartClearingPhase()) {
			combo_.ResetCombo();
		}
	}
	timer_ = config_.timeLimit;
}

void PuzzlePlayer::OnTimeSkip() {
	remainingTimeAtSkip_ = timer_;
	if (!IsAnimating()) {
		if (!StartClearingPhase()) {
			combo_.ResetCombo();
		}
	}
	timer_ = config_.timeLimit;
}

// ================================================================
// 攻撃計算
// ================================================================

void PuzzlePlayer::CalculateAttackFromMatches() {
	lastMatchSummary_ = {};
	int totalCells = 0;

	for (const auto& m : pendingMatches_) {
		// お邪魔パネルのダミーマッチはカウントしない
		if (m.panelType == PuzzleBoard::kGarbageType) {
			totalCells += static_cast<int>(m.cells.size());
			continue;
		}
		switch (m.type) {
		case PuzzleBoard::MatchType::Normal:   lastMatchSummary_.normalCount++; break;
		case PuzzleBoard::MatchType::Straight: lastMatchSummary_.straightCount++; break;
		case PuzzleBoard::MatchType::Cross:    lastMatchSummary_.crossCount++; break;
		case PuzzleBoard::MatchType::Square:   lastMatchSummary_.squareCount++; break;
		}
		totalCells += static_cast<int>(m.cells.size());
	}

	lastMatchSummary_.comboCount = combo_.GetCurrentCombo();
	lastMatchSummary_.isBreak = GetActiveBoard().IsEmpty();
	lastMatchSummary_.totalClearedCells = totalCells;

	// ロック時間計算
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

	// お邪魔パネル数計算（形状ごとのパラメータで算出）
	pendingGarbageToSend_ = 0;
	pendingGarbageToSend_ += lastMatchSummary_.normalCount * config_.normalGarbageCount;
	pendingGarbageToSend_ += lastMatchSummary_.straightCount * config_.straightGarbageCount;
	pendingGarbageToSend_ += lastMatchSummary_.crossCount * config_.crossGarbageCount;
	pendingGarbageToSend_ += lastMatchSummary_.squareCount * config_.squareGarbageCount;

	hasPendingAttack_ = (pendingLockTime_ > 0.0f || pendingGarbageToSend_ > 0);
}

// ================================================================
// 非アクティブボード更新
// ================================================================

void PuzzlePlayer::UpdateInactiveBoard(float deltaTime) {
	if (config_.inactiveGarbageDecayInterval <= 0.0f) return;
	inactiveDecayTimer_ += deltaTime;
	while (inactiveDecayTimer_ >= config_.inactiveGarbageDecayInterval) {
		inactiveDecayTimer_ -= config_.inactiveGarbageDecayInterval;
		GetInactiveBoard().RemoveOneGarbageRandom();
	}
}

// ================================================================
// フェーズ更新
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
// シェイク
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

// ================================================================
// 毎フレーム更新
// ================================================================

void PuzzlePlayer::Update(float deltaTime, KashipanEngine::InputCommand* inputCommand) {
	UpdateLocks(deltaTime);

	if (timerActive_ && !IsAnimating()) {
		timer_ -= deltaTime;
		if (timer_ <= 0.0f) {
			timer_ = 0.0f;
			OnTimerExpired();
		}
	}

	if (inputCommand) {
		if (inputCommand->Evaluate(cmdTimeSkip_).Triggered() && !IsAnimating()) {
			OnTimeSkip();
		}
		if (inputCommand->Evaluate(cmdSwitchBoard_).Triggered() && !IsAnimating()) {
			SwitchBoard();
		}
	}

	UpdatePhase(deltaTime);

	cursor_.Update(inputCommand, deltaTime, IsAnimating());
	UpdateCursorSprite();

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

	// 非アクティブボード更新
	UpdateInactiveBoard(deltaTime);

	// 崩壊度による自動ステージ切り替え
	if (!IsAnimating()) {
		AutoSwitchBoardIfNeeded();
	}

	// UI更新
	UpdateLockOverlays();
	UpdateTimerGauge();
	UpdateCollapseGauge();
	UpdateMatchText();
	UpdateInactivePreview(deltaTime);
	UpdateInactiveLockOverlays();
	UpdateGarbageWarnings();
	UpdateMoveGarbageWarnings();
	UpdateShake(deltaTime);
}

} // namespace Application
