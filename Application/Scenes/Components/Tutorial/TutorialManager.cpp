#include "Scenes/Components/Tutorial/TutorialManager.h"
#include "Scenes/Components/BackMonitor.h"
#include "Scenes/Components/BackMonitorWithGameScreen.h"
#include "Scenes/Components/BackMonitorWithMenuScreen.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/StageLighting.h"
#include "Objects/Components/Player/PlayerMove.h"
#include "Objects/Components/Bomb/BombManager.h"
#include "Scenes/Components/BPM/BPMSystem.h"
#include "Objects/Components/Bomb/ExplosionManager.h"

namespace KashipanEngine {

TutorialManager::TutorialManager(InputCommand* inputCommand,
    const Vector3& gameTargetPos,
    const Vector3& gameTargetRot,
    const Vector3& menuTargetPos,
    const Vector3& menuTargetRot)
    : TutorialBase(inputCommand, gameTargetPos, gameTargetRot, menuTargetPos, menuTargetRot) {}

void TutorialManager::Initialize() {
    InitializeInternal();
    currentTutorial_ = TutorialType::Movement;
    currentPhase_ = TutorialPhase::Initial;
    moveCount_ = 0;
    bombCount_ = 0;
    explosionCount_ = 0;
    beatBombCount_ = 0;
    explosionMoveCount_ = 0;
    isTrackingMovement_ = false;
    bombPlaced_ = false;
    isInputBlocked_ = false;
    inputBlockTimer_ = 0.0f;

    // 拍に合わせた爆弾設置の監視用変数を初期化
    lastBeatBombCount_ = 0;
    wasInBPMRange_ = false;

    // 爆発移動の監視用変数を初期化
    wasKnockedBack_ = false;

    // 移動チュートリアルの監視用変数を初期化
    wasMoving_ = false;

    // チュートリアルモデルを初期化
    InitializeTutorialModels();
}

void TutorialManager::InitializeTutorialModels() {
    if (tutorialModelsInitialized_ || !monitorScreenBuffer_) {
        return;
    }

    auto* ctx = GetOwnerContext();
    if (!ctx) {
        return;
    }

    // turtrial_1.obj ～ turtrial_4.obj を読み込み
    std::string modelNames[kTutorialModelCount] = {
        "turtrial_1.obj",
        "turtrial_2.obj",
        "turtrial_3.obj",
        "turtrial_4.obj"
    };

    float z[kTutorialModelCount] = {
        2.27f,
1.95f,
2.11f,
2.35f,
    };

    for (int i = 0; i < kTutorialModelCount; ++i) {
        auto modelData = ModelManager::GetModelDataFromFileName(modelNames[i]);
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("TutorialModel_" + std::to_string(i + 1));

        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 0.0f, z[i]));
            tr->SetScale(Vector3({ 1.0f ,1.0f ,0.1f }));
        }

        if (auto* mt = obj->GetComponent3D<Material3D>()) {
            mt->SetEnableLighting(false);
			mt->SetEnableShadowMapProjection(false);
            // 初期状態では非表示（アルファ0）
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.0f));
        }

        // モニターのScreenBufferにアタッチ
        obj->AttachToRenderer(GetMonitorScreenBuffer(), "Object3D.Solid.BlendNormal");

        tutorialModels_[i] = obj.get();
        ctx->AddObject3D(std::move(obj));
    }

    tutorialModelsInitialized_ = true;
}

void TutorialManager::UpdateTutorialModelDisplay() {
    if (!tutorialModelsInitialized_) {
        return;
    }

    // すべてのモデルを非表示
    HideAllTutorialModels();

    // チュートリアル完了後は何も表示しない
    if (currentPhase_ == TutorialPhase::Finished) {
        return;
    }

    // 現在のチュートリアルに対応するモデルを表示
    int modelIndex = static_cast<int>(currentTutorial_);
    if (modelIndex >= 0 && modelIndex < kTutorialModelCount && tutorialModels_[modelIndex]) {
        if (auto* mt = tutorialModels_[modelIndex]->GetComponent3D<Material3D>()) {
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 1.0f));
        }
    }
}

void TutorialManager::HideAllTutorialModels() {
    for (int i = 0; i < kTutorialModelCount; ++i) {
        if (tutorialModels_[i]) {
            if (auto* mt = tutorialModels_[i]->GetComponent3D<Material3D>()) {
                mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.0f));
            }
        }
    }
}

void TutorialManager::Update() {
    // 入力ブロック中の場合、タイマーを更新
    if (isInputBlocked_) {
        inputBlockTimer_ += GetDeltaTime();
        if (inputBlockTimer_ >= inputBlockDuration_) {
            isInputBlocked_ = false;
            inputBlockTimer_ = 0.0f;
        }
    }

    // 待機中の場合
    if (isWaiting_) {
        waitTimer_ += GetDeltaTime();
        if (waitTimer_ >= waitDuration_) {
            isWaiting_ = false;
            waitTimer_ = 0.0f;
        }
        return;
    }

    // 全チュートリアル完了の場合
    if (currentPhase_ == TutorialPhase::Finished) {
        return;
    }

    switch (currentPhase_) {
    case TutorialPhase::Initial:
        // 新しいチュートリアルが始まる前にカウンターをリセット
        moveCount_ = 0;
        bombCount_ = 0;
        explosionCount_ = 0;
        beatBombCount_ = 0;
        explosionMoveCount_ = 0;
        bombPlaced_ = false;
        isTrackingMovement_ = false;
        
        // 監視用変数もリセット
        lastBeatBombCount_ = 0;
        wasInBPMRange_ = false;
        wasKnockedBack_ = false;
        
        // 現在の移動状態を反映（移動完了の誤検出を防ぐ）
        if (playerMove_) {
            wasMoving_ = playerMove_->IsMoving();
        } else {
            wasMoving_ = false;
        }
        
        // BombManagerの現在の爆弾数を取得
        if (bombManager_) {
            lastActiveBombCount_ = bombManager_->GetActiveBombCount();
            lastBeatBombCount_ = bombManager_->GetActiveBombCount();
        }
        
        // モニターに説明を表示開始
        StartMonitorText();
        // チュートリアルモデルの表示を更新
        UpdateTutorialModelDisplay();
        currentPhase_ = TutorialPhase::ShowInstruction;
        break;

    case TutorialPhase::ShowInstruction:
        // 確認ボタン待ちへ遷移する前に入力をブロック
        // （確認ボタンの入力が爆弾設置などに伝わらないようにする）
        isInputBlocked_ = true;
        inputBlockTimer_ = 0.0f;
        currentPhase_ = TutorialPhase::WaitForConfirm;
        break;

    case TutorialPhase::WaitForConfirm:
        // ユーザーが確認ボタンを押すまで待つ（入力は常にブロック中）
        if (IsSubmit()) {
            // ステージに視点を向けて実践開始
            StartTutorial();
            currentPhase_ = TutorialPhase::Practicing;
            
            // 入力ブロックタイマーをリセット（Practicingフェーズ開始後に入力を許可）
            inputBlockTimer_ = 0.0f;
            
            // 移動追跡の初期化
            if (currentTutorial_ == TutorialType::Movement && playerMove_) {
                isTrackingMovement_ = true;
            }
            
            // 爆弾数追跡の初期化
            if (currentTutorial_ == TutorialType::BombPlaceAndDetonate && bombManager_) {
                lastActiveBombCount_ = bombManager_->GetActiveBombCount();
                bombPlaced_ = false;
            }
        }
        break;

    case TutorialPhase::Practicing:
        // 実践中の更新
        UpdatePracticing();
        
        // 完了条件チェック
        if (IsCurrentTutorialComplete()) {
            currentPhase_ = TutorialPhase::Completed;
            isTrackingMovement_ = false;
        }
        break;

    case TutorialPhase::Completed:
        // 完了メッセージを表示してから次へ
        StartMonitorText();
        
        // 短い待機後に次のチュートリアルへ
        isWaiting_ = true;
        waitTimer_ = 0.0f;
        
        AdvanceToNextTutorial();
        break;

    case TutorialPhase::Finished:
        // 何もしない（コールバックは AdvanceToNextTutorial で呼び出し済み）
        break;
    }
}

void TutorialManager::StartTutorials() {
    currentTutorial_ = TutorialType::Movement;
    currentPhase_ = TutorialPhase::Initial;
    // カウンターやフラグはInitialフェーズでリセットされる
    
    isInputBlocked_ = false;
    inputBlockTimer_ = 0.0f;

    // チュートリアルモデルの表示を更新
    UpdateTutorialModelDisplay();
}

void TutorialManager::SkipAllTutorials() {
    currentPhase_ = TutorialPhase::Finished;
    QuitTutorial();
    
    // すべてのチュートリアルモデルを非表示
    HideAllTutorialModels();
    
    if (onAllTutorialsCompleted_) {
        onAllTutorialsCompleted_();
    }
}

void TutorialManager::AdvanceToNextTutorial() {
    // 次のチュートリアルへ
    int nextIndex = static_cast<int>(currentTutorial_) + 1;
    
    if (nextIndex >= static_cast<int>(TutorialType::Count)) {
        // 全チュートリアル完了
        currentPhase_ = TutorialPhase::Finished;
        QuitTutorial();
        
        // すべてのチュートリアルモデルを非表示
        HideAllTutorialModels();
        
        if (onAllTutorialsCompleted_) {
            onAllTutorialsCompleted_();
        }
    } else {
        // チュートリアル完了後のリセット処理
        ResetTutorialEnvironment();
        
        // 次のチュートリアルへ
        currentTutorial_ = static_cast<TutorialType>(nextIndex);
        currentPhase_ = TutorialPhase::Initial;
        // カウンターはInitialフェーズでリセットされる
    }
}

void TutorialManager::ResetTutorialEnvironment() {
    // 全爆弾を消去
    if (bombManager_) {
        bombManager_->ClearAllBombs();
    }
    
    // 全壁を消去
    if (explosionManager_) {
        explosionManager_->ClearAllWalls();
    }
    
    // プレイヤーを(4,4)の位置に配置
    if (player_) {
        auto* transform = player_->GetComponent3D<Transform3D>();
        if (transform) {
            transform->SetTranslate(Vector3(8.0f, 0.0f, 8.0f));
        }
    }
}

void TutorialManager::UpdatePracticing() {
    switch (currentTutorial_) {
    case TutorialType::Movement:
        UpdateMovementPractice();
        break;
    case TutorialType::BombPlaceAndDetonate:
        UpdateBombPlaceAndDetonatePractice();
        break;
    case TutorialType::BeatBombPlace:
        UpdateBeatBombPlacePractice();
        break;
    case TutorialType::ExplosionMove:
        UpdateExplosionMovePractice();
        break;
    default:
        break;
    }
}

void TutorialManager::UpdateMovementPractice() {
    if (!playerMove_) return;

    // PlayerMoveが移動を完了したらカウント増加
    // 移動が完了した瞬間を検出するために、IsMoving()の変化を監視
    bool isMoving = playerMove_->IsMoving();
    
    if (wasMoving_ && !isMoving) {
        // 移動が完了した
        moveCount_++;
    }
    
    wasMoving_ = isMoving;
}

void TutorialManager::UpdateBombPlaceAndDetonatePractice() {
    if (!bombManager_) return;

    int currentBombCount = bombManager_->GetActiveBombCount();
    
    // 爆弾が設置されたか確認
    if (!bombPlaced_ && currentBombCount > lastActiveBombCount_) {
        bombPlaced_ = true;
        bombCount_++;
    }
    
    // 爆弾が設置された後、起爆されたか確認（爆弾数が減少）
    if (bombPlaced_ && currentBombCount < lastActiveBombCount_) {
        explosionCount_++;
        bombPlaced_ = false;  // リセットして次の設置→起爆を待つ
    }
    
    lastActiveBombCount_ = currentBombCount;
}

void TutorialManager::UpdateBeatBombPlacePractice() {
    // BPMSystemから拍に合わせた入力を検出
    if (!bpmSystem_ || !bombManager_) return;

    // BPMの許容範囲内かどうかをチェック
    float bpmProgress = bpmSystem_->GetBeatProgress();
    float bpmToleranceRange = 0.2f;  // 許容範囲（BombManagerと同じ値）
    bool withinBPMRange = (bpmProgress <= 0.0f + bpmToleranceRange || bpmProgress >= 1.0f - bpmToleranceRange);

    int currentBombCount = bombManager_->GetActiveBombCount();

    // BPM許容範囲内で爆弾が増えた場合、拍に合わせた設置成功
    if (withinBPMRange && currentBombCount > lastBeatBombCount_) {
        beatBombCount_++;
    }

    lastBeatBombCount_ = currentBombCount;
    wasInBPMRange_ = withinBPMRange;
}

void TutorialManager::UpdateExplosionMovePractice() {
    // PlayerMoveが爆発による移動を行ったかを検出
    if (!playerMove_) return;

    // 吹き飛び状態の変化を監視
    bool isKnockedBack = playerMove_->IsKnockedBack();

    // 吹き飛び状態に入った瞬間を検出
    if (!wasKnockedBack_ && isKnockedBack) {
        explosionMoveCount_++;
    }

    wasKnockedBack_ = isKnockedBack;
}

bool TutorialManager::IsCurrentTutorialComplete() const {
    return GetCurrentCount() >= GetRequiredCount();
}

int TutorialManager::GetRequiredCount() const {
    switch (currentTutorial_) {
    case TutorialType::Movement:
        return requiredMoves_;
    case TutorialType::BombPlaceAndDetonate:
        return requiredBombDetonations_;
    case TutorialType::BeatBombPlace:
        return requiredBeatBombs_;
    case TutorialType::ExplosionMove:
        return requiredExplosionMoves_;
    default:
        return 0;
    }
}

int TutorialManager::GetCurrentCount() const {
    switch (currentTutorial_) {
    case TutorialType::Movement:
        return moveCount_;
    case TutorialType::BombPlaceAndDetonate:
        // 設置と起爆の両方が完了した回数をカウント
        return explosionCount_;
    case TutorialType::BeatBombPlace:
        return beatBombCount_;
    case TutorialType::ExplosionMove:
        return explosionMoveCount_;
    default:
        return 0;
    }
}

} // namespace KashipanEngine
