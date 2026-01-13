#pragma once
#include <KashipanEngine.h>

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
            ptr->input_ = input_;
            ptr->isMoving_ = isMoving_;
            ptr->moveTimer_ = moveTimer_;
            ptr->startPosition_ = startPosition_;
            ptr->targetPosition_ = targetPosition_;
            return ptr;
        }

        std::optional<bool> Update() override {
            const auto& keyboard = input_->GetKeyboard();
            const float dt = std::max(0.0f, GetDeltaTime());

            // 移動中の処理
            if (isMoving_) {
                moveTimer_ += dt;
                float t = std::min(1.0f, moveTimer_ / moveDuration_);

                // 線形補間で現在位置を計算
                Vector3 currentPos = EaseInBack(startPosition_, targetPosition_, t);

                // Transform3Dに反映
                auto* ctx = GetOwner3DContext();
                if (ctx) {
                    auto* transform = ctx->GetComponent<Transform3D>();
                    if (transform) {
                        transform->SetTranslate(currentPos);
                    }
                }

                // 移動完了チェック
                if (t >= 1.0f) {
                    isMoving_ = false;
                    moveTimer_ = 0.0f;
                }

                return true;
            }

            // 移動していない場合、キー入力をチェック
            Vector3 moveDirection{ 0.0f, 0.0f, 0.0f };
            bool triggered = false;

            if (keyboard.IsTrigger(Key::Up)) {
                moveDirection = Vector3{ 0.0f, 0.0f, moveDistance_ };
                triggered = true;
            } else if (keyboard.IsTrigger(Key::Down)) {
                moveDirection = Vector3{ 0.0f, 0.0f, -moveDistance_ };
                triggered = true;
            } else if (keyboard.IsTrigger(Key::Left)) {
                moveDirection = Vector3{ -moveDistance_, 0.0f, 0.0f };
                triggered = true;
            } else if (keyboard.IsTrigger(Key::Right)) {
                moveDirection = Vector3{ moveDistance_, 0.0f, 0.0f };
                triggered = true;
            }

            if (triggered) {
                // 移動開始
                auto* ctx = GetOwner3DContext();
                if (ctx) {
                    auto* transform = ctx->GetComponent<Transform3D>();
                    if (transform) {
                        startPosition_ = transform->GetTranslate();
                        targetPosition_ = startPosition_ + moveDirection;
                        isMoving_ = true;
                        moveTimer_ = 0.0f;
                    }
                }
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

        void SetInput(const Input* input) { input_ = input; }
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
        float moveDistance_ = 2.0f;    // 1回の移動距離
        float moveDuration_ = 1.0f;    // 移動にかける時間（秒）

        bool isMoving_ = false;        // 移動中フラグ
        float moveTimer_ = 0.0f;       // 移動タイマー
        Vector3 startPosition_{ 0.0f, 0.0f, 0.0f };   // 移動開始位置
        Vector3 targetPosition_{ 0.0f, 0.0f, 0.0f };  // 移動目標位置

        const Input* input_ = nullptr;
    };

} // namespace KashipanEngine