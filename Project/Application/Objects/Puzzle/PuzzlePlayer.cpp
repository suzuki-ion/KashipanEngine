#include "Objects/Puzzle/PuzzlePlayer.h"
#include "Assets/AudioManager.h"
#include <algorithm>
#include <set>
#include <queue>
#include <cmath>

#include <MatsumotoUtility.h>

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
		cmdAttack_ = commandPrefix + "TimeSkip";
		cmdSwitchBoard_ = commandPrefix + "SwitchBoard";

        parentOriginalPos_ = parentTransform_ ? parentTransform_->GetTranslate() : Vector3(0.0f, 0.0f, 0.0f);

		boards_[0].Initialize(config_.stageSize, config_.panelTypeCount);
		boards_[1].Initialize(config_.stageSize, config_.panelTypeCount);
		activeBoard_ = 0;

		int center = config_.stageSize / 2;
		cursor_.Initialize(center, center, config_.stageSize, config_.cursorEasingDuration, commandPrefix);

		combo_.Initialize();

		gameElapsedTime_ = 0.0f;
		phase_ = Phase::Idle;
		moveCount_ = 0;
		inactiveDecayTimer_ = 0.0f;
		prevActionHoldingForSE_ = false;
		comboSePitch_ = -5.0f;

		rowLocks_[0].clear();
		colLocks_[0].clear();
		rowLocks_[1].clear();
		colLocks_[1].clear();

		pendingGarbagePositions_.clear();
		nextMoveGarbagePositions_.clear();
		pendingGarbageAccumulator_ = 0.0f;
		garbageQueue_.clear();

		CreateBoardRootTransforms();
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
			}
			else if (window_) {
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
				sprite->SetPivotPoint(0.5f, 0.5f);
				if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
					mat->SetColor(config_.stageBackgroundColor);
					mat->SetTexture(whiteTexture);
				}
				if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetParentTransform(activeBoardTransform_);
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
				sprite->SetPivotPoint(0.5f, 0.5f);
				if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
					mat->SetTexture(whiteTexture);
				}
				if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetParentTransform(activeBoardTransform_);
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
				sprite->SetPivotPoint(0.5f, 0.5f);
				if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
					mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
					mat->SetTexture(whiteTexture);
				}
				if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetParentTransform(activeBoardTransform_);
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
				sprite->SetPivotPoint(0.5f, 0.5f);
				if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
					mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
					mat->SetTexture(whiteTexture);
				}
				if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetParentTransform(activeBoardTransform_);
					Vector2 pos = BoardToScreen(r, c);
					tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
					tr->SetScale(Vector3(config_.panelScale, config_.panelScale, 1.0f));
				}
				attachSprite(sprite.get());
				moveGarbageWarningSprites_[r * n + c] = sprite.get();
				addObject2DFunc_(std::move(sprite));
			}
		}

		// 4. お邪魔パネルキューゲージ
		float gaugeY = -(stageHeight * 0.5f + 20.0f);
		for (int i = 0; i < kMaxGarbageGauges; i++) {
			float yOff = gaugeY - static_cast<float>(i) * 22.0f;
			// 背景
			{
				auto sprite = std::make_unique<KashipanEngine::Sprite>();
				sprite->SetUniqueBatchKey();
				sprite->SetName(playerName_ + "_GarbGaugeBG_" + std::to_string(i));
				sprite->SetPivotPoint(0.5f, 0.5f);
				if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
					mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
					mat->SetTexture(whiteTexture);
				}
				if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetParentTransform(activeBoardTransform_);
					tr->SetTranslate(Vector3(0.0f, yOff, 0.0f));
					tr->SetScale(Vector3(stageWidth, 16.0f, 1.0f));
				}
				attachSprite(sprite.get());
				garbageGaugeSprites_[i].bg = sprite.get();
				addObject2DFunc_(std::move(sprite));
			}
			// Fill
			{
				auto sprite = std::make_unique<KashipanEngine::Sprite>();
				sprite->SetUniqueBatchKey();
				sprite->SetName(playerName_ + "_GarbGaugeFill_" + std::to_string(i));
				// 1P: 右端アンカー（右から左へ減少）、2P: 左端アンカー（左から右へ減少→右から左に見える）
				sprite->SetPivotPoint(isPlayer2_ ? 0.0f : 1.0f, 0.5f);
				if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
					mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
					mat->SetTexture(whiteTexture);
				}
				if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
					float fillX = isPlayer2_ ? -stageWidth * 0.5f : stageWidth * 0.5f;
					tr->SetParentTransform(activeBoardTransform_);
					tr->SetTranslate(Vector3(fillX, yOff, 0.0f));
					tr->SetScale(Vector3(stageWidth, 12.0f, 1.0f));
				}
				attachSprite(sprite.get());
				garbageGaugeSprites_[i].fill = sprite.get();
				addObject2DFunc_(std::move(sprite));
			}
			// Amount text
			{
				auto text = std::make_unique<KashipanEngine::Text>(16);
				text->SetName(playerName_ + "_GarbGaugeText_" + std::to_string(i));
				if (auto* tr = text->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetParentTransform(activeBoardTransform_);
					// 1P: ゲージ右側、2P: ゲージ左側
					float textX = isPlayer2_ ? (-stageWidth * 0.5f - 4.0f) : (stageWidth * 0.5f + 4.0f);
					tr->SetTranslate(Vector3(textX, yOff, 0.0f));
                    tr->SetScale(Vector3(0.5f, 0.5f, 0.5f));
				}
				text->SetFont("Assets/Application/test.fnt");
				text->SetText("");
				// 1P: 左寄せ（ゲージ右に表示）、2P: 右寄せ（ゲージ左に表示）
				text->SetTextAlign(
					isPlayer2_ ? KashipanEngine::TextAlignX::Right : KashipanEngine::TextAlignX::Left,
					KashipanEngine::TextAlignY::Center);
				if (screenBuffer2D_) {
					text->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
				}
				else if (window_) {
					text->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
				}
				garbageGaugeSprites_[i].amountText = text.get();
				addObject2DFunc_(std::move(text));
			}
		}

		// 5. ロックオーバーレイ（行）
		rowLockSprites_.resize(n);
		for (int r = 0; r < n; r++) {
			auto sprite = std::make_unique<KashipanEngine::Sprite>();
			sprite->SetUniqueBatchKey();
			sprite->SetName(playerName_ + "_RowLock_" + std::to_string(r));
			sprite->SetPivotPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
				mat->SetTexture(KashipanEngine::TextureManager::GetTextureFromFileName("Lock_Horizontal.png"));
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(activeBoardTransform_);
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
			sprite->SetPivotPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
				mat->SetTexture(KashipanEngine::TextureManager::GetTextureFromFileName("Lock_Vertical.png"));
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(activeBoardTransform_);
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
			sprite->SetPivotPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(config_.cursorColor);
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(activeBoardTransform_);
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
				tr->SetParentTransform(activeBoardTransform_);
				tr->SetTranslate(Vector3(0.0f, stageHeight * 0.5f + 20.0f, 0.0f));
			}
			text->SetFont("Assets/Application/test.fnt");
			text->SetText("0%");
			text->SetTextAlign(KashipanEngine::TextAlignX::Center, KashipanEngine::TextAlignY::Center);
			if (screenBuffer2D_) {
				text->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			else if (window_) {
				text->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			activeCollapseText_ = text.get();
			addObject2DFunc_(std::move(text));
		}

		// 9. 非アクティブボードプレビュー背景
		float previewScale = 1.0f;
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
			sprite->SetPivotPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.1f, 0.1f, 0.1f, 0.8f));
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(inactiveBoardTransform_);
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
				sprite->SetPivotPoint(0.5f, 0.5f);
				if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
					mat->SetTexture(whiteTexture);
				}
				if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetParentTransform(inactiveBoardTransform_);
					float px = (c - halfPN + 0.5f) * previewCellSize;
					float py = (static_cast<float>(n - 1) - r - halfPN + 0.5f) * previewCellSize;
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
			sprite->SetPivotPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(inactiveBoardTransform_);
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
			sprite->SetPivotPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(inactiveBoardTransform_);
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
				tr->SetParentTransform(inactiveBoardTransform_);
				tr->SetTranslate(Vector3(0.0f, previewY - previewHeight * 0.5f - 12.0f, 0.0f));
                tr->SetScale(Vector3(2.0f, 2.0f, 2.0f));
			}
			text->SetFont("Assets/Application/test.fnt");
			text->SetText("0%");
			text->SetTextAlign(KashipanEngine::TextAlignX::Center, KashipanEngine::TextAlignY::Center);
			if (screenBuffer2D_) {
				text->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			else if (window_) {
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
			}
			else if (window_) {
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
			}
			else if (window_) {
				text->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			comboText_ = text.get();
			addObject2DFunc_(std::move(text));
		}

		// 14. 入れ替えのクールダウン
		{
			swapCooldown_.Initialize(config_.swapCooldown);
			float size = 500.0f;// クールダウン表示は結構大きめにして目立たせる
			// ローディングみたいな丸がクルクル回るやつの背景オーバーレイ
			auto bgSprite = std::make_unique<KashipanEngine::Sprite>();
			bgSprite->SetUniqueBatchKey();
			bgSprite->SetName(playerName_ + "_SwapCooldownBG");
			bgSprite->SetPivotPoint(0.5f, 0.5f);
			if (auto* mat = bgSprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.8f));
				mat->SetTexture(whiteTexture);
			}
			if (auto* tr = bgSprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(inactiveBoardTransform_);
				tr->SetScale(Vector3(size, size, 1.0f));
			}
			if (screenBuffer2D_) {
				bgSprite->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			else if (window_) {
				bgSprite->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			switchCooldownBackGroundSprite_ = bgSprite.get();
			addObject2DFunc_(std::move(bgSprite));

			// ローディングみたいな丸がクルクル回るやつ
			auto sprite = std::make_unique<KashipanEngine::Sprite>();
			sprite->SetUniqueBatchKey();
			sprite->SetName(playerName_ + "_SwapCooldown");
			sprite->SetPivotPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
				mat->SetTexture(KashipanEngine::TextureManager::GetTextureFromFileName("loading.png"));
			}
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(inactiveBoardTransform_);
				tr->SetScale(Vector3(size, size, 1.0f));
			}
			if (screenBuffer2D_) {
				sprite->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			else if (window_) {
				sprite->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			switchCooldownSprite_ = sprite.get();
			addObject2DFunc_(std::move(sprite));
		}

		// 15. 残り移動回数テキスト
		{
			auto text = std::make_unique<KashipanEngine::Text>(32);
			text->SetName(playerName_ + "_reMoveCount");
			if (auto* tr = text->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(activeBoardTransform_);
				if (isPlayer2_) {
					tr->SetTranslate(Vector3(stageHeight * 0.5f + 100.0f,0.0f, 0.0f));
				}
				else {
					tr->SetTranslate(Vector3(-stageHeight * 0.5f - 100.0f, 0.0f, 0.0f));
				}
				
			}
			text->SetFont("Assets/Application/test.fnt");
			text->SetText("0%");
			text->SetTextAlign(KashipanEngine::TextAlignX::Center, KashipanEngine::TextAlignY::Center);
			if (screenBuffer2D_) {
				text->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			else if (window_) {
				text->AttachToRenderer(window_, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			remainingMoveCount_ = text.get();
			addObject2DFunc_(std::move(text));
		}
	}

	// ================================================================
	// ボードルートトランスフォーム生成
	// ================================================================

	void PuzzlePlayer::CreateBoardRootTransforms()
	{
		if (!addObject2DFunc_) return;

		// アクティブボード用ルートトランスフォーム
		{
			auto sprite = std::make_unique<KashipanEngine::Sprite>();
			sprite->SetName(playerName_ + "_ActiveBoardRoot");
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(parentTransform_);
				tr->SetTranslate(Vector3(0.0f, 0.0f, 0.0f));
				tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
			}
			activeBoardTransform_ = sprite->GetComponent2D<KashipanEngine::Transform2D>();
			addObject2DFunc_(std::move(sprite));
		}

		// 非アクティブボード用ルートトランスフォーム
		{
			int n = config_.stageSize;
			float previewScale = 0.3f;
			float previewCellSize = (config_.panelScale + config_.panelGap) * previewScale;
			float previewWidth = static_cast<float>(n) * previewCellSize;
			float previewHeight = static_cast<float>(n) * previewCellSize;
			float cellSize = config_.panelScale + config_.panelGap;
			float stageWidth = static_cast<float>(n) * cellSize;
			float stageHeight = static_cast<float>(n) * cellSize;
			float previewX = isPlayer2_ ? (stageWidth * 0.5f + 30.0f + previewWidth * 0.5f)
				: (-stageWidth * 0.5f - 30.0f - previewWidth * 0.5f);
			float previewY = -stageHeight * 0.5f + previewHeight * 0.5f;

			auto sprite = std::make_unique<KashipanEngine::Sprite>();
			sprite->SetName(playerName_ + "_InactiveBoardRoot");
			if (auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetParentTransform(parentTransform_);
				tr->SetTranslate(Vector3(previewX, previewY, 0.0f));
				tr->SetScale(Vector3(previewScale, previewScale, 1.0f));
			}
			inactiveBoardTransform_ = sprite->GetComponent2D<KashipanEngine::Transform2D>();
			addObject2DFunc_(std::move(sprite));
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
				mat->SetTexture(KashipanEngine::TextureManager::GetTextureFromFileName("Noise.png"));
				mat->SetColor(config_.garbageColor);
			}
			else if (type > 0 && type <= PuzzleGameConfig::kMaxPanelTypes) {
				mat->SetColor(config_.panelColors[type - 1]);
				mat->SetTexture(KashipanEngine::TextureManager::GetTextureFromFileName("white1x1.png"));
			}
			else {
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

	void PuzzlePlayer::StartSwapPanelAnimation()
	{
		if (isPlayer2_) {
			activeBoardTransform_->SetTranslate(Vector3(300.0f, -300.0f, 0.0f));
			inactiveBoardTransform_->SetTranslate(Vector3(-300.0f, 300.0f, 0.0f));
		}
		else {
			activeBoardTransform_->SetTranslate(Vector3(-300.0f, -300.0f, 0.0f));
			inactiveBoardTransform_->SetTranslate(Vector3(300.0f, 300.0f, 0.0f));
		}
		activeBoardTransform_->SetScale(Vector3(0.3f, 0.3f, 1.3f));
		inactiveBoardTransform_->SetScale(Vector3(1.5f, 1.5f, 1.0f));
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
		for (auto* sprite : inactiveRowLockSprites_) {
			if (!sprite) continue;
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			}
		}
		for (auto* sprite : inactiveColLockSprites_) {
			if (!sprite) continue;
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			}
		}
	}

	void PuzzlePlayer::UpdateGarbageQueueGauges() {
		float cellSize = config_.panelScale + config_.panelGap;
		float stageWidth = static_cast<float>(config_.stageSize) * cellSize;

		for (int i = 0; i < kMaxGarbageGauges; i++) {
			auto& gs = garbageGaugeSprites_[i];
			bool active = i < static_cast<int>(garbageQueue_.size());

			// ゲージ位置を詰め直す
			float gaugeY = -(stageWidth * 0.5f + 20.0f) - static_cast<float>(i) * 22.0f;

			if (gs.bg) {
				if (auto* mat = gs.bg->GetComponent2D<KashipanEngine::Material2D>()) {
					mat->SetColor(active ? Vector4(0.15f, 0.15f, 0.15f, 1.0f) : Vector4(0.0f, 0.0f, 0.0f, 0.0f));
				}
				if (auto* tr = gs.bg->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetTranslate(Vector3(0.0f, gaugeY, 0.0f));
				}
			}
			if (gs.fill) {
				if (active) {
					const auto& entry = garbageQueue_[i];
					float ratio = (entry.totalTime > 0.0f) ? std::clamp(entry.remainingTime / entry.totalTime, 0.0f, 1.0f) : 0.0f;
					if (auto* mat = gs.fill->GetComponent2D<KashipanEngine::Material2D>()) {
						mat->SetColor(Vector4(0.8f, 0.2f, 0.2f, 1.0f));
					}
					if (auto* tr = gs.fill->GetComponent2D<KashipanEngine::Transform2D>()) {
						// 1P: 右端基準（右から左へ減少）、2P: 左端基準（右から左へ減少）
						float fillX = isPlayer2_ ? -stageWidth * 0.5f : stageWidth * 0.5f;
						tr->SetTranslate(Vector3(fillX, gaugeY, 0.0f));
						tr->SetScale(Vector3(stageWidth * ratio, 12.0f, 1.0f));
					}
				}
				else {
					if (auto* mat = gs.fill->GetComponent2D<KashipanEngine::Material2D>()) {
						mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
					}
				}
			}
			if (gs.amountText) {
				if (active) {
					int amount = static_cast<int>(garbageQueue_[i].garbageAmount);
					gs.amountText->SetTextFormat("{}", amount);
				}
				else {
					gs.amountText->SetText("");
				}
				if (auto* tr = gs.amountText->GetComponent2D<KashipanEngine::Transform2D>()) {
					// 1P: ゲージ右側、2P: ゲージ左側
					float textX = isPlayer2_ ? (-stageWidth * 0.5f - 4.0f) : (stageWidth * 0.5f + 4.0f);
				 tr->SetTranslate(Vector3(textX, gaugeY, 0.0f));
				}
			}
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
		for (auto* sprite : inactivePreviewSprites_) {
			if (!sprite) continue;
			if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			}
		}
		if (inactivePreviewBg_) {
			if (auto* mat = inactivePreviewBg_->GetComponent2D<KashipanEngine::Material2D>()) {
				mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			}
		}
	}

	std::vector<std::vector<std::pair<int, int>>> PuzzlePlayer::BuildAttachedGroups() const {
		const auto& board = GetActiveBoard();
		const int n = config_.stageSize;
		std::vector<std::vector<bool>> seed(n, std::vector<bool>(n, false));
		for (int r = 0; r < n; r++) {
			int c = 0;
			while (c < n) {
				int type = board.GetPanel(r, c);
				if (type <= 0) { c++; continue; }
				int s = c;
				while (c + 1 < n && board.GetPanel(r, c + 1) == type) c++;
				if (c - s + 1 >= config_.normalMinCount) for (int i = s; i <= c; i++) seed[r][i] = true;
				c++;
			}
		}
		for (int c = 0; c < n; c++) {
			int r = 0;
			while (r < n) {
				int type = board.GetPanel(r, c);
				if (type <= 0) { r++; continue; }
				int s = r;
				while (r + 1 < n && board.GetPanel(r + 1, c) == type) r++;
				if (r - s + 1 >= config_.normalMinCount) for (int i = s; i <= r; i++) seed[i][c] = true;
				r++;
			}
		}
		std::vector<std::vector<bool>> attached(n, std::vector<bool>(n, false));
		static const int dr[4] = { -1,1,0,0 };
		static const int dc[4] = { 0,0,-1,1 };
		for (int r = 0; r < n; r++) {
			for (int c = 0; c < n; c++) {
				if (!seed[r][c]) continue;
				int type = board.GetPanel(r, c);
				if (type <= 0) continue;
				std::queue<std::pair<int, int>> q;
				std::vector<std::vector<bool>> vis(n, std::vector<bool>(n, false));
				q.push({ r, c });
				vis[r][c] = true;
				while (!q.empty()) {
					auto [cr, cc] = q.front(); q.pop();
					attached[cr][cc] = true;
					for (int i = 0; i < 4; i++) {
						int nr = cr + dr[i], nc = cc + dc[i];
						if (nr < 0 || nr >= n || nc < 0 || nc >= n) continue;
						if (vis[nr][nc] || board.GetPanel(nr, nc) != type) continue;
						vis[nr][nc] = true;
						q.push({ nr, nc });
					}
				}
			}
		}
		std::vector<std::vector<std::pair<int, int>>> groups;
		std::vector<std::vector<bool>> grouped(n, std::vector<bool>(n, false));
		for (int r = 0; r < n; r++) {
			for (int c = 0; c < n; c++) {
				if (!attached[r][c] || grouped[r][c]) continue;
				int type = board.GetPanel(r, c);
				std::vector<std::pair<int, int>> g;
				std::queue<std::pair<int, int>> q;
				q.push({ r, c });
				grouped[r][c] = true;
				while (!q.empty()) {
					auto [cr, cc] = q.front(); q.pop();
					g.push_back({ cr, cc });
					for (int i = 0; i < 4; i++) {
						int nr = cr + dr[i], nc = cc + dc[i];
						if (nr < 0 || nr >= n || nc < 0 || nc >= n) continue;
						if (grouped[nr][nc] || !attached[nr][nc] || board.GetPanel(nr, nc) != type) continue;
						grouped[nr][nc] = true;
						q.push({ nr, nc });
					}
				}
				groups.push_back(std::move(g));
			}
		}
		return groups;
	}

	std::set<std::pair<int, int>> PuzzlePlayer::BuildAttachedCellSet() const {
		std::set<std::pair<int, int>> out;
		for (const auto& g : BuildAttachedGroups()) for (const auto& p : g) out.insert(p);
		return out;
	}

	void PuzzlePlayer::BuildPendingMatchesFromAttachments() {
		pendingMatches_.clear();
		std::set<std::pair<int, int>> clearedCells;
		for (const auto& g : BuildAttachedGroups()) {
			if (g.empty()) continue;
			PuzzleBoard::MatchResult mr;
			mr.type = PuzzleBoard::MatchType::Normal;
			mr.panelType = GetActiveBoard().GetPanel(g.front().first, g.front().second);
			mr.cells = g;
			for (const auto& p : g) clearedCells.insert(p);
			pendingMatches_.push_back(std::move(mr));
		}

		if (pendingMatches_.empty()) return;

		std::set<std::pair<int, int>> garbageCells;
		static const int dr[4] = { -1, 1, 0, 0 };
		static const int dc[4] = { 0, 0, -1, 1 };
		int n = config_.stageSize;
		for (const auto& [r, c] : clearedCells) {
			for (int i = 0; i < 4; i++) {
				int nr = r + dr[i];
				int nc = c + dc[i];
				if (nr < 0 || nr >= n || nc < 0 || nc >= n) continue;
				if (GetActiveBoard().GetPanel(nr, nc) == PuzzleBoard::kGarbageType) {
					garbageCells.insert({ nr, nc });
				}
			}
		}

		if (!garbageCells.empty()) {
			PuzzleBoard::MatchResult garbageMatch;
			garbageMatch.type = PuzzleBoard::MatchType::Normal;
			garbageMatch.panelType = PuzzleBoard::kGarbageType;
			garbageMatch.cells.assign(garbageCells.begin(), garbageCells.end());
			pendingMatches_.push_back(std::move(garbageMatch));
		}
	}

	int PuzzlePlayer::CountCrossShapesInAttachedMatches() const {
		int crosses = 0;
		for (const auto& m : pendingMatches_) {
			std::set<std::pair<int, int>> s(m.cells.begin(), m.cells.end());
			for (const auto& [r, c] : m.cells) {
				if (!s.count({ r - 1, c }) || !s.count({ r + 1, c }) || !s.count({ r, c - 1 }) || !s.count({ r, c + 1 })) continue;
				crosses++;
			}
		}
		return crosses;
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
		return 0.0f;
	}

	bool PuzzlePlayer::IsDefeated() const {
		float threshold = config_.defeatCollapseRatio;
		return GetActiveCollapseRatio() >= threshold;
	}

	// ================================================================
	// ロック
	// ================================================================

	bool PuzzlePlayer::IsRowLocked(int /*row*/) const {
		return false;
	}

	bool PuzzlePlayer::IsColLocked(int /*col*/) const {
		return false;
	}

	void PuzzlePlayer::UpdateLocks(float /*deltaTime*/) {
	}

	void PuzzlePlayer::ApplyLock(bool /*isRow*/, int /*index*/, float /*seconds*/) {
	}

	void PuzzlePlayer::ClearAllLocks() {
		rowLocks_[0].clear();
		colLocks_[0].clear();
		rowLocks_[1].clear();
		colLocks_[1].clear();
	}

	void PuzzlePlayer::AddToExistingLocks(float /*seconds*/) {
	}

	void PuzzlePlayer::SetCursorPosition(int row, int col) {
		cursor_.Initialize(row, col, config_.stageSize, config_.cursorEasingDuration);
	}

	bool PuzzlePlayer::MoveCursorOneStepToward(int targetRow, int targetCol) {
		if (cursor_.IsMoving()) return false;

		auto [row, col] = cursor_.GetPosition();
		targetRow = std::clamp(targetRow, 0, config_.stageSize - 1);
		targetCol = std::clamp(targetCol, 0, config_.stageSize - 1);

		if (row < targetRow) return cursor_.StepMove(1);
		if (row > targetRow) return cursor_.StepMove(0);
		if (col < targetCol) return cursor_.StepMove(3);
		if (col > targetCol) return cursor_.StepMove(2);
		return false;
	}

	void PuzzlePlayer::CountCursorStepForGarbage() {
		AdvanceMoveGarbageCounter();
	}

	void PuzzlePlayer::ForceMove(int direction) {
		if (IsAnimating()) return;
		StartMoveAction(direction);
	}

	void PuzzlePlayer::ForceAttack() {
		if (IsAnimating()) return;
		OnAttack();
	}

	void PuzzlePlayer::ForceSwitchBoard() {
	}

	void PuzzlePlayer::SwitchBoard() {
	}

	void PuzzlePlayer::AutoSwitchBoardIfNeeded() {
		// 現在の仕様では敗北条件はアクティブボードのみの崩壊度で判定するため、
		// 自動ステージ切り替えは行わない
	}

	void PuzzlePlayer::AdvanceMoveGarbageCounter() {
		moveCount_++;
		if (config_.movesPerGarbage <= 0 || moveCount_ < config_.movesPerGarbage) return;

		moveCount_ = 0;
		auto& board = GetActiveBoard();
		bool spawnedAny = false;
		for (auto& [r, c] : nextMoveGarbagePositions_) {
			if (board.GetPanel(r, c) > 0) {
				board.SetPanel(r, c, PuzzleBoard::kGarbageType);
				spawnedAny = true;
			}
		}
		if (spawnedAny) {
			auto h = KashipanEngine::AudioManager::GetSoundHandleFromFileName("noiseSpawn.mp3");
			if (h != KashipanEngine::AudioManager::kInvalidSoundHandle) {
				KashipanEngine::AudioManager::Play(h, 0.9f);
			}
		}
		nextMoveGarbagePositions_.clear();
		PreCalculateMoveGarbagePositions();
	}

	void PuzzlePlayer::StartMoveAction(int direction) {
		auto [row, col] = cursor_.GetPosition();
		int n = config_.stageSize;
		float scale = config_.panelScale;
		auto& board = GetActiveBoard();

		auto panelMoveHandle = KashipanEngine::AudioManager::GetSoundHandleFromFileName("panelMove.mp3");
		if (panelMoveHandle != KashipanEngine::AudioManager::kInvalidSoundHandle) {
			KashipanEngine::AudioManager::Play(panelMoveHandle, 0.9f);
		}

		auto groups = BuildAttachedGroups();
		std::map<std::pair<int, int>, int> cellToGroup;
		for (int gi = 0; gi < static_cast<int>(groups.size()); gi++) {
			for (const auto& p : groups[gi]) cellToGroup[p] = gi;
		}

		const bool vertical = (direction == 0 || direction == 1);
		std::set<int> affectedLines;
		affectedLines.insert(vertical ? col : row);

		// 選択中セル自体がくっつきグループに属している時だけ、グループ連動移動を行う。
		// それ以外（他ブロック移動に巻き込まれた場合）は通常の1ライン移動として扱う。
		auto cursorGroupIt = cellToGroup.find({ row, col });
		if (cursorGroupIt != cellToGroup.end()) {
			bool expanded = true;
			while (expanded) {
				expanded = false;
				std::set<int> newLines = affectedLines;
				for (int line : affectedLines) {
					for (int i = 0; i < n; i++) {
						int r = vertical ? i : line;
						int c = vertical ? line : i;
						auto it = cellToGroup.find({ r, c });
						if (it == cellToGroup.end()) continue;
						for (const auto& [gr, gc] : groups[it->second]) {
							int addLine = vertical ? gc : gr;
							if (!newLines.count(addLine)) {
								newLines.insert(addLine);
								expanded = true;
							}
						}
					}
				}
				affectedLines = std::move(newLines);
			}
		}

		for (int line : affectedLines) {
			switch (direction) {
			case 0: board.ShiftColDown(line); break;
			case 1: board.ShiftColUp(line); break;
			case 2: board.ShiftRowLeft(line); break;
			case 3: board.ShiftRowRight(line); break;
			}
		}

		AdvanceMoveGarbageCounter();
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

		if (vertical) {
			for (int mvCol : affectedLines) {
				for (int r = 0; r < n; r++) {
					if (direction == 0) addAnim(r, mvCol, (r == n - 1) ? n : (r + 1), mvCol);
					else addAnim(r, mvCol, (r == 0) ? -1 : (r - 1), mvCol);
				}
			}
		} else {
			for (int mvRow : affectedLines) {
				for (int c2 = 0; c2 < n; c2++) {
					if (direction == 2) addAnim(mvRow, c2, mvRow, (c2 == n - 1) ? n : (c2 + 1));
					else addAnim(mvRow, c2, mvRow, (c2 == 0) ? -1 : (c2 - 1));
				}
			}
		}

		std::set<int> animIndices;
		for (auto& a : phaseAnims_) animIndices.insert(a.row * n + a.col);
		for (int r = 0; r < n; r++) {
			for (int c2 = 0; c2 < n; c2++) {
				int idx = r * n + c2;
				if (animIndices.count(idx)) continue;
				ApplyPanelColor(r, c2);
				if (auto* panel = puzzlePanelSprites_[idx]) {
					if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
						Vector2 pos = BoardToScreen(r, c2);
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
		BuildPendingMatchesFromAttachments();
		if (pendingMatches_.empty()) return false;

		if (combo_.GetCurrentCombo() == 0) {
			pendingGarbageAccumulator_ = 0.0f;
			pendingGarbageToSend_ = 0;
		}

		combo_.AddCombo(1);

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
		attackCooldownTimer_ = std::max(attackCooldownTimer_, pendingAttackCooldown_);
		pendingAttackCooldown_ = 0.0f;
		StartFillingPhase();
	}

	void PuzzlePlayer::StartFillingPhase() {
		int n = config_.stageSize;
		float scale = config_.panelScale;

		std::set<std::pair<int, int>> clearedCells;
		for (const auto& m : pendingMatches_) {
			for (auto& cell : m.cells) {
				clearedCells.insert(cell);
			}
		}

		phaseAnims_.clear();
		for (int r = 0; r < n; r++) {
			for (int c = 0; c < n; c++) {
				ApplyPanelColor(r, c);
				int idx = r * n + c;
				auto* panel = (idx >= 0 && idx < static_cast<int>(puzzlePanelSprites_.size())) ? puzzlePanelSprites_[idx] : nullptr;
				if (!panel) continue;

				Vector2 pos = BoardToScreen(r, c);
				if (auto* tr = panel->GetComponent2D<KashipanEngine::Transform2D>()) {
					tr->SetTranslate(Vector3(pos.x, pos.y, 0.0f));
					if (clearedCells.count({ r, c }) > 0) {
						tr->SetScale(Vector3(0.0f, 0.0f, 1.0f));
					}
					else {
						tr->SetScale(Vector3(scale, scale, 1.0f));
					}
				}

				if (clearedCells.count({ r, c }) > 0) {
					PanelAnim a;
					a.row = r; a.col = c;
					a.startPos = pos; a.endPos = pos;
					a.startScale = Vector2(0.0f, 0.0f);
					a.endScale = Vector2(scale, scale);
					phaseAnims_.push_back(a);
				}
			}
		}

		pendingMatches_.clear();

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

	void PuzzlePlayer::CalculateAttackFromMatches() {
		lastMatchSummary_ = {};
		int totalCells = 0;
		int garbageCells = 0;
		for (const auto& m : pendingMatches_) {
			if (m.panelType == PuzzleBoard::kGarbageType) {
				garbageCells += static_cast<int>(m.cells.size());
			}
			totalCells += static_cast<int>(m.cells.size());
		}
		lastMatchSummary_.totalClearedCells = totalCells;
		lastMatchSummary_.garbageClearedCells = garbageCells;
		lastMatchSummary_.crossCount = CountCrossShapesInAttachedMatches();

		float garbageBase = static_cast<float>(totalCells) * config_.attackGarbageMultiplier;
		garbageBase += static_cast<float>(garbageCells) * config_.garbageClearedBonus;
		garbageBase += static_cast<float>(lastMatchSummary_.crossCount) * config_.crossGarbageCount;
		garbageBase *= GetEscalationMultiplier();
		pendingGarbageAccumulator_ += garbageBase;
		pendingGarbageToSend_ = static_cast<int>(pendingGarbageAccumulator_);
		hasPendingAttack_ = (pendingGarbageToSend_ > 0);

		pendingAttackCooldown_ = static_cast<float>(totalCells) * config_.attackCooldownPerClearedBlock;
	}

	void PuzzlePlayer::OnAttack() {
		if (attackCooldownTimer_ > 0.0f) return;
		if (!IsAnimating()) {
			comboSePitch_ = -5.0f;
			if (!StartClearingPhase()) {
				combo_.ResetCombo();
			}
		}
	}

	void PuzzlePlayer::UpdateInactiveBoard(float /*deltaTime*/) {
	}

	void PuzzlePlayer::Update(float deltaTime, KashipanEngine::InputCommand* inputCommand) {
		swapCooldown_.Update(deltaTime);
		UpdateLocks(deltaTime);
		if (attackCooldownTimer_ > 0.0f) {
			attackCooldownTimer_ = std::max(0.0f, attackCooldownTimer_ - deltaTime);
		}

		gameElapsedTime_ += deltaTime;
		UpdateGarbageQueue(deltaTime);

		if (inputCommand) {
			if (inputCommand->Evaluate(cmdAttack_).Triggered() && !IsAnimating()) {
				OnAttack();
			}
		}

		UpdatePhase(deltaTime);

		const auto prevCursorPos = cursor_.GetPosition();
		const bool prevHolding = cursor_.IsHoldingAction();
		cursor_.Update(inputCommand, deltaTime, IsAnimating());
		UpdateCursorSprite();

		if (inputCommand) {
			const auto currentCursorPos = cursor_.GetPosition();
			if (currentCursorPos != prevCursorPos) {
				auto h = KashipanEngine::AudioManager::GetSoundHandleFromFileName("cursorMove.mp3");
				if (h != KashipanEngine::AudioManager::kInvalidSoundHandle) {
					KashipanEngine::AudioManager::Play(h, 0.8f);
				}
			}

			const bool nowHolding = cursor_.IsHoldingAction();
			if (!prevHolding && nowHolding) {
				auto h = KashipanEngine::AudioManager::GetSoundHandleFromFileName("cursorSubmit.mp3");
				if (h != KashipanEngine::AudioManager::kInvalidSoundHandle) {
					KashipanEngine::AudioManager::Play(h, 0.8f);
				}
			}
			if (prevHolding && !nowHolding) {
				auto h = KashipanEngine::AudioManager::GetSoundHandleFromFileName("cursorCancel.mp3");
				if (h != KashipanEngine::AudioManager::kInvalidSoundHandle) {
					KashipanEngine::AudioManager::Play(h, 0.8f);
				}
			}
		}

		if (cursor_.HasMoveAction() && !IsAnimating()) {
			int dir = cursor_.GetMoveActionDirection();
			StartMoveAction(dir);
		}

		UpdateInactiveBoard(deltaTime);

		if (!IsAnimating()) {
			AutoSwitchBoardIfNeeded();
		}

		UpdateLockOverlays();
		UpdateGarbageQueueGauges();
		UpdateCollapseGauge();
		UpdateMatchText();
		UpdateInactivePreview(deltaTime);
		UpdateInactiveLockOverlays();
		UpdateGarbageWarnings();
		UpdateMoveGarbageWarnings();
		UpdateShake(deltaTime);
		UpdateSwapCoolDownSpriteAnimation(deltaTime);
		UpdateSwapPanelAnimations(deltaTime);
		UpdateMoveCountText();
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
		if (!normalCells.empty()) nextMoveGarbagePositions_.push_back(normalCells.front());
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

	void PuzzlePlayer::UpdateMoveCountText() {
		if (remainingMoveCount_) {
			int pct = config_.movesPerGarbage - moveCount_;
			remainingMoveCount_->SetTextFormat("{}", pct);
		}
	}

	void PuzzlePlayer::EnqueueGarbage(float amount, float delayTime) {
		if (amount < 1.0f) return;
		garbageQueue_.push_back({ delayTime, delayTime, amount });
	}

	float PuzzlePlayer::OffsetGarbageQueue(float amount) {
		float remaining = amount;
		for (auto it = garbageQueue_.begin(); it != garbageQueue_.end() && remaining > 0.0f; ) {
			if (remaining >= it->garbageAmount) {
				remaining -= it->garbageAmount;
				it = garbageQueue_.erase(it);
			}
			else {
				it->garbageAmount -= remaining;
				remaining = 0.0f;
				++it;
			}
		}
		return remaining;
	}

	void PuzzlePlayer::UpdateGarbageQueue(float deltaTime) {
		for (auto it = garbageQueue_.begin(); it != garbageQueue_.end(); ) {
			it->remainingTime -= deltaTime;
			if (it->remainingTime <= 0.0f) {
				int count = static_cast<int>(it->garbageAmount);
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
				for (int i = 0; i < count && i < static_cast<int>(normalCells.size()); i++) {
					board.SetPanel(normalCells[i].first, normalCells[i].second, PuzzleBoard::kGarbageType);
				}
				SyncAllPanelVisuals();
				it = garbageQueue_.erase(it);
			}
			else {
				++it;
			}
		}
	}

	float PuzzlePlayer::GetEscalationMultiplier() const {
		if (config_.garbageEscalationInterval <= 0.0f) return 1.0f;
		int steps = static_cast<int>(gameElapsedTime_ / config_.garbageEscalationInterval);
		return 1.0f + static_cast<float>(steps) * config_.garbageEscalationIncrement;
	}

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
				tr->SetTranslate(Vector3(Lerp(a.startPos.x, a.endPos.x, easedT), Lerp(a.startPos.y, a.endPos.y, easedT), 0.0f));
				tr->SetScale(Vector3(Lerp(a.startScale.x, a.endScale.x, easedT), Lerp(a.startScale.y, a.endScale.y, easedT), 1.0f));
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
			case Phase::Moving: OnMoveFinished(); break;
			case Phase::Clearing: OnClearFinished(); break;
			case Phase::Filling: OnFillFinished(); break;
			default: break;
			}
		}
	}

	void PuzzlePlayer::UpdateShake(float /*deltaTime*/) {}
	void PuzzlePlayer::UpdateSwapPanelAnimations(float /*deltaTime*/) {}
	void PuzzlePlayer::UpdateSwapCoolDownSpriteAnimation(float /*deltaTime*/) {}

} // namespace Application
