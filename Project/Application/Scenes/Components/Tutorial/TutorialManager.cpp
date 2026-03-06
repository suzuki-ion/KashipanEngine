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
#include "Objects/Components/BPMScaling.h"

namespace KashipanEngine {

TutorialManager::TutorialManager(InputCommand* inputCommand,
    const Vector3& gameTargetPos,
    const Vector3& gameTargetRot,
    const Vector3& menuTargetPos,
    const Vector3& menuTargetRot)
    : TutorialBase(inputCommand, gameTargetPos, gameTargetRot, menuTargetPos, menuTargetRot) {}

void TutorialManager::Initialize() {
    InitializeInternal();
    currentTutorial_ = TutorialType::UseTutorial;
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
    
    soundHandleSelect_ = AudioManager::GetSoundHandleFromFileName("select.mp3");
    soundHandleSubmit_ = AudioManager::GetSoundHandleFromFileName("submit.mp3");
    soundHandlePinpon_ = AudioManager::GetSoundHandleFromFileName("pinpon.mp3");

    // UseTutorial選択状態を初期化
    currentSelection_ = UseTutorialSelection::Yes;
    useTutorialDecided_ = false;

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
        "turtrial_0.obj",
        "turtrial_1.obj",
        "turtrial_2.obj",
        "turtrial_3.obj",
        "turtrial_4.obj",
        "tower.obj",
    };

    Vector3 t[kTutorialModelCount] = {
        Vector3(0.0f,1.35f,2.0f),
        Vector3(-0.5f,0.25f,1.77f),
        Vector3(0.0f,0.36f,1.95f),
        Vector3(-0.51f,0.1f,2.11f),
        Vector3(-0.39f,0.22f,2.35f),
         Vector3(0.0f,0.0f,2.0f),
    };

    Vector3 sStart[kTutorialModelCount] = {
        Vector3(1.2f,1.5f,0.1f),
        Vector3(0.76f,0.91f,0.1f),
        Vector3(0.9f,1.0f,0.1f),
        Vector3(1.02f,1.38f,0.1f),
        Vector3(1.0f,1.55f,0.1f),
        Vector3(0.95f,1.44f,0.1f),
        
    };

    //Vector3 sEnd[kTutorialModelCount] = {
    //    Vector3(1.3f,1.3f,0.1f),
    //    Vector3(1.0f,1.0f,0.1f),
    //    Vector3(1.0f,1.0f,0.1f),
    //    Vector3(1.0f,1.0f,0.1f),
    //    Vector3(1.0f,1.0f,0.1f),
    //    Vector3(1.0f,1.0f,0.1f),

    //};

    for (int i = 0; i < kTutorialModelCount; ++i) {
        auto modelData = ModelManager::GetModelDataFromFileName(modelNames[i]);
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("TutorialModel_" + std::to_string(i + 1));

        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(t[i]);
            tr->SetScale(sStart[i]);
        }

        if (auto* mt = obj->GetComponent3D<Material3D>()) {
            mt->SetEnableLighting(false);
			mt->SetEnableShadowMapProjection(false);
            // 初期状態では非表示（アルファ0）
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.0f));
        }

        //obj->RegisterComponent<BPMScaling>(playerScaleMin_, playerScaleMax_, EaseType::EaseOutExpo);

        // モニターのScreenBufferにアタッチ
        obj->AttachToRenderer(GetMonitorScreenBuffer(), "Object3D.Solid.BlendNormal");

        tutorialModels_[i] = obj.get();
        ctx->AddObject3D(std::move(obj));
    }

    {
        auto modelData = ModelManager::GetModelDataFromFileName("Yes.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("Yes");

        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, -1.15f, 2.0f));
            tr->SetScale(Vector3({ 0.66f ,0.66f ,0.1f }));
        }

        if (auto* mt = obj->GetComponent3D<Material3D>()) {
            mt->SetEnableLighting(false);
            mt->SetEnableShadowMapProjection(false);
            // 初期状態では非表示（アルファ0）
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.0f));
        }

        // モニターのScreenBufferにアタッチ
        obj->AttachToRenderer(GetMonitorScreenBuffer(), "Object3D.Solid.BlendNormal");

        useTutorialYes_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }

    {
        auto modelData = ModelManager::GetModelDataFromFileName("No.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("No");

        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, -2.35f, 2.0f));
            tr->SetScale(Vector3({ 0.66f ,0.66f ,0.1f }));
        }

        if (auto* mt = obj->GetComponent3D<Material3D>()) {
            mt->SetEnableLighting(false);
            mt->SetEnableShadowMapProjection(false);
            // 初期状態では非表示（アルファ0）
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.0f));
        }

        // モニターのScreenBufferにアタッチ
        obj->AttachToRenderer(GetMonitorScreenBuffer(), "Object3D.Solid.BlendNormal");

        useTutorialNo_ = obj.get();
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
    
    // UseTutorialオプションも非表示
    HideUseTutorialOptions();

    // チュートリアル完了後は何も表示しない
    if (currentPhase_ == TutorialPhase::Finished) {
        return;
    }
    
    // UseTutorialの場合は選択肢を表示
    if (currentTutorial_ == TutorialType::UseTutorial) {
        UpdateUseTutorialDisplay();
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

void TutorialManager::HideUseTutorialOptions() {
    if (useTutorialYes_) {
        if (auto* mt = useTutorialYes_->GetComponent3D<Material3D>()) {
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.0f));
        }
    }
    if (useTutorialNo_) {
        if (auto* mt = useTutorialNo_->GetComponent3D<Material3D>()) {
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.0f));
        }
    }
}

void TutorialManager::UpdateUseTutorialSelection() {
    // 左右入力で選択を変更
    if (IsMoveLeft() || IsMoveRight() || IsMoveUp() || IsMoveDown()) {
        // Yes <-> No を切り替え
        if (currentSelection_ == UseTutorialSelection::Yes) {
            currentSelection_ = UseTutorialSelection::No;
            AudioManager::Play(soundHandleSelect_, 1.0f, 0.0f, false);
        } else {
            currentSelection_ = UseTutorialSelection::Yes;
            AudioManager::Play(soundHandleSelect_, 1.0f, 0.0f, false);
        }
        
        // 表示を更新
        UpdateUseTutorialDisplay();
    }
}

void TutorialManager::UpdateUseTutorialDisplay() {
    if (!useTutorialYes_ || !useTutorialNo_) {
        return;
    }
    
    // turtrial_0.obj を表示
    if (tutorialModels_[0]) {
        if (auto* mt = tutorialModels_[0]->GetComponent3D<Material3D>()) {
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 1.0f));
        }
    }
    
    // Yesの表示
    if (auto* mt = useTutorialYes_->GetComponent3D<Material3D>()) {
        if (currentSelection_ == UseTutorialSelection::Yes) {
            // 選択中は明るく表示
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 1.0f));
        } else {
            // 選択されていない場合は暗く表示
            mt->SetColor(Vector4(0.3f, 0.3f, 0.3f, 1.0f));
        }
    }
    
    // Noの表示
    if (auto* mt = useTutorialNo_->GetComponent3D<Material3D>()) {
        if (currentSelection_ == UseTutorialSelection::No) {
            // 選択中は明るく表示
            mt->SetColor(Vector4(0.5f, 0.5f, 0.5f, 1.0f));
        } else {
            // 選択されていない場合は暗く表示
            mt->SetColor(Vector4(0.3f, 0.3f, 0.3f, 1.0f));
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
        // UseTutorialの場合は特別な処理
        if (currentTutorial_ == TutorialType::UseTutorial) {
            // 左右入力で選択を変更
            UpdateUseTutorialSelection();
            
            // ユーザーが確認ボタンを押すまで待つ
            if (IsSubmit()) {
                useTutorialDecided_ = true;
                AudioManager::Play(soundHandleSubmit_, 1.0f, 0.0f, false);
                
                if (currentSelection_ == UseTutorialSelection::Yes) {
                    // Yesが選択された場合、Movementチュートリアルに進む
                    currentTutorial_ = TutorialType::Movement;
                    currentPhase_ = TutorialPhase::Initial;
                } else {
                    // Noが選択された場合、全チュートリアルをスキップ
                    SkipAllTutorials();
                }
                
                // 入力ブロックタイマーをリセット
                isInputBlocked_ = true;
                inputBlockTimer_ = 0.0f;
            }
        } else if (currentTutorial_ == TutorialType::MissonText) {
            // MissonTextの場合は確認ボタンを押したらメニューに遷移
            if (IsSubmit()) {
                AudioManager::Play(soundHandleSubmit_, 1.0f, 0.0f, false);
                currentPhase_ = TutorialPhase::Finished;
                QuitTutorial();
                
                // すべてのチュートリアルモデルを非表示
                HideAllTutorialModels();
                HideUseTutorialOptions();
                
                if (onAllTutorialsCompleted_) {
                    onAllTutorialsCompleted_();
                }
                
                // 入力ブロックタイマーをリセット
                isInputBlocked_ = true;
                inputBlockTimer_ = 0.0f;
            }
        } else {
            // 他のチュートリアルの場合は通常処理
            // ユーザーが確認ボタンを押すまで待つ（入力は常にブロック中）
            if (IsSubmit()) {
                AudioManager::Play(soundHandleSubmit_, 1.0f, 0.0f, false);
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
                
                // 拍に合わせた爆弾設置の初期化
                if (currentTutorial_ == TutorialType::BeatBombPlace && bombManager_) {
                    lastBeatBombCount_ = bombManager_->GetActiveBombCount();
                    wasInBPMRange_ = false;
                }
                
                // 爆発移動の初期化
                if (currentTutorial_ == TutorialType::ExplosionMove && playerMove_) {
                    wasKnockedBack_ = playerMove_->IsKnockedBack();
                }
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
        
        // 1秒の待機後に次のチュートリアルへ
        isWaiting_ = true;
        waitTimer_ = 0.0f;
        waitDuration_ = completedWaitDuration_;  // 1秒の待機時間を設定
        
        AdvanceToNextTutorial();
        break;

    case TutorialPhase::Finished:
        // 何もしない（コールバックは AdvanceToNextTutorial で呼び出し済み）
        break;
    }
}

void TutorialManager::StartTutorials() {
    currentTutorial_ = TutorialType::UseTutorial;
    currentPhase_ = TutorialPhase::Initial;
    // カウンターやフラグはInitialフェーズでリセットされる
    
    // UseTutorial選択状態をリセット
    currentSelection_ = UseTutorialSelection::Yes;
    useTutorialDecided_ = false;
    
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
    HideUseTutorialOptions();
    
    if (onAllTutorialsCompleted_) {
        onAllTutorialsCompleted_();
    }
}

void TutorialManager::AdvanceToNextTutorial() {
    // 次のチュートリアルへ
    int nextIndex = static_cast<int>(currentTutorial_) + 1;
    
    // UseTutorialの次はMovementに進む（UseTutorialをスキップ）
    if (currentTutorial_ == TutorialType::UseTutorial) {
        nextIndex = static_cast<int>(TutorialType::Movement);
    }
    
    if (nextIndex >= static_cast<int>(TutorialType::Count)) {
        // 全チュートリアル完了
        currentPhase_ = TutorialPhase::Finished;
        QuitTutorial();
        
        // すべてのチュートリアルモデルを非表示
        HideAllTutorialModels();
        HideUseTutorialOptions();
        
        if (onAllTutorialsCompleted_) {
            onAllTutorialsCompleted_();
        }
    } else {
        // チュートリアル完了後のリセット処理
        ResetTutorialEnvironment();
        
        // 次のチュートリアルへ
        currentTutorial_ = static_cast<TutorialType>(nextIndex);
        currentPhase_ = TutorialPhase::Initial;
        AudioManager::Play(soundHandlePinpon_, 0.5f, 0.0f, false);
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
    case TutorialType::MissonText:
        UpdateMissonTextPractice();
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

    // BPM許容範囲外から範囲内に移行し、かつ爆弾が増えた場合のみカウント
    // これにより、チュートリアル開始時の誤検出を防ぐ
    if (bombManager_->GetActiveBombCount() == 3) {
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

void TutorialManager::UpdateMissonTextPractice() {
    // ミッションテキストは表示するだけで、ユーザーが確認ボタンを押すのを待つ
    // 確認ボタンが押されたら完了とみなす（IsCurrentTutorialComplete()で判定）
    // 何もしない（確認ボタンの押下はWaitForConfirmフェーズで処理される）
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
    case TutorialType::MissonText:
        return 0;  // ミッションテキストは実践不要
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
    case TutorialType::MissonText:
        return 0;  // ミッションテキストは実践不要
    default:
        return 0;
    }
}

} // namespace KashipanEngine
