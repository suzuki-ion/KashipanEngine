//#pragma once
//#include <KashipanEngine.h>
//#include "PlayerDrection.h"
//#include "PlayerMove.h"
//
//namespace KashipanEngine {
//
//    /// 矢印キー4方向の入力で指定距離を移動するコンポーネント
//    class PlayerBombMaking final : public IObjectComponent3D {
//    public:
//        explicit PlayerBombMaking(float moveDistance = 2.0f, float moveDuration = 1.0f)
//            : IObjectComponent3D("PlayerArrowMove", 1)
//            , moveDistance_(moveDistance)
//            , moveDuration_(moveDuration) {}
//
//        ~PlayerBombMaking() override = default;
//
//        std::unique_ptr<IObjectComponent> Clone() const override {
//            auto ptr = std::make_unique<PlayerBombMaking>(moveDistance_, moveDuration_);
//            ptr->input_ = input_;
//            ptr->moveTimer_ = moveTimer_;
//            ptr->startPosition_ = startPosition_;
//            ptr->targetPosition_ = targetPosition_;
//            return ptr;
//        }
//
//        std::optional<bool> Update() override {
//            const auto& keyboard = input_->GetKeyboard();
//            const float dt = std::max(0.0f, GetDeltaTime());
//
//            
//
//            return true;
//        }
//
//        /// @brief 爆弾設置を試みる
//        void TryPlaceBomb() {
//            // 設置上限数チェック
//            if (GetBombCount() >= maxBombs_) {
//                return;
//            }
//
//            // BPM進行度のチェック（タイミングが合っているか）
//            float distanceFromBeat = std::min(bpmProgress_, 1.0f - bpmProgress_);
//            if (distanceFromBeat > bpmToleranceRange_) {
//                return; // タイミングが合っていない
//            }
//
//            // PlayerMoveコンポーネントから向きを取得
//            auto* playerMove = GetOwner3DContext()->GetComponent<PlayerMove>();
//            if (!playerMove) {
//                return;
//            }
//
//            PlayerDirection direction = playerMove->GetPlayerDirection();
//
//            // プレイヤーの現在位置を取得
//            auto* transform = GetOwner3DContext()->GetComponent<Transform3D>();
//            if (!transform) {
//                return;
//            }
//
//            Vector3 currentPos = transform->GetTranslate();
//
//            // 爆弾の設置位置を計算
//            Vector3 bombPosition = CalculateBombPosition(currentPos, direction);
//
//            // マップ座標に変換してチェック
//            int mapX, mapZ;
//            WorldToMapCoordinates(bombPosition, mapX, mapZ);
//
//            // マップの外には設置できない
//            if (!IsValidMapPosition(mapX, mapZ)) {
//                return;
//            }
//        }
//
//        bool IsMakingBomb() const {
//            return isMaking_;
//		}
//
//        /// @brief BPM進行度の設定
//        void SetBPMProgress(float bpmProgress) {
//            bpmProgress_ = bpmProgress;
//        }
//
//        /// @brief BPMの許容範囲の設定
//        void SetBPMToleranceRange(float range) {
//            bpmToleranceRange_ = range;
//        }
//
//		/// @brief プレイヤーの向きを設定
//        void SetPlayerDirection(PlayerDirection direction) {
//            playerDirection_ = direction;
//		}
//
//        /// @brief 入力システムの設定
//        void SetInput(const Input* input) { input_ = input; }
//
//#if defined(USE_IMGUI)
//        void ShowImGui() override {
//            ImGui::TextUnformatted("PlayerArrowMove");
//            ImGui::DragFloat("Move Distance", &moveDistance_, 0.1f, 0.1f, 100.0f);
//            ImGui::DragFloat("Move Duration", &moveDuration_, 0.01f, 0.01f, 10.0f);
//        }
//#endif
//
//    private:
//        float moveDistance_ = 2.0f;    // 1回の移動距離
//        float moveDuration_ = 1.0f;    // 移動にかける時間（秒）
//        float bpmProgress_ = 0.0f;     // BPMに同期した進行度（0.0～1.0）
//        float bpmToleranceRange_ = 0.25f; // BPM進行度の許容範囲
//
//        int mapW_ = 13;
//        int mapH_ = 13;
//
//        bool isMaking_ = false;        // 移動中フラグ
//        float moveTimer_ = 0.0f;       // 移動タイマー
//        Vector3 startPosition_{ 0.0f, 0.0f, 0.0f };   // 移動開始位置
//        Vector3 targetPosition_{ 0.0f, 0.0f, 0.0f };  // 移動目標位置
//
//        const Input* input_ = nullptr;
//
//        PlayerDirection playerDirection_ = PlayerDirection::Down;
//    };
//
//} // namespace KashipanEngine