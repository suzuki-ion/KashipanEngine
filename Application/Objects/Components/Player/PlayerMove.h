#pragma once
#include <KashipanEngine.h>
#include "PlayerDrection.h"
#include "Objects/Components/Bomb/BombManager.h"

#include "Utilities/Easing.h"

namespace KashipanEngine {

    /// 矢印キー4方向の入力で指定距離を移動するコンポーネント
    class PlayerMove final : public IObjectComponent3D {
    public:
        explicit PlayerMove(float moveDistance = 2.0f, float moveDuration = 1.0f)
            : IObjectComponent3D("PlayerArrowMove", 1)
            , moveDistance_(moveDistance)
            , moveDuration_(moveDuration) {}

        ~PlayerMove() override = default;

        std::unique_ptr<IObjectComponent> Clone() const override {
            auto ptr = std::make_unique<PlayerMove>(moveDistance_, moveDuration_);
			ptr->inputCommand_ = inputCommand_;
            ptr->isMoving_ = isMoving_;
            ptr->moveTimer_ = moveTimer_;
            ptr->startPosition_ = startPosition_;
            ptr->targetPosition_ = targetPosition_;
            return ptr;
        }

        std::optional<bool> Update() override {
           
            // 移動していない場合、キー入力をチェック
            moveDirection_ = Vector3{ 0.0f, 0.0f, 0.0f };
            triggered_ = false;

            if (inputCommand_->Evaluate("ModeChange").Triggered()) {
                if (useToleranceRange_) {
                    useToleranceRange_ = false;
                } else {
                    useToleranceRange_ = true;
                }
            }

			// BPM進行度に応じて移動入力をチェック
            if (!isMoving_) {
                if (useToleranceRange_) {
                    if (bpmProgress_ <= 0.0f + bpmToleranceRange_ || bpmProgress_ >= 1.0f - bpmToleranceRange_) {
                        JudgeMove();
                    }
                } else {
                    JudgeMove();
                }
            }

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
#if defined(USE_IMGUI)
        void ShowImGui() override {
            ImGui::TextUnformatted("PlayerArrowMove");
            ImGui::DragFloat("Move Distance", &moveDistance_, 0.1f, 0.1f, 100.0f);
            ImGui::DragFloat("Move Duration", &moveDuration_, 0.01f, 0.01f, 10.0f);
            ImGui::Text("Is Moving: %s", isMoving_ ? "Yes" : "No");
            if (isMoving_) {
                ImGui::Text("Progress: %.1f%%", (moveTimer_ / moveDuration_) * 100.0f);
            }
        }
#endif
    private:

		/// @brief 移動入力のチェック
        void JudgeMove() {
            if (inputCommand_->Evaluate("MoveUp").Triggered()) {
                moveDirection_ = Vector3{ 0.0f, 0.0f, moveDistance_ };
                playerDirection_ = PlayerDirection::Up;
                triggered_ = true;
            } else if (inputCommand_->Evaluate("MoveDown").Triggered()) {
                moveDirection_ = Vector3{ 0.0f, 0.0f, -moveDistance_ };
                playerDirection_ = PlayerDirection::Down;
                triggered_ = true;
            } else if (inputCommand_->Evaluate("MoveLeft").Triggered()) {
                moveDirection_ = Vector3{ -moveDistance_, 0.0f, 0.0f };
                playerDirection_ = PlayerDirection::Left;
                triggered_ = true;
            } else if (inputCommand_->Evaluate("MoveRight").Triggered()) {
                moveDirection_ = Vector3{ moveDistance_, 0.0f, 0.0f };
                playerDirection_ = PlayerDirection::Right;
                triggered_ = true;
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

            // 移動先にBombがあるなら移動しない
            if (bombManager_ && bombManager_->IsBombAtPosition(targetPosition_)) {
                return false;
            }

            // マップ外への移動かチェック
            isOutOfBounds_ = false;
            if (targetPosition_.x < 0.0f || targetPosition_.x > static_cast<float>(mapW_ * 2 - 2) ||
                targetPosition_.z < 0.0f || targetPosition_.z > static_cast<float>(mapH_ * 2 - 2)) {
                isOutOfBounds_ = true;
                // マップ外の場合、targetPositionは元の位置に戻す
                targetPosition_ = startPosition_;

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

                    switch (playerDirection_)
                    {
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
                }
            }

            // 移動完了チェック
            if (t >= 1.0f) {
                isMoving_ = false;
                moveTimer_ = 0.0f;
            }
        };
    private:
		PlayerDirection playerDirection_ = PlayerDirection::Down; // プレイヤーの向き

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

        const InputCommand* inputCommand_ = nullptr;
        BombManager* bombManager_ = nullptr;
    };

} // namespace KashipanEngine