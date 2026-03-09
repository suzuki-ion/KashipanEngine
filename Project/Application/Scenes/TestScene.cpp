#include "Scenes/TestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <algorithm>

#include <MatsumotoUtility.h>

namespace KashipanEngine {

	// ================================================================
	// ctor / dtor / Initialize
	// ================================================================

	TestScene::TestScene()
		: SceneBase("TestScene") {
	}

	TestScene::~TestScene() {}

	void TestScene::Initialize() {
		SetNextSceneName("ResultScene");

		gameStartSystem_.Initialize();
		gameStartSystem_.StartSequence(1.0f);

		sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

		AddSceneComponent(std::make_unique<SceneChangeIn>());
		AddSceneComponent(std::make_unique<SceneChangeOut>());

		if (auto* in = GetSceneComponent<SceneChangeIn>()) {
			in->Play();
		}

		// 設定の読み込み
		config_.LoadFromJSON(kConfigPath);

		auto* screenBuffer2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer2D() : nullptr;
		auto* window = Window::GetWindow("Main Window");

		float cx = window ? static_cast<float>(window->GetClientWidth()) * 0.5f : 960.0f;
		float cy = window ? static_cast<float>(window->GetClientHeight()) * 0.5f : 540.0f;

		// ================================================================
		// 背景スプライト
		// ================================================================
		{
			auto sprite = std::make_unique<Sprite>();
			sprite->SetUniqueBatchKey();
			sprite->SetName("PuzzleBackground");
			sprite->SetAnchorPoint(0.5f, 0.5f);
			if (auto* mat = sprite->GetComponent2D<Material2D>()) {
				mat->SetTexture(TextureManager::GetTextureFromFileName("puzzleBackground.png"));
				mat->SetColor(Vector4(0.5f, 0.5f, 0.5f, 1.0f));
			}
			if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
				tr->SetTranslate(Vector3(cx, cy, 0.0f));
				float w = window ? static_cast<float>(window->GetClientWidth()) : 1920.0f;
				float h = window ? static_cast<float>(window->GetClientHeight()) : 1080.0f;
				tr->SetScale(Vector3(w, h, 1.0f));
			}
			if (screenBuffer2D) {
				sprite->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
			} else if (window) {
				sprite->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			backgroundSprite_ = sprite.get();
			AddObject2D(std::move(sprite));
		}

		// ================================================================
		// プレイヤー1 親スプライト（左側）
		// ================================================================
		{
			auto sprite = std::make_unique<Sprite>();
			sprite->SetName("P1_Parent");
			sprite->SetAnchorPoint(0.5f, 0.5f);
			if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
				tr->SetTranslate(Vector3(cx * 0.5f, cy, 0.0f));
				tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
			}
			player1ParentSprite_ = sprite.get();
			player1ParentTransform_ = sprite->GetComponent2D<Transform2D>();
			AddObject2D(std::move(sprite));
		}

		// ================================================================
		// プレイヤー2 親スプライト（右側）
		// ================================================================
		{
			auto sprite = std::make_unique<Sprite>();
			sprite->SetName("P2_Parent");
			sprite->SetAnchorPoint(0.5f, 0.5f);
			if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
				tr->SetTranslate(Vector3(cx * 1.5f, cy, 0.0f));
				tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
			}
			player2ParentSprite_ = sprite.get();
			player2ParentTransform_ = sprite->GetComponent2D<Transform2D>();
			AddObject2D(std::move(sprite));
		}

		// ================================================================
		// プレイヤー初期化
		// ================================================================
		auto addObj2D = [this](std::unique_ptr<Object2DBase> obj) { return AddObject2D(std::move(obj)); };
		player1_.Initialize(config_, screenBuffer2D, window, addObj2D, player1ParentTransform_, "P1", "Puzzle", false);
		player2_.Initialize(config_, screenBuffer2D, window, addObj2D, player2ParentTransform_, "P2", "P2Puzzle", true);

		// NPC初期化
		if (isNPCMode_) {
			npc_.Initialize(&player2_, npcDifficulty_);
		}

		// 結果テキスト
		{
			auto text = std::make_unique<Text>(64);
			text->SetName("ResultText");
			if (auto* tr = text->GetComponent2D<Transform2D>()) {
				tr->SetTranslate(Vector3(cx, cy * 1.8f, 0.0f));
			}
			text->SetFont("Assets/Application/test.fnt");
			text->SetText("");
			text->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
			if (screenBuffer2D) {
				text->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
			} else if (window) {
				text->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
			}
			resultText_ = text.get();
			AddObject2D(std::move(text));
		}

		gameOver_ = false;
		winner_ = 0;

		// ================================================================
		// 操作チュートリアルの初期化
		// ================================================================

		// 1P用チュートリアルスプライトの作成
		{
			std::vector<KashipanEngine::Sprite*> tutorialSprites;
			for (int i = 0; i < 4; i++) {
				auto sprite = std::make_unique<Sprite>();
				sprite->SetName("tutorialSprite" + std::to_string(i));
				sprite->SetUniqueBatchKey();
				sprite->SetAnchorPoint(0.5f, 0.5f);
				if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
					tr->SetTranslate(Vector3(0.0f, 0.0f, 0.0f));
					tr->SetScale(Vector3(128.0f + 64.0f, 32.0f + 16.0f, 1.0f));
				}
				if (auto* mat = sprite->GetComponent2D<Material2D>()) {
					mat->SetTexture(TextureManager::GetTextureFromFileName("tr_" + std::to_string(i) + ".png"));
					mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
				}
				tutorialSprites.push_back(sprite.get());
				sprite->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
				AddObject2D(std::move(sprite));
			}
			tutorialManager1P_.Initialize(tutorialSprites);
			tutorialManager1P_.SetPosition(Vector2(cx * 0.2f, cy * 0.1f));
		}
		// 2P用チュートリアルスプライトの作成
		{
			std::vector<KashipanEngine::Sprite*> tutorialSprites;
			for (int i = 0; i < 4; i++) {
				auto sprite = std::make_unique<Sprite>();
				sprite->SetName("tutorialSprite" + std::to_string(i));
				sprite->SetUniqueBatchKey();
				sprite->SetAnchorPoint(0.5f, 0.5f);
				if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
					tr->SetTranslate(Vector3(0.0f, 0.0f, 0.0f));
					tr->SetScale(Vector3(128.0f + 64.0f, 32.0f + 16.0f, 1.0f));
				}
				if (auto* mat = sprite->GetComponent2D<Material2D>()) {
					mat->SetTexture(TextureManager::GetTextureFromFileName("tr_" + std::to_string(i) + ".png"));
					mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
				}
				tutorialSprites.push_back(sprite.get());
				sprite->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
				AddObject2D(std::move(sprite));
			}
			tutorialManager2P_.Initialize(tutorialSprites);
			tutorialManager2P_.SetPosition(Vector2(cx * 1.8f, cy *0.1f));
		}

        // ================================================================
        // BGM
		// ================================================================
		{
            AudioManager::PlayParams params;
            params.sound = AudioManager::GetSoundHandleFromFileName("bgmGame.mp3");
            params.volume = 0.2f;
            params.loop = true;
            audioPlayer_.AddAudio(params);
            audioPlayer_.ChangeAudio(2.0);
        }
	}

	// ================================================================
	/// 対戦処理
	// ================================================================

	void TestScene::ProcessAttack(Application::PuzzlePlayer& attacker, Application::PuzzlePlayer& defender) {
		if (!attacker.HasPendingAttack()) return;

		const auto& summary = attacker.GetLastMatchSummary();

		// ロック時間計算
		float baseLock = 0.0f;
		baseLock += static_cast<float>(summary.normalCount) * config_.normalLockTime;
		baseLock += static_cast<float>(summary.straightCount) * config_.straightLockTime;
		baseLock += static_cast<float>(summary.crossCount) * config_.crossLockTime;
		baseLock += static_cast<float>(summary.squareCount) * config_.squareLockTime;

		float lockComboMult = 1.0f;
		if (summary.comboCount > 1) {
			lockComboMult = std::pow(config_.comboLockMultiplier, static_cast<float>(summary.comboCount - 1));
		}
		float lockBreakMult = summary.isBreak ? config_.breakLockMultiplier : 1.0f;

		float totalLockTime = baseLock * lockComboMult * lockBreakMult;

		// ロック適用
		ApplyLockToDefender(defender, summary, totalLockTime);

		// お邪魔パネル送信（遅延キュー方式＋相殺）
		float garbageAmount = attacker.GetPendingGarbageToSendFloat();
		if (garbageAmount > 0.0f) {
			// まず自分のキューを相殺
			float surplus = attacker.OffsetGarbageQueue(garbageAmount);
			// 余剰分を相手に遅延キューとして送信
			if (surplus >= 1.0f) {
				float delayTime = surplus * config_.garbageDelayTimeMultiplier;
				defender.EnqueueGarbage(surplus, delayTime);

				auto h = AudioManager::GetSoundHandleFromFileName("noiseSend.mp3");
				if (h != AudioManager::kInvalidSoundHandle) {
					AudioManager::Play(h, 0.9f);
				}
			}
		}

		attacker.ClearPendingAttack();
		attacker.ClearPendingGarbage();
	}

	void TestScene::ApplyLockToDefender(Application::PuzzlePlayer& defender,
		const Application::PuzzlePlayer::MatchSummary& summary,
		float lockTime) {

		if (lockTime <= 0.0f) return;

		int boardSize = defender.GetBoardSize();
		if (boardSize <= 0) return;

		// 既存ロックに加算
		if (!defender.GetRowLocks().empty() || !defender.GetColLocks().empty()) {
			defender.AddToExistingLocks(lockTime);
		}

		// クロスの場合は行1つ＋列1つをロック
		int crossLocks = summary.crossCount;
		for (int i = 0; i < crossLocks; i++) {
			if (defender.GetTotalLockCount() >= 2) break;

			for (int attempt = 0; attempt < boardSize; attempt++) {
				int r = KashipanEngine::GetRandomInt(0, boardSize - 1);
				if (!defender.IsRowLocked(r)) {
					defender.ApplyLock(true, r, lockTime);
					break;
				}
			}

			if (defender.GetTotalLockCount() >= 2) break;

			for (int attempt = 0; attempt < boardSize; attempt++) {
				int c = KashipanEngine::GetRandomInt(0, boardSize - 1);
				if (!defender.IsColLocked(c)) {
					defender.ApplyLock(false, c, lockTime);
					break;
				}
			}
		}

		// ノーマル/ストレート/スクエア → 行or列1つをロック
		int otherLocks = summary.normalCount + summary.straightCount + summary.squareCount;
		for (int i = 0; i < otherLocks; i++) {
			if (defender.GetTotalLockCount() >= 2) break;

			bool isRow = KashipanEngine::GetRandomBool(0.5f);
			for (int attempt = 0; attempt < boardSize * 2; attempt++) {
				int idx = KashipanEngine::GetRandomInt(0, boardSize - 1);
				if (isRow && !defender.IsRowLocked(idx)) {
					defender.ApplyLock(true, idx, lockTime);
					break;
				}
				if (!isRow && !defender.IsColLocked(idx)) {
					defender.ApplyLock(false, idx, lockTime);
					break;
				}
				isRow = !isRow;
			}
		}
	}

	void TestScene::CheckWinCondition() {
		if (gameOver_) return;

		if (player1_.IsDefeated()) {
			gameOver_ = true;
			winner_ = 2;
			if (resultText_) resultText_->SetText("Player 2 Wins!");
		} else if (player2_.IsDefeated()) {
			gameOver_ = true;
			winner_ = 1;
			if (resultText_) resultText_->SetText("Player 1 Wins!");
		}

		autoSceneChangeTimer_ = 3.0f;
	}

	// ================================================================
	// OnUpdate
	// ================================================================

	void TestScene::OnUpdate() {
		float deltaTime = GetDeltaTime();

		// シーン遷移処理
		if (auto* ic = GetInputCommand()) {
			if (ic->Evaluate("DebugSceneChange").Triggered()) {
				if (GetNextSceneName().empty()) {
					SetNextSceneName("MenuScene");
				}
				if (auto* out = GetSceneComponent<SceneChangeOut>()) {
					out->Play();
				}
			}
		}

		if (!GetNextSceneName().empty()) {
			if (auto* out = GetSceneComponent<SceneChangeOut>()) {
				if (out->IsFinished()) {
					ChangeToNextScene();
				}
			}
		}

		gameStartSystem_.Update(deltaTime);
		if (!gameOver_ && gameStartSystem_.IsGameStarted()) {
			// プレイヤー1 更新（キーボード/コントローラー）
			player1_.Update(deltaTime, GetInputCommand());
			
			// プレイヤー2 / NPC 更新
			if (isNPCMode_) {
				npc_.Update(deltaTime);
				player2_.Update(deltaTime, nullptr); // NPC制御なので入力なし
			} else {
				// ローカル対戦の場合：2Pはコントローラー入力（P2Puzzle*コマンド）
				player2_.Update(deltaTime, GetInputCommand());
			}
			tutorialManager2P_.SetIsActiveTutorial(!isNPCMode_);

			// 操作チュートリアルの更新
			tutorialManager1P_.SetIsGrip(player1_.IsMoveCursorMode());
			tutorialManager2P_.SetIsGrip(player2_.IsMoveCursorMode());
			tutorialManager1P_.Update();
			tutorialManager2P_.Update();

			// 攻撃処理
			ProcessAttack(player1_, player2_);
			ProcessAttack(player2_, player1_);

			// 勝敗判定
			CheckWinCondition();
		}

		// ゲームオーバー後の自動シーン遷移処理
		if (gameOver_) {
			autoSceneChangeTimer_ -= deltaTime;
			if(autoSceneChangeTimer_ <= 0.0f) {
				if (GetNextSceneName().empty()) {
					SetNextSceneName("MenuScene");
				}
				if (auto* out = GetSceneComponent<SceneChangeOut>()) {
					out->Play();
				}
			}
		}

#if defined(USE_IMGUI)
		ImGui::Begin("Puzzle Game Config");

		if (ImGui::Button("Force GameOver")) {
			gameOver_ = true;
			winner_ = 1;
			if (resultText_) resultText_->SetText("Player 1 Wins!");
		}

		ImGui::Text("=== Battle Status ===");
		ImGui::Text("P1 Active Board: %d", player1_.GetActiveIndex());
		ImGui::Text("P1 Collapse: %.0f%% / %.0f%%",
			player1_.GetActiveCollapseRatio() * 100.0f,
			player1_.GetInactiveCollapseRatio() * 100.0f);
		ImGui::Text("P2 Active Board: %d", player2_.GetActiveIndex());
		ImGui::Text("P2 Collapse: %.0f%% / %.0f%%",
			player2_.GetActiveCollapseRatio() * 100.0f,
			player2_.GetInactiveCollapseRatio() * 100.0f);
		ImGui::Text("P1 Elapsed: %.1f", player1_.GetGameElapsedTime());
		ImGui::Text("P2 Elapsed: %.1f", player2_.GetGameElapsedTime());
		const char* phaseNames[] = { "Idle", "Moving", "Clearing", "Filling" };
		ImGui::Text("P1 Phase: %s", phaseNames[static_cast<int>(player1_.GetPhase())]);
		ImGui::Text("P2 Phase: %s", phaseNames[static_cast<int>(player2_.GetPhase())]);
		ImGui::Text("P1 Combo: %d", player1_.GetCombo().GetCurrentCombo());
		ImGui::Text("P2 Combo: %d", player2_.GetCombo().GetCurrentCombo());
		ImGui::Text("P1 Garbage Queue: %d", static_cast<int>(player1_.GetGarbageQueue().size()));
		ImGui::Text("P2 Garbage Queue: %d", static_cast<int>(player2_.GetGarbageQueue().size()));

		if (gameOver_) {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Winner: Player %d", winner_);
		}

		ImGui::Separator();
		ImGui::Text("=== NPC Settings ===");
		ImGui::Checkbox("NPC Mode", &isNPCMode_);
		int diff = static_cast<int>(npcDifficulty_);
		const char* diffNames[] = { "Easy", "Normal", "Hard" };
		if (ImGui::Combo("NPC Difficulty", &diff, diffNames, 3)) {
			npcDifficulty_ = static_cast<Application::PuzzleNPC::Difficulty>(diff);
		}

		ImGui::Separator();
		ImGui::Text("=== Config ===");
		ImGui::SliderInt("Stage Size", &config_.stageSize, 3, 8);
		ImGui::SliderFloat("Panel Scale", &config_.panelScale, 16.0f, 128.0f);
		ImGui::SliderFloat("Panel Gap", &config_.panelGap, 0.0f, 16.0f);
		ImGui::SliderInt("Panel Type Count", &config_.panelTypeCount, 2, Application::PuzzleGameConfig::kMaxPanelTypes);
		ImGui::SliderFloat("Panel Move Easing", &config_.panelMoveEasingDuration, 0.01f, 1.0f);
		ImGui::SliderFloat("Panel Clear Easing", &config_.panelClearEasingDuration, 0.01f, 2.0f);
		ImGui::SliderFloat("Panel Spawn Easing", &config_.panelSpawnEasingDuration, 0.01f, 1.0f);
		ImGui::SliderFloat("Cursor Easing", &config_.cursorEasingDuration, 0.01f, 1.0f);
		ImGui::SliderInt("Normal Min Count", &config_.normalMinCount, 2, 8);
		ImGui::SliderInt("Straight Min Count", &config_.straightMinCount, 3, 8);

		ImGui::Separator();
		ImGui::Text("Battle Settings");
		ImGui::SliderFloat("Normal Lock Time", &config_.normalLockTime, 0.0f, 30.0f);
		ImGui::SliderFloat("Straight Lock Time", &config_.straightLockTime, 0.0f, 30.0f);
		ImGui::SliderFloat("Cross Lock Time", &config_.crossLockTime, 0.0f, 30.0f);
		ImGui::SliderFloat("Square Lock Time", &config_.squareLockTime, 0.0f, 30.0f);
		ImGui::SliderFloat("Combo Lock Mult", &config_.comboLockMultiplier, 1.0f, 10.0f);
		ImGui::SliderFloat("Break Lock Mult", &config_.breakLockMultiplier, 1.0f, 10.0f);

		ImGui::Separator();
		ImGui::Text("Garbage Settings");
		ImGui::SliderInt("Moves Per Garbage", &config_.movesPerGarbage, 1, 30);
		ImGui::SliderFloat("Attack Garbage Mult", &config_.attackGarbageMultiplier, 0.0f, 3.0f);
		ImGui::SliderFloat("Inactive Decay Interval", &config_.inactiveGarbageDecayInterval, 0.1f, 10.0f);
		ImGui::SliderFloat("Normal Garbage Count", &config_.normalGarbageCount, 0.0f, 20.0f);
		ImGui::SliderFloat("Straight Garbage Count", &config_.straightGarbageCount, 0.0f, 20.0f);
		ImGui::SliderFloat("Cross Garbage Count", &config_.crossGarbageCount, 0.0f, 20.0f);
		ImGui::SliderFloat("Square Garbage Count", &config_.squareGarbageCount, 0.0f, 20.0f);
		ImGui::SliderFloat("Combo Garbage Mult", &config_.comboGarbageMultiplier, 1.0f, 5.0f);
		ImGui::SliderFloat("Garbage Cleared Bonus", &config_.garbageClearedBonus, 0.0f, 5.0f);
		ImGui::SliderFloat("Garbage Delay Time Mult", &config_.garbageDelayTimeMultiplier, 0.01f, 5.0f);
		ImGui::SliderFloat("Escalation Interval", &config_.garbageEscalationInterval, 10.0f, 300.0f);
		ImGui::SliderFloat("Escalation Increment", &config_.garbageEscalationIncrement, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("Defeat Settings");
		ImGui::SliderFloat("Defeat Collapse Ratio", &config_.defeatCollapseRatio, 0.1f, 1.0f);

		ImGui::Separator();
		ImGui::Text("Colors");
		for (int i = 0; i < std::min(config_.panelTypeCount, Application::PuzzleGameConfig::kMaxPanelTypes); i++) {
			std::string label = "Panel Color " + std::to_string(i + 1);
			ImGui::ColorEdit4(label.c_str(), &config_.panelColors[i].x);
		}
		ImGui::ColorEdit4("Stage BG Color", &config_.stageBackgroundColor.x);
		ImGui::ColorEdit4("Cursor Color", &config_.cursorColor.x);
		ImGui::ColorEdit4("Lock Color", &config_.lockColor.x);
		ImGui::ColorEdit4("Garbage Color", &config_.garbageColor.x);
		ImGui::ColorEdit4("Garbage Warning", &config_.garbageWarningColor.x);

		if (ImGui::Button("Save Config")) {
			config_.SaveToJSON(kConfigPath);
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Config")) {
			config_.LoadFromJSON(kConfigPath);
		}
		ImGui::SameLine();
		if (ImGui::Button("Restart")) {
			SetNextSceneName(GetName());
			ChangeToNextScene();
		}

		// ロック情報表示
		ImGui::Separator();
		ImGui::Text("P1 Row Locks:");
		for (auto& [k, v] : player1_.GetRowLocks()) {
			ImGui::Text("  Row %d: %.1fs", k, v.remainingTime);
		}
		ImGui::Text("P1 Col Locks:");
		for (auto& [k, v] : player1_.GetColLocks()) {
			ImGui::Text("  Col %d: %.1fs", k, v.remainingTime);
		}
		ImGui::Text("P2 Row Locks:");
		for (auto& [k, v] : player2_.GetRowLocks()) {
			ImGui::Text("  Row %d: %.1fs", k, v.remainingTime);
		}
		ImGui::Text("P2 Col Locks:");
		for (auto& [k, v] : player2_.GetColLocks()) {
			ImGui::Text("  Col %d: %.1fs", k, v.remainingTime);
		}

		// お邪魔パネルキュー表示
		ImGui::Separator();
		ImGui::Text("P1 Garbage Queue:");
		for (size_t i = 0; i < player1_.GetGarbageQueue().size(); i++) {
			const auto& e = player1_.GetGarbageQueue()[i];
			ImGui::Text("  [%d] %.0f pcs, %.1fs / %.1fs", static_cast<int>(i), e.garbageAmount, e.remainingTime, e.totalTime);
		}
		ImGui::Text("P2 Garbage Queue:");
		for (size_t i = 0; i < player2_.GetGarbageQueue().size(); i++) {
			const auto& e = player2_.GetGarbageQueue()[i];
			ImGui::Text("  [%d] %.0f pcs, %.1fs / %.1fs", static_cast<int>(i), e.garbageAmount, e.remainingTime, e.totalTime);
		}

		ImGui::End();
#endif
	}

} // namespace KashipanEngine
