#include "Scenes/TestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <numbers>
#include <algorithm>
#include <map>
#include <set>

namespace KashipanEngine {

// ================================================================
// パネルの基本スケール
// ================================================================
static constexpr Vector3 kPanelScale{ 2.0f, 1.0f, 2.0f };
static constexpr float   kPanelY = 1.0f; // 地面パネルの上面

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

    // ボードの初期化
    board_.Initialize(config_.stageSize);

    // カーソルの初期化（ボード中央）
    int center = config_.stageSize / 2;
    cursor_.Initialize(center, center, config_.stageSize, config_.cursorEasingDuration);

    // コンボ管理の初期化
    combo_.Initialize();

    // 3Dオブジェクトの生成
    CreateStageObjects();

    // カメラの設定
    if (auto *cam = sceneDefaultVariables_->GetMainCamera3D()) {
        float fovRad = config_.cameraFov * std::numbers::pi_v<float> / 180.0f;
        cam->SetFovY(fovRad);

        if (auto *tr = cam->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, config_.cameraHeight, config_.cameraZDistance));
            float pitch = std::atan2(config_.cameraHeight, -config_.cameraZDistance);
            tr->SetRotate(Vector3(pitch, 0.0f, 0.0f));
        }
    }

    // 全パネルの見た目を同期
    SyncAllPanelVisuals();
    UpdateCursorObject();

    phase_ = Phase::Idle;
}

// ================================================================
// 座標変換
// ================================================================

Vector3 TestScene::BoardToWorld(int row, int col) const {
    return BoardToWorld(static_cast<float>(row), static_cast<float>(col));
}

Vector3 TestScene::BoardToWorld(float row, float col) const {
    float halfN = static_cast<float>(config_.stageSize) * 0.5f;
    float x = (col - halfN + 0.5f) * 2.0f;
    float z = (row - halfN + 0.5f) * 2.0f;
    return Vector3(x, 0.0f, z);
}

// ================================================================
// ステージ生成
// ================================================================

void TestScene::CreateStageObjects() {
    int n = config_.stageSize;
    auto *screenBuffer3D = sceneDefaultVariables_->GetScreenBuffer3D();

    // 地面パネル
    groundPanels_.resize(n * n);
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            auto box = std::make_unique<Box>();
            box->SetName("Ground_" + std::to_string(r) + "_" + std::to_string(c));
            if (auto *tr = box->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(BoardToWorld(r, c));
                tr->SetScale(kPanelScale);
            }
            if (auto *mat = box->GetComponent3D<Material3D>()) {
                mat->SetColor(config_.groundColor);
                mat->SetEnableLighting(true);
            }
            if (screenBuffer3D) {
                box->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            }
            groundPanels_[r * n + c] = box.get();
            AddObject3D(std::move(box));
        }
    }

    // パズルパネル
    puzzlePanels_.resize(n * n);
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            auto box = std::make_unique<Box>();
            box->SetName("Panel_" + std::to_string(r) + "_" + std::to_string(c));
            if (auto *tr = box->GetComponent3D<Transform3D>()) {
                Vector3 pos = BoardToWorld(r, c);
                pos.y = kPanelY;
                tr->SetTranslate(pos);
                tr->SetScale(kPanelScale);
            }
            if (auto *mat = box->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(true);
            }
            if (screenBuffer3D) {
                box->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            }
            puzzlePanels_[r * n + c] = box.get();
            AddObject3D(std::move(box));
        }
    }

    // カーソル
    {
        auto box = std::make_unique<Box>();
        box->SetName("Cursor");
        if (auto *tr = box->GetComponent3D<Transform3D>()) {
            Vector3 pos = BoardToWorld(cursor_.GetPosition().first,
                                       cursor_.GetPosition().second);
            pos.y = kPanelY + 1.05f;
            tr->SetTranslate(pos);
            tr->SetScale(Vector3(2.1f, 0.1f, 2.1f));
        }
        if (auto *mat = box->GetComponent3D<Material3D>()) {
            mat->SetColor(config_.cursorColor);
            mat->SetEnableLighting(false);
        }
        if (screenBuffer3D) {
            box->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        }
        cursorObject_ = box.get();
        AddObject3D(std::move(box));
    }
}

// ================================================================
// パネル見た目ユーティリティ
// ================================================================

void TestScene::ApplyPanelColor(int row, int col) {
    int idx = row * config_.stageSize + col;
    if (idx < 0 || idx >= static_cast<int>(puzzlePanels_.size())) return;
    auto *panel = puzzlePanels_[idx];
    if (!panel) return;

    int type = board_.GetPanel(row, col);
    if (type > 0 && type <= Application::PuzzleGameConfig::kMaxPanelTypes) {
        if (auto *mat = panel->GetComponent3D<Material3D>()) {
            mat->SetColor(config_.panelColors[type - 1]);
        }
    }
}

void TestScene::SyncAllPanelVisuals() {
    int n = config_.stageSize;
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            int idx = r * n + c;
            if (idx >= static_cast<int>(puzzlePanels_.size())) continue;
            auto *panel = puzzlePanels_[idx];
            if (!panel) continue;

            ApplyPanelColor(r, c);

            if (auto *tr = panel->GetComponent3D<Transform3D>()) {
                Vector3 pos = BoardToWorld(r, c);
                pos.y = kPanelY;
                tr->SetTranslate(pos);
                tr->SetScale(kPanelScale);
            }
        }
    }
}

void TestScene::UpdateCursorObject() {
    if (!cursorObject_) return;
    auto [interpRow, interpCol] = cursor_.GetInterpolatedPosition();
    if (auto *tr = cursorObject_->GetComponent3D<Transform3D>()) {
        Vector3 pos = BoardToWorld(interpRow, interpCol);
        pos.y = kPanelY + 1.05f;
        tr->SetTranslate(pos);
    }
}

// ================================================================
// フェーズ遷移ヘルパー
// ================================================================

void TestScene::StartMoveAction(int direction) {
    auto [row, col] = cursor_.GetPosition();
    int n = config_.stageSize;

    // --- 内部データを瞬時に更新 ---
    switch (direction) {
    case 0: board_.ShiftColUp(col);    break;
    case 1: board_.ShiftColDown(col);  break;
    case 2: board_.ShiftRowLeft(row);  break;
    case 3: board_.ShiftRowRight(row); break;
    }

    // --- 色を新しいデータに合わせて即更新 ---
    // ただし位置は「移動前」に設定する（アニメーションで動かすため）
    phaseAnims_.clear();

    auto addAnim = [&](int r, int c, int fromR, int fromC) {
        int idx = r * n + c;
        auto *panel = puzzlePanels_[idx];
        if (!panel) return;

        // 色は新しい種類に更新
        ApplyPanelColor(r, c);

        // 移動前の位置
        Vector3 startP = BoardToWorld(fromR, fromC);
        startP.y = kPanelY;
        // 移動先の位置
        Vector3 endP = BoardToWorld(r, c);
        endP.y = kPanelY;

        // オブジェクトを開始位置に配置
        if (auto *tr = panel->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(startP);
            tr->SetScale(kPanelScale);
        }

        PanelAnim a;
        a.row = r;
        a.col = c;
        a.startPos = startP;
        a.endPos = endP;
        a.startScale = kPanelScale;
        a.endScale = kPanelScale;
        phaseAnims_.push_back(a);
    };

    switch (direction) {
    case 0: // 列を上にシフト（row+方向）
        for (int r = 0; r < n; r++) {
            // ループ元は画面外の下端から来る表現にする
            int visualFromR = (r == 0) ? -1 : (r - 1);
            addAnim(r, col, visualFromR, col);
        }
        break;
    case 1: // 列を下にシフト
        for (int r = 0; r < n; r++) {
            int visualFromR = (r == n - 1) ? n : (r + 1);
            addAnim(r, col, visualFromR, col);
        }
        break;
    case 2: // 行を左にシフト
        for (int c = 0; c < n; c++) {
            int visualFromC = (c == n - 1) ? n : (c + 1);
            addAnim(row, c, row, visualFromC);
        }
        break;
    case 3: // 行を右にシフト
        for (int c = 0; c < n; c++) {
            int visualFromC = (c == 0) ? -1 : (c - 1);
            addAnim(row, c, row, visualFromC);
        }
        break;
    }

    // アニメーション対象でないパネルの位置も正しく設定
    // （移動対象以外は SyncAllPanelVisuals 相当の位置に）
    std::set<int> animIndices;
    for (auto &a : phaseAnims_) animIndices.insert(a.row * n + a.col);
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            int idx = r * n + c;
            if (animIndices.count(idx)) continue;
            ApplyPanelColor(r, c);
            if (auto *panel = puzzlePanels_[idx]) {
                if (auto *tr = panel->GetComponent3D<Transform3D>()) {
                    Vector3 pos = BoardToWorld(r, c);
                    pos.y = kPanelY;
                    tr->SetTranslate(pos);
                    tr->SetScale(kPanelScale);
                }
            }
        }
    }

    phase_ = Phase::Moving;
    phaseTimer_ = 0.0f;
    phaseDuration_ = config_.panelEasingDuration;
}

void TestScene::OnMoveFinished() {
    // 全パネル位置を確定
    SyncAllPanelVisuals();

    // マッチ検出 → Clearing or Idle
    if (!StartClearingPhase()) {
        combo_.ResetCombo();
        phase_ = Phase::Idle;
    }
}

bool TestScene::StartClearingPhase() {
    pendingMatches_ = board_.DetectMatches();
    if (pendingMatches_.empty()) return false;

    // コンボ加算
    combo_.AddCombo(static_cast<int>(pendingMatches_.size()));

    int n = config_.stageSize;
    std::vector<std::vector<bool>> toBeClear(n, std::vector<bool>(n, false));
    for (const auto &m : pendingMatches_) {
        if (m.isHorizontal) {
            for (int i = 0; i < m.length; i++)
                toBeClear[m.fixedIndex][m.start + i] = true;
        } else {
            for (int i = 0; i < m.length; i++)
                toBeClear[m.start + i][m.fixedIndex] = true;
        }
    }

    phaseAnims_.clear();
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            if (!toBeClear[r][c]) continue;

            Vector3 pos = BoardToWorld(r, c);
            pos.y = kPanelY;

            Vector3 endPos = pos;
            endPos.y = kPanelY + config_.panelClearRiseHeight;

            PanelAnim a;
            a.row = r;
            a.col = c;
            a.startPos = pos;
            a.endPos = endPos;
            a.startScale = kPanelScale;
            a.endScale = Vector3(kPanelScale.x, 0.0f, kPanelScale.z); // Y を潰して消す
            phaseAnims_.push_back(a);
        }
    }

    phase_ = Phase::Clearing;
    phaseTimer_ = 0.0f;
    phaseDuration_ = config_.panelClearDuration;
    return true;
}

void TestScene::OnClearFinished() {
    // 内部データの消去＆補充（瞬時）
    board_.ClearAndFillMatches(pendingMatches_);
    // pendingMatches_ は StartFillingPhase 内で参照するためここではクリアしない

    // Filling フェーズへ
    StartFillingPhase();
}

void TestScene::StartFillingPhase() {
    // ClearAndFillMatches 後のボードは既に補充済み。
    // pendingMatches_ を参照して、横消し/縦消しの方向に応じたスライドインアニメーションを付ける。
    //
    // 仕様:
    //   横消し → 消えた分だけ左のパネルが右に移動し、左端から新パネル出現
    //   縦消し → 消えた分だけ上のパネルが下に移動し、上端から新パネル出現

    int n = config_.stageSize;

    // 消した行/列ごとの消去数を集計
    std::map<int, int> hRowClearCount; // 横消し行 → 消去パネル数
    std::map<int, int> vColClearCount; // 縦消し列 → 消去パネル数

    for (const auto &m : pendingMatches_) {
        if (m.isHorizontal) {
            hRowClearCount[m.fixedIndex] += m.length;
        } else {
            vColClearCount[m.fixedIndex] += m.length;
        }
    }
    // 重複セルがあるかもしれないので上限 clamp
    for (auto &[row, cnt] : hRowClearCount) cnt = std::min(cnt, n);
    for (auto &[col, cnt] : vColClearCount) cnt = std::min(cnt, n);

    pendingMatches_.clear();

    phaseAnims_.clear();

    // 色を全更新
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++)
            ApplyPanelColor(r, c);

    // 横消し行: 各パネルを clearCount マス分右からスライドイン
    // 仕様: 「消えた分だけ左のパネルが右に移動してきて、左から新パネルが出現」
    //   → パネル全体が右方向に clearCount マスぶん移動してくるように見える
    for (auto &[row, clearCount] : hRowClearCount) {
        for (int c = 0; c < n; c++) {
            Vector3 endP = BoardToWorld(row, c);
            endP.y = kPanelY;
            // 左側 clearCount マス分ずれた位置から来る
            Vector3 startP = BoardToWorld(row, c - clearCount);
            startP.y = kPanelY;

            PanelAnim a;
            a.row = row;
            a.col = c;
            a.startPos = startP;
            a.endPos = endP;
            a.startScale = kPanelScale;
            a.endScale = kPanelScale;
            phaseAnims_.push_back(a);

            // 開始位置にセット
            int idx = row * n + c;
            if (auto *panel = puzzlePanels_[idx]) {
                if (auto *tr = panel->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(startP);
                    tr->SetScale(kPanelScale);
                }
            }
        }
    }

    // 縦消し列: 各パネルを clearCount マス分下からスライドイン
    // 仕様: 「消えた分だけ上のパネルが下に移動してきて、上から新パネルが出現」
    //   → パネル全体が下方向に clearCount マスぶん移動してくるように見える
    std::set<int> hRowSet;
    for (auto &[row, cnt] : hRowClearCount) hRowSet.insert(row);

    for (auto &[col, clearCount] : vColClearCount) {
        for (int r = 0; r < n; r++) {
            // 横消し行と重複するセルはスキップ（横のアニメが優先）
            if (hRowSet.count(r)) continue;

            Vector3 endP = BoardToWorld(r, col);
            endP.y = kPanelY;
            // 上端 clearCount マス分ずれた位置から来る
            Vector3 startP = BoardToWorld(r + clearCount, col);
            startP.y = kPanelY;

            PanelAnim a;
            a.row = r;
            a.col = col;
            a.startPos = startP;
            a.endPos = endP;
            a.startScale = kPanelScale;
            a.endScale = kPanelScale;
            phaseAnims_.push_back(a);

            int idx = r * n + col;
            if (auto *panel = puzzlePanels_[idx]) {
                if (auto *tr = panel->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(startP);
                    tr->SetScale(kPanelScale);
                }
            }
        }
    }

    // アニメーション対象外のパネルは正規位置へ
    std::set<int> animSet;
    for (auto &a : phaseAnims_) animSet.insert(a.row * n + a.col);
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            int idx = r * n + c;
            if (animSet.count(idx)) continue;
            if (auto *panel = puzzlePanels_[idx]) {
                if (auto *tr = panel->GetComponent3D<Transform3D>()) {
                    Vector3 pos = BoardToWorld(r, c);
                    pos.y = kPanelY;
                    tr->SetTranslate(pos);
                    tr->SetScale(kPanelScale);
                }
            }
        }
    }

    if (phaseAnims_.empty()) {
        // 補充アニメーション無し → 即座に連鎖チェック
        SyncAllPanelVisuals();
        OnFillFinished();
        return;
    }

    phase_ = Phase::Filling;
    phaseTimer_ = 0.0f;
    phaseDuration_ = config_.panelEasingDuration;
}

void TestScene::OnFillFinished() {
    SyncAllPanelVisuals();

    // 連鎖マッチチェック
    if (!StartClearingPhase()) {
        combo_.ResetCombo();
        phase_ = Phase::Idle;
    }
}

// ================================================================
// フェーズ更新
// ================================================================

void TestScene::UpdatePhase(float deltaTime) {
    if (phase_ == Phase::Idle) return;

    phaseTimer_ += deltaTime;
    float t = std::clamp(phaseTimer_ / phaseDuration_, 0.0f, 1.0f);
    float easedT = Apply(t, EaseType::EaseOutCubic);

    int n = config_.stageSize;

    for (auto &a : phaseAnims_) {
        int idx = a.row * n + a.col;
        if (idx < 0 || idx >= static_cast<int>(puzzlePanels_.size())) continue;
        auto *panel = puzzlePanels_[idx];
        if (!panel) continue;

        if (auto *tr = panel->GetComponent3D<Transform3D>()) {
            Vector3 pos = Lerp(a.startPos, a.endPos, easedT);
            tr->SetTranslate(pos);

            Vector3 scale = Lerp(a.startScale, a.endScale, easedT);
            tr->SetScale(scale);
        }
    }

    // フェーズ完了判定
    if (t >= 1.0f) {
        Phase finishedPhase = phase_;
        phaseAnims_.clear();

        switch (finishedPhase) {
        case Phase::Moving:
            OnMoveFinished();
            break;
        case Phase::Clearing:
            OnClearFinished();
            break;
        case Phase::Filling:
            OnFillFinished();
            break;
        default:
            break;
        }
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

    // フェーズアニメーション更新
    UpdatePhase(deltaTime);

    // カーソル更新
    cursor_.Update(GetInputCommand(), deltaTime, IsAnimating());

    // カーソルオブジェクトの位置更新
    UpdateCursorObject();

    // 移動アクション処理（Idle 時のみ受付）
    if (cursor_.HasMoveAction() && !IsAnimating()) {
        StartMoveAction(cursor_.GetMoveActionDirection());
    }

    // カメラ設定の動的更新
    if (auto *cam = sceneDefaultVariables_->GetMainCamera3D()) {
        float fovRad = config_.cameraFov * std::numbers::pi_v<float> / 180.0f;
        cam->SetFovY(fovRad);

        if (auto *tr = cam->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, config_.cameraHeight, config_.cameraZDistance));
            float pitch = std::atan2(config_.cameraHeight, -config_.cameraZDistance);
            tr->SetRotate(Vector3(pitch, 0.0f, 0.0f));
        }
    }

#if defined(USE_IMGUI)
    ImGui::Begin("Puzzle Game Config");

    ImGui::SliderInt("Stage Size", &config_.stageSize, 3, 8);
    ImGui::SliderFloat("Panel Easing Duration", &config_.panelEasingDuration, 0.01f, 1.0f);
    ImGui::SliderFloat("Cursor Easing Duration", &config_.cursorEasingDuration, 0.01f, 1.0f);
    ImGui::SliderFloat("Camera Height", &config_.cameraHeight, 5.0f, 50.0f);
    ImGui::SliderFloat("Camera Z Distance", &config_.cameraZDistance, -30.0f, 0.0f);
    ImGui::SliderFloat("Camera FOV", &config_.cameraFov, 10.0f, 120.0f);
    ImGui::SliderFloat("Panel Clear Rise Height", &config_.panelClearRiseHeight, 0.1f, 5.0f);
    ImGui::SliderFloat("Panel Clear Duration", &config_.panelClearDuration, 0.1f, 2.0f);

    for (int i = 0; i < std::min(config_.stageSize, Application::PuzzleGameConfig::kMaxPanelTypes); i++) {
        std::string label = "Panel Color " + std::to_string(i + 1);
        ImGui::ColorEdit4(label.c_str(), &config_.panelColors[i].x);
    }
    ImGui::ColorEdit4("Ground Color", &config_.groundColor.x);
    ImGui::ColorEdit4("Cursor Color", &config_.cursorColor.x);

    // 色変更を即反映（Idle 時のみ安全に全同期）
    if (!IsAnimating()) {
        SyncAllPanelVisuals();
    }
    if (cursorObject_) {
        if (auto *mat = cursorObject_->GetComponent3D<Material3D>()) {
            mat->SetColor(config_.cursorColor);
        }
    }
    for (auto *ground : groundPanels_) {
        if (ground) {
            if (auto *mat = ground->GetComponent3D<Material3D>()) {
                mat->SetColor(config_.groundColor);
            }
        }
    }

    if (ImGui::Button("Save Config")) {
        config_.SaveToJSON(kConfigPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Config")) {
        config_.LoadFromJSON(kConfigPath);
    }

    ImGui::Separator();
    ImGui::Text("Combo: %d", combo_.GetCurrentCombo());
    ImGui::Text("Total Combo: %d", combo_.GetTotalCombo());

    auto [curRow, curCol] = cursor_.GetPosition();
    ImGui::Text("Cursor: (%d, %d)", curRow, curCol);

    const char *phaseNames[] = { "Idle", "Moving", "Clearing", "Filling" };
    ImGui::Text("Phase: %s", phaseNames[static_cast<int>(phase_)]);
    if (IsAnimating()) {
        ImGui::Text("Progress: %.1f%%", (phaseTimer_ / phaseDuration_) * 100.0f);
    }

    ImGui::End();
#endif
}

} // namespace KashipanEngine
