#include "Scenes/TestScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <algorithm>
#include <cmath>

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    // シーン遷移用アニメーション
    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

    // 設定の読み込み
    config_.LoadFromJSON(kConfigPath);

    // 3Dスクリーンバッファの取得
    auto *screenBuffer3D = sceneDefaultVariables_->GetScreenBuffer3D();

    // ゲームロジックの初期化
    int n = config_.stageSize;
    puzzleBoard_.Initialize(n, config_.panelTypeCount);
    puzzleCursor_.Initialize(n / 2, n / 2, n, config_.cursorEasingDuration);
    puzzleGoal_.Generate(puzzleBoard_, config_.panelTypeCount, config_.goalMoveCount);

    panelAnim_.active = false;

    // カメラの設定
    SetupCamera();

    // 平行光源の向きを真下に設定
    if (auto *light = sceneDefaultVariables_->GetDirectionalLight()) {
        light->SetDirection(Vector3(0.0f, -1.0f, 0.0f));
    }

    // シャドウマッピング用バッファの取得
    auto *shadowMapBuffer = sceneDefaultVariables_->GetShadowMapBuffer();

    // ===== 3Dオブジェクトの生成 =====

    // 地面パネル
    groundPanels_.resize(n, std::vector<Box*>(n, nullptr));
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            auto obj = std::make_unique<Box>();
            std::string name = "Ground[" + std::to_string(r) + "][" + std::to_string(c) + "]";
            obj->SetName(name);

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(GridToWorld(r, c));
                tr->SetScale(Vector3(2.0f, 1.0f, 2.0f));
            }
            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetColor(config_.groundColor);
                mat->SetEnableLighting(true);
            }
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");

            groundPanels_[r][c] = obj.get();
            AddObject3D(std::move(obj));
        }
    }

    // パズルパネル
    puzzlePanels_.resize(n, std::vector<Box*>(n, nullptr));
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            auto obj = std::make_unique<Box>();
            std::string name = "Puzzle[" + std::to_string(r) + "][" + std::to_string(c) + "]";
            obj->SetName(name);

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                Vector3 pos = GridToWorld(r, c);
                pos.y = 1.0f; // 地面の上
                tr->SetTranslate(pos);
                tr->SetScale(Vector3(1.9f, 0.5f, 1.9f)); // 地面より少し小さく
            }
            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(true);
            }
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");

            puzzlePanels_[r][c] = obj.get();
            AddObject3D(std::move(obj));
        }
    }

    // カーソル
    {
        auto obj = std::make_unique<Box>();
        obj->SetName("Cursor");

        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            auto [cr, cc] = puzzleCursor_.GetPosition();
            Vector3 pos = GridToWorld(cr, cc);
            pos.y = 1.6f;
            tr->SetTranslate(pos);
            tr->SetScale(Vector3(2.1f, 0.2f, 2.1f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(config_.cursorColor);
            mat->SetEnableLighting(false);
        }
        obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");

        cursorBox_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // 目標パネル地面 (3x3)
    goalGroundPanels_.resize(3, std::vector<Box*>(3, nullptr));
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            auto obj = std::make_unique<Box>();
            std::string name = "GoalGround[" + std::to_string(r) + "][" + std::to_string(c) + "]";
            obj->SetName(name);

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(GoalGridToWorld(r, c));
                tr->SetScale(Vector3(2.0f, 1.0f, 2.0f));
            }
            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetColor(config_.groundColor);
                mat->SetEnableLighting(true);
            }
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");

            goalGroundPanels_[r][c] = obj.get();
            AddObject3D(std::move(obj));
        }
    }

    // 目標パネル (3x3)
    goalPanels_.resize(3, std::vector<Box*>(3, nullptr));
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            auto obj = std::make_unique<Box>();
            std::string name = "GoalPanel[" + std::to_string(r) + "][" + std::to_string(c) + "]";
            obj->SetName(name);

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                Vector3 pos = GoalGridToWorld(r, c);
                pos.y = 1.0f;
                tr->SetTranslate(pos);
                tr->SetScale(Vector3(1.9f, 0.5f, 1.9f));
            }
            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(true);
            }
            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");

            goalPanels_[r][c] = obj.get();
            AddObject3D(std::move(obj));
        }
    }

    // 遮光オブジェクト（ステージ真上に配置、中央3x3だけ穴が空いている）
    {
        int center = n / 2;
        float roofHeight = 2.0f;
        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                // 中央3x3はスキップ（光を通す穴）
                if (r >= center - 1 && r <= center + 1 && c >= center - 1 && c <= center + 1) {
                    continue;
                }

                auto obj = std::make_unique<Box>();
                std::string name = "Roof[" + std::to_string(r) + "][" + std::to_string(c) + "]";
                obj->SetName(name);

                if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                    Vector3 pos = GridToWorld(r, c);
                    pos.y = roofHeight;
                    tr->SetTranslate(pos);
                    tr->SetScale(Vector3(2.0f, 0.2f, 2.0f));
                }
                if (auto *mat = obj->GetComponent3D<Material3D>()) {
                    mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 1.0f));
                    mat->SetEnableLighting(false);
                }
                // 画面には描画しないが、シャドウマップには書き込む
                obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");

                roofPanels_.push_back(obj.get());
                AddObject3D(std::move(obj));
            }
        }
    }

    // 色の初期設定
    UpdatePanelColors();
    UpdateGoalPanelColors();
}

TestScene::~TestScene() {
    // 設定の保存
    config_.SaveToJSON(kConfigPath);
}

void TestScene::OnUpdate() {
    float dt = std::max(0.0f, GetDeltaTime());

    // パネルアニメーション中の処理
    if (panelAnim_.active) {
        panelAnim_.timer += dt;
        float t = std::clamp(panelAnim_.timer / config_.panelEasingDuration, 0.0f, 1.0f);
        float easedT = Apply(t, EaseType::EaseOutCubic);

        // 移動するパネルの描画位置を更新
        // toRow,toCol のパネル（移動先=元の空きマス）の見た目を from→to でイージング
        Box* movingPanel = puzzlePanels_[panelAnim_.toRow][panelAnim_.toCol];
        if (movingPanel) {
            if (auto *tr = movingPanel->GetComponent3D<Transform3D>()) {
                Vector3 fromPos = GridToWorld(panelAnim_.fromRow, panelAnim_.fromCol);
                Vector3 toPos = GridToWorld(panelAnim_.toRow, panelAnim_.toCol);
                fromPos.y = 1.0f;
                toPos.y = 1.0f;
                Vector3 currentPos = Eased(fromPos, toPos, easedT, EaseType::EaseOutCubic);
                tr->SetTranslate(currentPos);
            }
        }

        if (t >= 1.0f) {
            panelAnim_.active = false;

            // 目標達成チェック
            if (puzzleGoal_.IsGoalReached(puzzleBoard_)) {
                // 新しい目標を生成
                puzzleGoal_.Generate(puzzleBoard_, config_.panelTypeCount, config_.goalMoveCount);
                UpdateGoalPanelColors();
            }
        }
    }

    // カーソルの更新（パネルアニメーション中でもカーソルは動かせる）
    puzzleCursor_.Update(GetInputCommand(), dt);

    // 移動アクション（Space/Aボタン）
    if (!panelAnim_.active && !puzzleCursor_.IsMoving()) {
        if (auto *ic = GetInputCommand()) {
            if (ic->Evaluate("PuzzleAction").Triggered()) {
                auto [cr, cc] = puzzleCursor_.GetPosition();
                // 選択しているパネルの上下左右に空きがあるか
                if (puzzleBoard_.HasAdjacentEmpty(cr, cc) && puzzleBoard_.GetPanel(cr, cc) != 0) {
                    auto [er, ec] = puzzleBoard_.GetEmptyPos();

                    // パネルを移動（内部的には瞬時）
                    puzzleBoard_.TryMovePanel(cr, cc);

                    // オブジェクトポインタの入れ替え
                    // cr,cc → er,ec にパネルが移動
                    std::swap(puzzlePanels_[cr][cc], puzzlePanels_[er][ec]);

                    // パネルアニメーション開始
                    panelAnim_.fromRow = cr;
                    panelAnim_.fromCol = cc;
                    panelAnim_.toRow = er;
                    panelAnim_.toCol = ec;
                    panelAnim_.timer = 0.0f;
                    panelAnim_.active = true;

                    // 色の更新
                    UpdatePanelColors();
                }
            }
        }
    }

    // カーソルの描画位置を更新
    if (cursorBox_) {
        if (auto *tr = cursorBox_->GetComponent3D<Transform3D>()) {
            auto [interpR, interpC] = puzzleCursor_.GetInterpolatedPosition();
            Vector3 pos = GridToWorld(0, 0); // ベース
            int n = config_.stageSize;
            float halfN = static_cast<float>(n - 1) * 0.5f;
            pos.x = (interpC - halfN) * 2.0f;
            pos.z = (interpR - halfN) * 2.0f;
            pos.y = 1.6f;
            tr->SetTranslate(pos);
        }
    }

    // 地面パネルの空きマス色の更新
    {
        int n = config_.stageSize;
        auto [er, ec] = puzzleBoard_.GetEmptyPos();
        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                if (groundPanels_[r][c]) {
                    if (auto *mat = groundPanels_[r][c]->GetComponent3D<Material3D>()) {
                        if (r == er && c == ec) {
                            mat->SetColor(config_.emptyGroundColor);
                        } else {
                            mat->SetColor(config_.groundColor);
                        }
                    }
                }
            }
        }
    }

    // デバッグ用：シーン遷移
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

#ifdef USE_IMGUI
    // ImGuiでの調整
    ImGui::Begin("Puzzle Game Config");

    int stageSize = config_.stageSize;
    if (ImGui::SliderInt("Stage Size", &stageSize, 3, 11)) {
        if (stageSize % 2 == 0) stageSize++;
        if (stageSize != config_.stageSize) {
            config_.stageSize = stageSize;
        }
    }
    ImGui::SliderInt("Panel Type Count", &config_.panelTypeCount, 2, 8);
    ImGui::SliderInt("Goal Move Count", &config_.goalMoveCount, 1, 20);
    ImGui::SliderFloat("Panel Easing Duration", &config_.panelEasingDuration, 0.01f, 1.0f);
    if (ImGui::SliderFloat("Cursor Easing Duration", &config_.cursorEasingDuration, 0.01f, 1.0f)) {
        puzzleCursor_.SetEasingDuration(config_.cursorEasingDuration);
    }
    if (ImGui::SliderFloat("Camera Height", &config_.cameraHeight, 1.0f, 30.0f)) {
        SetupCamera();
    }
    if (ImGui::SliderFloat("Camera Z Distance", &config_.cameraZDistance, -20.0f, 0.0f)) {
        SetupCamera();
    }
    if (ImGui::SliderFloat("Camera FOV", &config_.cameraFov, 10.0f, 120.0f)) {
        SetupCamera();
    }
    ImGui::DragFloat3("Goal Offset", &config_.goalPanelOffset.x, 0.1f);
    ImGui::ColorEdit4("Ground Color", &config_.groundColor.x);
    ImGui::ColorEdit4("Empty Ground Color", &config_.emptyGroundColor.x);
    ImGui::ColorEdit4("Cursor Color", &config_.cursorColor.x);

    for (int i = 0; i < config_.panelTypeCount; i++) {
        std::string label = "Panel Color " + std::to_string(i + 1);
        ImGui::ColorEdit4(label.c_str(), &config_.panelColors[i].x);
    }

    if (ImGui::Button("Save Config")) {
        config_.SaveToJSON(kConfigPath);
    }

    ImGui::End();
#endif
}

Vector3 TestScene::GridToWorld(int row, int col) const {
    int n = config_.stageSize;
    float halfN = static_cast<float>(n - 1) * 0.5f;
    float x = (static_cast<float>(col) - halfN) * 2.0f;
    float z = (static_cast<float>(row) - halfN) * 2.0f;
    return Vector3(x, 0.0f, z);
}

Vector3 TestScene::GoalGridToWorld(int row, int col) const {
    Vector3 base = config_.goalPanelOffset;
    float x = base.x + (static_cast<float>(col) - 1.0f) * 2.0f;
    float z = base.z + (static_cast<float>(row) - 1.0f) * 2.0f;
    return Vector3(x, base.y, z);
}

void TestScene::UpdatePanelColors() {
    int n = config_.stageSize;
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            Box* panel = puzzlePanels_[r][c];
            if (!panel) continue;

            int type = puzzleBoard_.GetPanel(r, c);
            if (auto *mat = panel->GetComponent3D<Material3D>()) {
                if (type == 0) {
                    // 空きマス：非表示（透明にする）
                    mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
                } else if (type >= 1 && type <= 8) {
                    mat->SetColor(config_.panelColors[type - 1]);
                }
            }

            // 位置のリセット（アニメーション中でないパネル）
            if (!panelAnim_.active || !(panelAnim_.toRow == r && panelAnim_.toCol == c)) {
                if (auto *tr = panel->GetComponent3D<Transform3D>()) {
                    Vector3 pos = GridToWorld(r, c);
                    pos.y = 1.0f;
                    tr->SetTranslate(pos);
                }
            }
        }
    }
}

void TestScene::UpdateGoalPanelColors() {
    auto& goal = puzzleGoal_.GetGoalPattern();
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            Box* panel = goalPanels_[r][c];
            if (!panel) continue;

            int type = (r < static_cast<int>(goal.size()) && c < static_cast<int>(goal[r].size())) ? goal[r][c] : 0;
            if (auto *mat = panel->GetComponent3D<Material3D>()) {
                if (type == 0) {
                    mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.0f));
                } else if (type >= 1 && type <= 8) {
                    mat->SetColor(config_.panelColors[type - 1]);
                }
            }
        }
    }
}

void TestScene::SetupCamera() {
    auto *camera = sceneDefaultVariables_->GetMainCamera3D();
    if (!camera) return;

    if (auto *tr = camera->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(Vector3(0.0f, config_.cameraHeight, config_.cameraZDistance));
        // ステージ中央を見下ろすように回転を計算
        Vector3 target(0.0f, 0.0f, 0.0f);
        Vector3 eye(0.0f, config_.cameraHeight, config_.cameraZDistance);
        Vector3 dir = target - eye;
        float pitch = std::atan2(-dir.y, std::sqrt(dir.x * dir.x + dir.z * dir.z));
        float yaw = std::atan2(dir.x, dir.z);
        tr->SetRotate(Vector3(pitch, yaw, 0.0f));
    }
    camera->SetFovY(config_.cameraFov * 3.14159265f / 180.0f);
    camera->SetAspectRatio(1920.0f / 1080.0f);
    camera->SetNearClip(0.1f);
    camera->SetFarClip(100.0f);
}

} // namespace KashipanEngine