#pragma once
#include <KashipanEngine.h>
#include "PlayerDrection.h"
#include "Objects/Components/Bomb/BombManager.h"
#include "Objects/Components/Bomb/ExplosionManager.h"
#include "Objects/Components/Map/WallInfo.h"

#include "Utilities/Easing.h"

namespace KashipanEngine {

    /// 矢印キー4方向の入力で指定距離を移動するコンポーネント
    class PlayerMove final : public IObjectComponent3D {
    public:
        explicit PlayerMove(float moveDistance = 2.0f, float moveDuration = 1.0f)
            : IObjectComponent3D("PlayerArrowMove", 1)
            , moveDistance_(moveDistance)
            , moveDuration_(moveDuration) {
            moveInputTimer_.Start(moveInputInterval_, false);
        }

        ~PlayerMove() override = default;

        std::unique_ptr<IObjectComponent> Clone() const override {
            auto ptr = std::make_unique<PlayerMove>(moveDistance_, moveDuration_);
			ptr->inputCommand_ = inputCommand_;
            ptr->isMoving_ = isMoving_;
            ptr->moveTimer_ = moveTimer_;
            ptr->startPosition_ = startPosition_;
            ptr->targetPosition_ = targetPosition_;
            ptr->isRotating_ = isRotating_;
            ptr->rotationTimer_ = rotationTimer_;
            ptr->startRotationY_ = startRotationY_;
            ptr->hasBufferedInput_ = hasBufferedInput_;
            ptr->bufferedDirection_ = bufferedDirection_;
            ptr->bufferedPlayerDirection_ = bufferedPlayerDirection_;
            ptr->isKnockedBack_ = isKnockedBack_;
            ptr->knockbackStartPosition_ = knockbackStartPosition_;
            ptr->knockbackTargetPosition_ = knockbackTargetPosition_;
            ptr->knockbackTimer_ = knockbackTimer_;
            ptr->enableDestructiveKnockback_ = enableDestructiveKnockback_;
            ptr->knockbackVelocity_ = knockbackVelocity_;
            ptr->knockbackRotation_ = knockbackRotation_;
            ptr->enemyManager_ = enemyManager_;
            ptr->walls_ = walls_;
            ptr->wallsWidth_ = wallsWidth_;
            ptr->wallsHeight_ = wallsHeight_;
            return ptr;
        }

        std::optional<bool> Update() override {
            // 吹き飛び中の処理
            if (isKnockedBack_) {
                UpdateKnockback();
                return true;
            }

            // 移動していない場合、キー入力をチェック
            moveDirection_ = Vector3{ 0.0f, 0.0f, 0.0f };
            triggered_ = false;

			moveInputTimer_.Update();

            if (inputCommand_->Evaluate("ModeChange").Triggered()) {
                if (useToleranceRange_) {
                    useToleranceRange_ = false;
                } else {
                    useToleranceRange_ = true;
                }
            }

			// BPM進行度に応じて移動入力をチェック
            JudgeMove(true);

			// 移動処理開始
            if (triggered_) {
                OnTheMove();
            }

            // 移動中の処理
            if (isMoving_) {
                IsMove();

                return true;
            }

            return true;
        }

        /// @brief 移動距離の設定
        void SetMoveDistance(float distance) { moveDistance_ = distance; }

        /// @brief 移動距離の取得
        float GetMoveDistance() const { return moveDistance_; }

        /// @brief 移動時間の設定
        void SetMoveDuration(float duration) { moveDuration_ = duration; }

        /// @brief 移動時間の取得
        float GetMoveDuration() const { return moveDuration_; }

        /// @brief 移動中かどうかを取得
        bool IsMoving() const { return isMoving_; }

		/// @brief BPM進行度の設定
        void SetBPMProgress(float bpmProgress) { bpmProgress_ = bpmProgress; }

		/// @brief BPMの許容範囲の設定
        void SetBPMToleranceRange(float range) { bpmToleranceRange_ = range; }

		/// @brief マップサイズの設定
        void SetMapSize(int width, int height) { mapW_ = width; mapH_ = height; }

		/// @brief 入力コマンドシステムの設定
		void SetInputCommand(const InputCommand* inputCommand) { inputCommand_ = inputCommand; }
    
        /// @brief プレイヤーの向きを取得
        PlayerDirection GetPlayerDirection() const { return playerDirection_; }

		/// @brief BombManagerの設定
        void SetBombManager(BombManager* bombManager) { bombManager_ = bombManager; }

        /// @brief ExplosionManagerの設定
        void SetExplosionManager(ExplosionManager* explosionManager) { explosionManager_ = explosionManager; }

		/// @brief ゲーム開始フラグの設定
        void SetIsStarted(bool start) { isStarted_ = start; }

		/// @brief Bombでの移動停止判定の設定
		void SetIsMoveBombStop(bool f) { isMoveBombStop_ = f; }

		/// @brief 移動入力のインターバル設定
        void SetMoveInputInterval(float interval) { moveInputInterval_ = interval; }

        /// @brief 入力バッファ受付開始タイミングの設定（移動完了までの残り時間の割合、0.0～1.0）
        void SetInputBufferThreshold(float threshold) { inputBufferThreshold_ = threshold; }

        /// @brief プレイヤーを吹き飛ばす
        /// @param explosionCenter 爆発の中心位置
        void KnockBack(const Vector3& explosionCenter) {
            // 既に吹き飛び中なら無視
            if (isKnockedBack_) {
                return;
            }

            auto* ctx = GetOwner3DContext();
            if (!ctx) {
                return;
            }

            auto* transform = ctx->GetComponent<Transform3D>();
            if (!transform) {
                return;
            }

            // 現在の位置を取得してグリッド座標にスナップ
            Vector3 currentPos = transform->GetTranslate();
            currentPos.y = 0.0f;
            
            // 現在位置をグリッド座標に変換
            int currentGridX = static_cast<int>(std::round(currentPos.x / 2.0f));
            int currentGridZ = static_cast<int>(std::round(currentPos.z / 2.0f));
            
            // グリッド座標から正確なワールド座標を計算
            Vector3 snappedCurrentPos = Vector3{
                static_cast<float>(currentGridX * 2),
                0.0f,
                static_cast<float>(currentGridZ * 2)
            };

            // 爆発の中心をグリッド座標にスナップ
            int explosionGridX = static_cast<int>(std::round(explosionCenter.x / 2.0f));
            int explosionGridZ = static_cast<int>(std::round(explosionCenter.z / 2.0f));
            
            Vector3 snappedExplosionCenter = Vector3{
                static_cast<float>(explosionGridX * 2),
                0.0f,
                static_cast<float>(explosionGridZ * 2)
            };

            // 爆発の中心からプレイヤーへのグリッド方向を計算
            int deltaGridX = currentGridX - explosionGridX;
            int deltaGridZ = currentGridZ - explosionGridZ;
            
            Vector3 knockbackDirection{ 0.0f, 0.0f, 0.0f };
            
            // グリッド上の方向を4方向にスナップ
            if (deltaGridX == 0 && deltaGridZ == 0) {
                // 完全に同じ位置の場合は、プレイヤーの向きの逆方向を使用
                switch (playerDirection_) {
                case PlayerDirection::Up:
                    knockbackDirection = Vector3{ 0.0f, 0.0f, 1.0f };
                    break;
                case PlayerDirection::Down:
                    knockbackDirection = Vector3{ 0.0f, 0.0f, -1.0f };
                    break;
                case PlayerDirection::Left:
                    knockbackDirection = Vector3{ -1.0f, 0.0f, 0.0f };
                    break;
                case PlayerDirection::Right:
                    knockbackDirection = Vector3{ 1.0f, 0.0f, 0.0f };
                    break;
                }
            } else {
                // X軸とZ軸のどちらが強いかを判定（グリッド座標での距離）
                int absDeltaX = std::abs(deltaGridX);
                int absDeltaZ = std::abs(deltaGridZ);
                
                if (absDeltaX > absDeltaZ) {
                    // X軸方向に吹き飛ぶ（左右）
                    if (deltaGridX > 0) {
                        knockbackDirection = Vector3{ 1.0f, 0.0f, 0.0f };  // 右
                    } else {
                        knockbackDirection = Vector3{ -1.0f, 0.0f, 0.0f }; // 左
                    }
                } else {
                    // Z軸方向に吹き飛ぶ（上下）
                    if (deltaGridZ > 0) {
                        knockbackDirection = Vector3{ 0.0f, 0.0f, 1.0f };  // 上
                    } else {
                        knockbackDirection = Vector3{ 0.0f, 0.0f, -1.0f }; // 下
                    }
                }
            }

            // 破壊的ノックバックが有効な場合
            if (enableDestructiveKnockback_) {
                // 敵と同じように連続的に飛んでいく
                isKnockedBack_ = true;
                knockbackStartPosition_ = snappedCurrentPos;
                knockbackVelocity_ = knockbackDirection * knockbackSpeed_;
                knockbackTimer_ = 0.0f;
                knockbackRotation_ = 0.0f;
            } else {
                // 従来の1マス吹き飛びモード
                // 1マス分（moveDistance_）吹き飛ぶ（グリッド座標で計算）
                knockbackStartPosition_ = snappedCurrentPos;
                Vector3 targetPos = snappedCurrentPos + (knockbackDirection * moveDistance_);
                
                // 目標位置のグリッド座標を計算
                int targetGridX = static_cast<int>(std::round(targetPos.x / 2.0f));
                int targetGridZ = static_cast<int>(std::round(targetPos.z / 2.0f));
                
                // マップ範囲外への吹き飛びをクランプ（グリッド座標で）
                targetGridX = std::clamp(targetGridX, 0, mapW_ - 1);
                targetGridZ = std::clamp(targetGridZ, 0, mapH_ - 1);
                
                // グリッド座標から正確なワールド座標を計算
                knockbackTargetPosition_ = Vector3{
                    static_cast<float>(targetGridX * 2),
                    0.0f,
                    static_cast<float>(targetGridZ * 2)
                };

                // 吹き飛び先に壁があるかチェック
                if (explosionManager_ && explosionManager_->IsWallActiveOrMoving(knockbackTargetPosition_)) {
                    // 壁がある場合は吹き飛ばない
                    return;
                }

                // 吹き飛び先にボムがあるかチェック
                if (isMoveBombStop_ && bombManager_ && bombManager_->IsBombAtPosition(knockbackTargetPosition_)) {
                    // ボムがある場合は吹き飛ばない
                    return;
                }

                // 吹き飛び状態に設定
                isKnockedBack_ = true;
                knockbackTimer_ = 0.0f;
            }

            // 移動中なら中断
            if (isMoving_) {
                isMoving_ = false;
                moveTimer_ = 0.0f;
                hasBufferedInput_ = false;
            }
        }

        /// @brief 吹き飛び中かどうかを取得
        bool IsKnockedBack() const { return isKnockedBack_; }

        /// @brief 破壊的ノックバックの有効/無効を設定
        /// @param enable trueで有効、falseで無効（従来の1マス吹き飛び）
        void SetEnableDestructiveKnockback(bool enable) { enableDestructiveKnockback_ = enable; }

        /// @brief 破壊的ノックバックが有効かどうかを取得
        bool IsDestructiveKnockbackEnabled() const { return enableDestructiveKnockback_; }

        /// @brief EnemyManagerを設定（破壊的ノックバック用）
        void SetEnemyManager(class EnemyManager* enemyManager) { enemyManager_ = enemyManager; }

        /// @brief 壁情報を設定（破壊的ノックバック用）
        void SetWalls(WallInfo* walls, int width, int height) {
            walls_ = walls;
            wallsWidth_ = width;
            wallsHeight_ = height;
        }

#if defined(USE_IMGUI)
        void ShowImGui() override {
            ImGui::TextUnformatted("PlayerArrowMove");
            ImGui::DragFloat("Move Distance", &moveDistance_, 0.1f, 0.1f, 100.0f);
            ImGui::DragFloat("Move Duration", &moveDuration_, 0.01f, 0.01f, 10.0f);
            ImGui::Text("Is Moving: %s", isMoving_ ? "Yes" : "No");
            if (isMoving_) {
                ImGui::Text("Progress: %.1f%%", (moveTimer_ / moveDuration_) * 100.0f);
            }
            ImGui::Text("Has Buffered Input: %s", hasBufferedInput_ ? "Yes" : "No");
            ImGui::DragFloat("Input Buffer Threshold", &inputBufferThreshold_, 0.01f, 0.0f, 1.0f);
        }
#endif
    private:

        void PlayMoveSound(bool f) {
            if (f) {
                auto handle = AudioManager::GetSoundHandleFromAssetPath("Application/Audio/InGame/playerJump.mp3");
                if (handle == AudioManager::kInvalidSoundHandle) {
                    // 音声が未ロードならログ出力するか無視（ここでは無害に戻す）
                    return;
                }
                AudioManager::Play(handle, moveVolume_);
            } else {
                auto handle = AudioManager::GetSoundHandleFromAssetPath("Application/Audio/InGame/beatMiss.mp3");
                if (handle == AudioManager::kInvalidSoundHandle) {
                    // 音声が未ロードならログ出力するか無視（ここでは無害に戻す）
                    return;
                }
                AudioManager::Play(handle, missVolume_);
            }
		}

		/// @brief 吹き飛び処理
        void UpdateKnockback() {
            if (enableDestructiveKnockback_) {
                // 破壊的ノックバック：敵と同じように連続的に飛んでいく
                const float dt = GetDeltaTime();
                
                // 現在の位置を保存
                auto* ctx = GetOwner3DContext();
                if (!ctx) {
                    isKnockedBack_ = false;
                    return;
                }

                auto* transform = ctx->GetComponent<Transform3D>();
                if (!transform) {
                    isKnockedBack_ = false;
                    return;
                }

                Vector3 oldPosition = transform->GetTranslate();
                oldPosition.y = 0.0f;
                
                // 吹き飛び速度を適用
                Vector3 newPosition = oldPosition + knockbackVelocity_ * dt;
                
                // 吹き飛び中のグリッド座標を取得
                const int currentGridX = static_cast<int>(std::round(newPosition.x / 2.0f));
                const int currentGridZ = static_cast<int>(std::round(newPosition.z / 2.0f));
                
                // 壁があれば破壊
                if (walls_ && currentGridX >= 0 && currentGridX < wallsWidth_ && 
                    currentGridZ >= 0 && currentGridZ < wallsHeight_) {
                    const int index = currentGridZ * wallsWidth_ + currentGridX;
                    if (walls_[index].isActive || walls_[index].isMoving) {
                        walls_[index].isActive = false;
                        walls_[index].isMoving = false;
                        walls_[index].hp = 0;
                        walls_[index].moveTimer.Reset();
                    }
                }
                
                // 敵との衝突をチェックして破壊
                if (enemyManager_) {
                    auto enemyPositions = enemyManager_->GetActiveEnemyPositions();
                    const auto& activeEnemies = enemyManager_->GetActiveEnemies();
                    for (size_t i = 0; i < enemyPositions.size(); ++i) {
                        const Vector3& enemyPos = enemyPositions[i];
                        // 現在のグリッド位置と敵のグリッド位置を比較
                        int enemyGridX = static_cast<int>(std::round(enemyPos.x / 2.0f));
                        int enemyGridZ = static_cast<int>(std::round(enemyPos.z / 2.0f));
                        
                        if (currentGridX == enemyGridX && currentGridZ == enemyGridZ) {
                            // 敵を破壊（爆発扱いでダメージ）
                            if (i < activeEnemies.size() && activeEnemies[i].object) {
                                enemyManager_->OnExplosionHit(activeEnemies[i].object, newPosition);
                            }
                        }
                    }
                }
                
                // 回転アニメーション
                knockbackRotation_ += 12.56f * dt;
                
                // 高さの計算（ジャンプ）
                knockbackTimer_ += dt;
                float jumpHeight = 0.5f;
                
                // Transform3Dに反映
                newPosition.y = jumpHeight;
                transform->SetTranslate(newPosition);
                transform->SetRotate(Vector3{ 0.0f, knockbackRotation_, 0.0f });
                
                // マップ範囲外チェック（端に到達したら停止）
                const float minX = -2.0f;
                const float minZ = -2.0f;
                const float maxX = static_cast<float>((mapW_) * 2.0f);
                const float maxZ = static_cast<float>((mapH_) * 2.0f);
                
                if (newPosition.x <= minX || newPosition.x >= maxX ||
                    newPosition.z <= minZ || newPosition.z >= maxZ) {
                    // 端に到達したので停止
                    // グリッド位置にスナップ
                    int finalGridX = std::clamp(currentGridX, 0, mapW_ - 1);
                    int finalGridZ = std::clamp(currentGridZ, 0, mapH_ - 1);
                    Vector3 finalPos = Vector3{
                        static_cast<float>(finalGridX * 2),
                        0.0f,
                        static_cast<float>(finalGridZ * 2)
                    };
                    transform->SetTranslate(finalPos);
                    
                    // 向きをリセット
                    switch (playerDirection_) {
                    case PlayerDirection::Up:
                        transform->SetRotate(Vector3{ 0.0f, 3.14f, 0.0f });
                        break;
                    case PlayerDirection::Down:
                        transform->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
                        break;
                    case PlayerDirection::Left:
                        transform->SetRotate(Vector3{ 0.0f, 1.57f, 0.0f });
                        break;
                    case PlayerDirection::Right:
                        transform->SetRotate(Vector3{ 0.0f, -1.57f, 0.0f });
                        break;
                    }
                    
                    isKnockedBack_ = false;
                    knockbackTimer_ = 0.0f;
                    knockbackRotation_ = 0.0f;
                }
            } else {
                // 従来の1マス吹き飛び処理
                knockbackTimer_ += GetDeltaTime();
                float t = std::min(1.0f, knockbackTimer_ / knockbackDuration_);

                // イージングを使って滑らかに吹き飛ぶ
                Vector3 currentPos = Vector3(MyEasing::Lerp(knockbackStartPosition_, knockbackTargetPosition_, t, EaseType::EaseOutExpo));

                // 高さの補間
                float currentPosY = float(MyEasing::Lerp_GAB(0.0f, 0.5f, t, EaseType::EaseOutCirc, EaseType::EaseInCirc));

                // Transform3Dに反映
                auto* ctx = GetOwner3DContext();
                if (ctx) {
                    auto* transform = ctx->GetComponent<Transform3D>();
                    if (transform) {
                        currentPos.y = currentPosY;
                        transform->SetTranslate(currentPos);
                    }
                }

                // 吹き飛び完了チェック
                if (t >= 1.0f) {
                    isKnockedBack_ = false;
                    knockbackTimer_ = 0.0f;
                }
            }
        }

		/// @brief 移動入力のチェック
        void JudgeMove(bool f) {
            // 移動完了間近（inputBufferThreshold_より進行している場合）かチェック
            bool canBuffer = isMoving_ && (moveTimer_ / moveDuration_) >= inputBufferThreshold_;
            
            if (inputCommand_->Evaluate("MoveUp").Triggered()) {
                if (f) {
                    if (!isMoving_ && isStarted_) {
                        // 通常の移動開始
                        moveDirection_ = Vector3{ 0.0f, 0.0f, moveDistance_ };
                        playerDirection_ = PlayerDirection::Up;
                        triggered_ = true;
                    } else if (canBuffer && isStarted_) {
                        // 移動中だが、バッファリング可能な場合
                        hasBufferedInput_ = true;
                        bufferedDirection_ = Vector3{ 0.0f, 0.0f, moveDistance_ };
                        bufferedPlayerDirection_ = PlayerDirection::Up;
                    }

                    // 拍に合わせた正常な移動成功時のみ、Chainモード中のBombの爆発サイズを増加
                    if (bombManager_ && !isOutOfBounds_) {
                        if ((bpmProgress_ <= 0.0f + bpmToleranceRange_ || bpmProgress_ >= 1.0f - bpmToleranceRange_) && moveInputTimer_.IsFinished()) {
                            //bombManager_->IncrementChainBombExplosionSize(1.0f);
                            shouldRotate_ = true; // 拍に合わせた移動の場合、回転フラグを立てる
                        }
                    }

                    moveInputTimer_.Start(moveInputInterval_, false);
                }
				PlayMoveSound(f);
            } else if (inputCommand_->Evaluate("MoveDown").Triggered()) {
                if (f) {
                    if (!isMoving_ && isStarted_) {
                        moveDirection_ = Vector3{ 0.0f, 0.0f, -moveDistance_ };
                        playerDirection_ = PlayerDirection::Down;
                        triggered_ = true;
                    } else if (canBuffer && isStarted_) {
                        hasBufferedInput_ = true;
                        bufferedDirection_ = Vector3{ 0.0f, 0.0f, -moveDistance_ };
                        bufferedPlayerDirection_ = PlayerDirection::Down;
                    }

                    // 拍に合わせた正常な移動成功時のみ、Chainモード中のBombの爆発サイズを増加
                    if (bombManager_ && !isOutOfBounds_) {
                        if ((bpmProgress_ <= 0.0f + bpmToleranceRange_ || bpmProgress_ >= 1.0f - bpmToleranceRange_) && moveInputTimer_.IsFinished()) {
                            //bombManager_->IncrementChainBombExplosionSize(1.0f);
                            shouldRotate_ = true; // 拍に合わせた移動の場合、回転フラグを立てる
                        }
                    }

                    moveInputTimer_.Start(moveInputInterval_, false);
                }
                PlayMoveSound(f);
            } else if (inputCommand_->Evaluate("MoveLeft").Triggered()) {
                if (f) {
                    if (!isMoving_ && isStarted_) {
                        moveDirection_ = Vector3{ -moveDistance_, 0.0f, 0.0f };
                        playerDirection_ = PlayerDirection::Left;
                        triggered_ = true;
                    } else if (canBuffer && isStarted_) {
                        hasBufferedInput_ = true;
                        bufferedDirection_ = Vector3{ -moveDistance_, 0.0f, 0.0f };
                        bufferedPlayerDirection_ = PlayerDirection::Left;
                    }

                    // 拍に合わせた正常な移動成功時のみ、Chainモード中のBombの爆発サイズを増加
                    if (bombManager_ && !isOutOfBounds_) {
                        if ((bpmProgress_ <= 0.0f + bpmToleranceRange_ || bpmProgress_ >= 1.0f - bpmToleranceRange_) && moveInputTimer_.IsFinished()) {
                            //bombManager_->IncrementChainBombExplosionSize(1.0f);
                            shouldRotate_ = true; // 拍に合わせた移動の場合、回転フラグを立てる
                        }
                    }

                    moveInputTimer_.Start(moveInputInterval_, false);
                }
                PlayMoveSound(f);
            } else if (inputCommand_->Evaluate("MoveRight").Triggered()) {
                if (f) {
                    if (!isMoving_ && isStarted_) {
                        moveDirection_ = Vector3{ moveDistance_, 0.0f, 0.0f };
                        playerDirection_ = PlayerDirection::Right;
                        triggered_ = true;
                    } else if (canBuffer && isStarted_) {
                        hasBufferedInput_ = true;
                        bufferedDirection_ = Vector3{ moveDistance_, 0.0f, 0.0f };
                        bufferedPlayerDirection_ = PlayerDirection::Right;
                    }

                    // 拍に合わせた正常な移動成功時のみ、Chainモード中のBombの爆発サイズを増加
                    if (bombManager_ && !isOutOfBounds_) {
                        if ((bpmProgress_ <= 0.0f + bpmToleranceRange_ || bpmProgress_ >= 1.0f - bpmToleranceRange_) && moveInputTimer_.IsFinished()) {
                            //bombManager_->IncrementChainBombExplosionSize(1.0f);
                            shouldRotate_ = true; // 拍に合わせた移動の場合、回転フラグを立てる
                        }
                    }

                    moveInputTimer_.Start(moveInputInterval_, false);
                }
                PlayMoveSound(f);
            }
        };

		/// @brief 移動処理開始
        bool OnTheMove() {
            auto* ctx = GetOwner3DContext();
            if (!ctx) {
                return false;
            }

            auto* transform = ctx->GetComponent<Transform3D>();
            if (!transform) {
                return false;
            }

            startPosition_ = transform->GetTranslate();
            targetPosition_ = startPosition_ + moveDirection_;

            if (isMoveBombStop_) {
                // 移動先にBombがあるなら移動しない
                if (bombManager_ && bombManager_->IsBombAtPosition(targetPosition_)) {
                    return false;
                }
            }

            // 移動先に壁（アクティブまたは移動中）があるなら移動しない
            if (explosionManager_ && explosionManager_->IsWallActiveOrMoving(targetPosition_)) {
                return false;
            }

            // マップ外への移動かチェック
            isOutOfBounds_ = false;
            if (targetPosition_.x < 0.0f || targetPosition_.x > static_cast<float>(mapW_ * 2 - 2) ||
                targetPosition_.z < 0.0f || targetPosition_.z > static_cast<float>(mapH_ * 2 - 2)) {
                isOutOfBounds_ = true;
                // マップ外の場合、targetPositionは元の位置に戻す
                targetPosition_ = startPosition_;
                shouldRotate_ = false; // マップ外への移動の場合は回転しない

                switch (playerDirection_){
                case PlayerDirection::Up:
					playerDirection_ = PlayerDirection::Down;
                    break;
                case PlayerDirection::Down:
					playerDirection_ = PlayerDirection::Up;
                    break;
                case PlayerDirection::Left:
					playerDirection_ = PlayerDirection::Right;
                    break;
                case PlayerDirection::Right:
					playerDirection_ = PlayerDirection::Left;
                    break;
                }
            }

            // 回転アニメーション開始
            if (shouldRotate_) {
                isRotating_ = true;
                rotationTimer_ = 0.0f;
                startRotationY_ = transform->GetRotate().y;
            }
            isMoving_ = true;
            moveTimer_ = 0.0f;
            return true;
        }

		/// @brief 移動処理
        void IsMove() {
            moveTimer_ += GetDeltaTime();
            float t = std::min(1.0f, moveTimer_ / moveDuration_);

            Vector3 currentPos;
            
            if (isOutOfBounds_) {
                // マップ外への移動の場合、Lerp_GABで一旦外に出てから戻る
                Vector3 outOfBoundsTarget = startPosition_ + moveDirection_;
                currentPos = Vector3(MyEasing::Lerp_GAB(startPosition_, outOfBoundsTarget, t));
            } else {
                // 通常の移動
                currentPos = Vector3(MyEasing::Lerp(startPosition_, targetPosition_, t, EaseType::EaseOutExpo));
            }

            float currentPosY = float(MyEasing::Lerp_GAB(0.0f, 0.5f, t, EaseType::EaseOutCirc, EaseType::EaseInCirc));

            // Transform3Dに反映
            auto* ctx = GetOwner3DContext();
            if (ctx) {
                auto* transform = ctx->GetComponent<Transform3D>();
                if (transform) {
                    currentPos.y = currentPosY;
                    transform->SetTranslate(currentPos);

                    // 回転アニメーション
                    float baseRotationY = 0.0f;
                    switch (playerDirection_)
                    {
                    case PlayerDirection::Up:
                        baseRotationY = 3.14f;
                        break;
                    case PlayerDirection::Down:
                        baseRotationY = 0.0f;
                        break;
                    case PlayerDirection::Left:
                        baseRotationY = 1.57f;
                        break;
                    case PlayerDirection::Right:
                        baseRotationY = -1.57f;
                        break;
                    }

                    // 拍に合わせた移動の場合、Y軸周りに1回転
                    if (isRotating_) {
                        rotationTimer_ += GetDeltaTime();
                        float rotationT = std::min(1.0f, rotationTimer_ / moveDuration_);
                        
                        // 1回転 (2π rad = 6.28318...) を追加
                        float additionalRotation = rotationT * 6.28318530718f;
                        transform->SetRotate(Vector3{ 0.0f, baseRotationY + additionalRotation, 0.0f });
                        
                        if (rotationT >= 1.0f) {
                            isRotating_ = false;
                            rotationTimer_ = 0.0f;
                        }
                    } else {
                        transform->SetRotate(Vector3{ 0.0f, baseRotationY, 0.0f });
                    }
                }
            }

            // 移動完了チェック
            if (t >= 1.0f) {
                isMoving_ = false;
                moveTimer_ = 0.0f;
                shouldRotate_ = false;
                
                // バッファされた入力があれば、次の移動を開始
                if (hasBufferedInput_) {
                    moveDirection_ = bufferedDirection_;
                    playerDirection_ = bufferedPlayerDirection_;
                    triggered_ = true;
                    hasBufferedInput_ = false;
                    
                    // すぐに次の移動を開始
                    OnTheMove();
                }
            }
        };

    private:
		PlayerDirection playerDirection_ = PlayerDirection::Down; // プレイヤーの向き

		bool isStarted_ = false;
		bool isMoveBombStop_ = false; // ボムのある方向に移動するかどうか 

        float moveDistance_ = 2.0f;       // 1回の移動距離
        float moveDuration_ = 1.0f;       // 移動にかける時間（秒）
		float bpmProgress_ = 0.0f;        // BPMに同期した進行度（0.0～1.0）
		float bpmToleranceRange_ = 0.25f; // BPM進行度の許容範囲
		bool useToleranceRange_ = true;   // 許容範囲を使用するかどうか

        int mapW_ = 13;
        int mapH_ = 13;

		bool triggered_ = false; // 移動入力があったかどうか
        bool isMoving_ =  false;  // 移動中フラグ
        bool isOutOfBounds_ = false; // マップ外への移動フラグ
        float moveTimer_ = 0.0f; // 移動タイマー
		Vector3 moveDirection_{ 0.0f, 0.0f, 0.0f }; // 移動方向ベクトル

        Vector3 startPosition_{ 0.0f, 0.0f, 0.0f };   // 移動開始位置
        Vector3 targetPosition_{ 0.0f, 0.0f, 0.0f };  // 移動目標位置

        // 回転アニメーション用
        bool shouldRotate_ = false;      // 回転すべきかどうか
        bool isRotating_ = false;        // 回転中フラグ
        float rotationTimer_ = 0.0f;     // 回転タイマー
        float startRotationY_ = 0.0f;    // 回転開始時のY軸回転

        // 入力バッファリング用
        bool hasBufferedInput_ = false;  // バッファされた入力があるか
        Vector3 bufferedDirection_{ 0.0f, 0.0f, 0.0f }; // バッファされた移動方向
        PlayerDirection bufferedPlayerDirection_ = PlayerDirection::Down; // バッファされたプレイヤーの向き
        float inputBufferThreshold_ = 0.7f; // 入力バッファを受け付ける移動完了までの割合（デフォルト70%）

        // 吹き飛び関連
        bool isKnockedBack_ = false;              // 吹き飛び中フラグ
        Vector3 knockbackStartPosition_{ 0.0f, 0.0f, 0.0f };  // 吹き飛び開始位置
        Vector3 knockbackTargetPosition_{ 0.0f, 0.0f, 0.0f }; // 吹き飛び目標位置
        float knockbackTimer_ = 0.0f;             // 吹き飛びタイマー
        float knockbackDuration_ = 0.3f;          // 吹き飛び時間（秒）

        // 破壊的ノックバック用の追加メンバー
        bool enableDestructiveKnockback_ = true;  // 破壊的ノックバックの有効/無効
        Vector3 knockbackVelocity_{ 0.0f, 0.0f, 0.0f };  // 吹き飛び速度
        float knockbackSpeed_ = 15.0f;  // 吹き飛び速度（敵と同じ）
        float knockbackRotation_ = 0.0f;  // 吹き飛び中の回転角度
        class EnemyManager* enemyManager_ = nullptr;  // 敵破壊用
        WallInfo* walls_ = nullptr;  // 壁破壊用
        int wallsWidth_ = 0;
        int wallsHeight_ = 0;

        const InputCommand* inputCommand_ = nullptr;
        BombManager* bombManager_ = nullptr;
        ExplosionManager* explosionManager_ = nullptr;

		GameTimer moveInputTimer_;
		float moveInputInterval_ = 0.3f;

		float moveVolume_ = 0.1f;
		float missVolume_ = 0.1f;
    };

} // namespace KashipanEngine