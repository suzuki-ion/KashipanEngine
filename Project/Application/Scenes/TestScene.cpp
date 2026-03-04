#include "Scenes/TestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <algorithm>

namespace KashipanEngine {

// ================================================================
// ctor / dtor / Initialize
// ================================================================

TestScene::TestScene()
    : SceneBase("TestScene") {}

TestScene::~TestScene() {}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    // 設定の読み込み
    config_.LoadFromJSON(kConfigPath);

    auto *screenBuffer2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer2D() : nullptr;
    auto *window = Window::GetWindow("Main Window");

    float cx = window ? static_cast<float>(window->GetClientWidth()) * 0.5f : 960.0f;
    float cy = window ? static_cast<float>(window->GetClientHeight()) * 0.5f : 540.0f;

    // ================================================================
    // プレイヤー1 親スプライト（左側）
    // ================================================================
    {
        auto sprite = std::make_unique<Sprite>();
        sprite->SetName("P1_Parent");
        sprite->SetAnchorPoint(0.5f, 0.5f);
        if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
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
        if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
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
    player1_.Initialize(config_, screenBuffer2D, window, addObj2D, player1ParentTransform_, "P1", "Puzzle");
    player2_.Initialize(config_, screenBuffer2D, window, addObj2D, player2ParentTransform_, "P2", "P2Puzzle");

    // NPC初期化
    if (isNPCMode_) {
        npc_.Initialize(&player2_, npcDifficulty_);
    }

    // 結果テキスト
    {
        auto text = std::make_unique<Text>(64);
        text->SetName("ResultText");
        if (auto *tr = text->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3(cx, cy * 0.2f, 0.0f));
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
}

// ================================================================
// 対戦処理
// ================================================================

void TestScene::ProcessAttack(Application::PuzzlePlayer& attacker, Application::PuzzlePlayer& defender) {
    if (!attacker.HasPendingAttack()) return;

    const auto& summary = attacker.GetLastMatchSummary();

    // ダメージ計算
    float baseDamage = 0.0f;
    baseDamage += static_cast<float>(summary.normalCount * config_.normalDamage);
    baseDamage += static_cast<float>(summary.straightCount * config_.straightDamage);
    baseDamage += static_cast<float>(summary.crossCount * config_.crossDamage);
    baseDamage += static_cast<float>(summary.squareCount * config_.squareDamage);

    float comboMult = 1.0f;
    if (summary.comboCount > 1) {
        comboMult = std::pow(config_.comboDamageMultiplier, static_cast<float>(summary.comboCount - 1));
    }
    float breakMult = summary.isBreak ? config_.breakDamageMultiplier : 1.0f;

    // 余り時間ボーナス
    float remainBonus = attacker.GetRemainingTimeAtSkip() * config_.remainingTimeDamageBonus;

    int totalDamage = static_cast<int>(std::round((baseDamage * comboMult * breakMult) + remainBonus));
    defender.ApplyDamage(totalDamage);

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

    float remainLockBonus = attacker.GetRemainingTimeAtSkip() * config_.remainingTimeLockBonus;
    float totalLockTime = (baseLock * lockComboMult * lockBreakMult) + remainLockBonus;

    // ロック適用
    ApplyLockToDefender(defender, summary, totalLockTime);

    attacker.ClearPendingAttack();
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
        // 行ロック：未ロックの行からランダム
        bool foundRow = false;
        for (int attempt = 0; attempt < boardSize; attempt++) {
            int r = KashipanEngine::GetRandomInt(0, boardSize - 1);
            if (!defender.IsRowLocked(r)) {
                defender.ApplyLock(true, r, lockTime);
                foundRow = true;
                break;
            }
        }
        if (!foundRow) {
            // 全行ロック済み → 既存に加算済み
        }

        // 列ロック：未ロックの列からランダム
        bool foundCol = false;
        for (int attempt = 0; attempt < boardSize; attempt++) {
            int c = KashipanEngine::GetRandomInt(0, boardSize - 1);
            if (!defender.IsColLocked(c)) {
                defender.ApplyLock(false, c, lockTime);
                foundCol = true;
                break;
            }
        }
    }

    // ノーマル/ストレート/スクエア → 行or列1つをロック
    int otherLocks = summary.normalCount + summary.straightCount + summary.squareCount;
    for (int i = 0; i < otherLocks; i++) {
        bool isRow = KashipanEngine::GetRandomBool(0.5f);
        bool found = false;
        for (int attempt = 0; attempt < boardSize * 2; attempt++) {
            int idx = KashipanEngine::GetRandomInt(0, boardSize - 1);
            if (isRow && !defender.IsRowLocked(idx)) {
                defender.ApplyLock(true, idx, lockTime);
                found = true;
                break;
            }
            if (!isRow && !defender.IsColLocked(idx)) {
                defender.ApplyLock(false, idx, lockTime);
                found = true;
                break;
            }
            isRow = !isRow;
        }
    }
}

void TestScene::CheckWinCondition() {
    if (gameOver_) return;

    if (player1_.IsDead()) {
        gameOver_ = true;
        winner_ = 2;
        if (resultText_) resultText_->SetText("Player 2 Wins!");
    } else if (player2_.IsDead()) {
        gameOver_ = true;
        winner_ = 1;
        if (resultText_) resultText_->SetText("Player 1 Wins!");
    }
}

// ================================================================
// OnUpdate
// ================================================================

void TestScene::OnUpdate() {
    float deltaTime = GetDeltaTime();

    // シーン遷移処理
    if (auto *ic = GetInputCommand()) {
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("MenuScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
    }

    if (!GetNextSceneName().empty()) {
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }

    if (!gameOver_) {
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

        // 攻撃処理
        ProcessAttack(player1_, player2_);
        ProcessAttack(player2_, player1_);

        // 勝敗判定
        CheckWinCondition();
    }

#if defined(USE_IMGUI)
    ImGui::Begin("Puzzle Game Config");

    ImGui::Text("=== Battle Status ===");
    ImGui::Text("P1 HP: %d / %d", player1_.GetHP(), config_.playerHP);
    ImGui::Text("P2 HP: %d / %d", player2_.GetHP(), config_.playerHP);
    ImGui::Text("P1 Timer: %.1f", player1_.GetTimer());
    ImGui::Text("P2 Timer: %.1f", player2_.GetTimer());
    const char *phaseNames[] = { "Idle", "Moving", "Clearing", "Filling" };
    ImGui::Text("P1 Phase: %s", phaseNames[static_cast<int>(player1_.GetPhase())]);
    ImGui::Text("P2 Phase: %s", phaseNames[static_cast<int>(player2_.GetPhase())]);
    ImGui::Text("P1 Combo: %d", player1_.GetCombo().GetCurrentCombo());
    ImGui::Text("P2 Combo: %d", player2_.GetCombo().GetCurrentCombo());

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
    ImGui::SliderInt("Player HP", &config_.playerHP, 1, 500);
    ImGui::SliderFloat("Time Limit", &config_.timeLimit, 1.0f, 60.0f);
    ImGui::SliderInt("Normal Damage", &config_.normalDamage, 0, 50);
    ImGui::SliderInt("Straight Damage", &config_.straightDamage, 0, 50);
    ImGui::SliderInt("Cross Damage", &config_.crossDamage, 0, 50);
    ImGui::SliderInt("Square Damage", &config_.squareDamage, 0, 50);
    ImGui::SliderFloat("Normal Lock Time", &config_.normalLockTime, 0.0f, 30.0f);
    ImGui::SliderFloat("Straight Lock Time", &config_.straightLockTime, 0.0f, 30.0f);
    ImGui::SliderFloat("Cross Lock Time", &config_.crossLockTime, 0.0f, 30.0f);
    ImGui::SliderFloat("Square Lock Time", &config_.squareLockTime, 0.0f, 30.0f);
    ImGui::SliderFloat("Combo Damage Mult", &config_.comboDamageMultiplier, 1.0f, 10.0f);
    ImGui::SliderFloat("Break Damage Mult", &config_.breakDamageMultiplier, 1.0f, 10.0f);
    ImGui::SliderFloat("Combo Lock Mult", &config_.comboLockMultiplier, 1.0f, 10.0f);
    ImGui::SliderFloat("Break Lock Mult", &config_.breakLockMultiplier, 1.0f, 10.0f);
    ImGui::SliderFloat("Remain Time Dmg Bonus", &config_.remainingTimeDamageBonus, 0.0f, 5.0f);
    ImGui::SliderFloat("Remain Time Lock Bonus", &config_.remainingTimeLockBonus, 0.0f, 5.0f);

    ImGui::Separator();
    ImGui::Text("Colors");
    for (int i = 0; i < std::min(config_.panelTypeCount, Application::PuzzleGameConfig::kMaxPanelTypes); i++) {
        std::string label = "Panel Color " + std::to_string(i + 1);
        ImGui::ColorEdit4(label.c_str(), &config_.panelColors[i].x);
    }
    ImGui::ColorEdit4("Stage BG Color", &config_.stageBackgroundColor.x);
    ImGui::ColorEdit4("Cursor Color", &config_.cursorColor.x);
    ImGui::ColorEdit4("Lock Color", &config_.lockColor.x);

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

    ImGui::End();
#endif
}

} // namespace KashipanEngine
