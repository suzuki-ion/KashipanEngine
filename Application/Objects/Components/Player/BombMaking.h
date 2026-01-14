#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <memory>

namespace KashipanEngine {

    // 前方宣言
    class SceneBase;
    class TestScene;

    /// BPMに合わせてZキーで爆弾を設置するコンポーネント
    class BombMaking final : public IObjectComponent3D {
    public:
        /// @brief コンストラクタ
        /// @param maxBombs 設置可能な最大爆弾数
        explicit BombMaking(int maxBombs = 3)
            : IObjectComponent3D("BombMaking", 1)
            , maxBombs_(maxBombs) {}

        ~BombMaking() override = default;

        std::unique_ptr<IObjectComponent> Clone() const override {
            auto ptr = std::make_unique<BombMaking>(maxBombs_);
            ptr->input_ = input_;
            ptr->bpmProgress_ = bpmProgress_;
            ptr->bpmToleranceRange_ = bpmToleranceRange_;
            ptr->bombOffset_ = bombOffset_;
            ptr->screenBuffer_ = screenBuffer_;
            ptr->shadowMapBuffer_ = shadowMapBuffer_;
            ptr->scene_ = scene_;
            return ptr;
        }

        std::optional<bool> Update() override {
            if (!input_) return true;

            const auto& keyboard = input_->GetKeyboard();

            // BPMの許容範囲内でZキーが押されたかチェック
            if (bpmProgress_ <= 0.0f + bpmToleranceRange_ || bpmProgress_ >= 1.0f - bpmToleranceRange_) {
                if (keyboard.IsTrigger(Key::Z)) {
                    TryPlaceBomb();
                }
            }

            return true;
        }

        /// @brief 爆弾を設置する試み
        void TryPlaceBomb() {
            // 最大数チェック
            if (static_cast<int>(bombObjects_.size()) >= maxBombs_) {
                return; // 最大数に達している場合は設置しない
            }

            auto* ctx = GetOwner3DContext();
            if (!ctx) return;

            auto* transform = ctx->GetComponent<Transform3D>();
            if (!transform) return;

            // プレイヤーの現在位置と向きを取得
            Vector3 playerPos = transform->GetTranslate();
            Vector3 playerRot = transform->GetRotate();

            // 向いている方向に基づいて爆弾の位置を計算
            Vector3 bombPos = playerPos + CalculateBombOffset(playerRot);

            // 爆弾オブジェクトを作成
            CreateBomb(bombPos);
        }

        /// @brief プレイヤーの向きから爆弾設置位置のオフセットを計算
        Vector3 CalculateBombOffset(const Vector3& rotation) const {
            // Y軸の回転から方向を判定（PlayerMove.hの実装に基づく）
            float yRot = rotation.y;
            
            // 各方向の回転値
            const float upRot = 3.14f;      // Up
            const float downRot = 0.0f;     // Down
            const float leftRot = 1.57f;    // Left
            const float rightRot = -1.57f;  // Right

            const float tolerance = 0.1f;

            if (std::abs(yRot - upRot) < tolerance) {
                // Up: Z+ 方向
                return Vector3{ 0.0f, 0.0f, bombOffset_ };
            } else if (std::abs(yRot - downRot) < tolerance) {
                // Down: Z- 方向
                return Vector3{ 0.0f, 0.0f, -bombOffset_ };
            } else if (std::abs(yRot - leftRot) < tolerance) {
                // Left: X- 方向
                return Vector3{ -bombOffset_, 0.0f, 0.0f };
            } else if (std::abs(yRot - rightRot) < tolerance || std::abs(yRot + rightRot) < tolerance) {
                // Right: X+ 方向
                return Vector3{ bombOffset_, 0.0f, 0.0f };
            }

            // デフォルト: 前方（Down方向）
            return Vector3{ 0.0f, 0.0f, -bombOffset_ };
        }

        /// @brief 爆弾オブジェクトを作成
        void CreateBomb(const Vector3& position) {
            if (!scene_) return;

            // bomb.objモデルを読み込んで爆弾オブジェクトを作成
            auto modelData = ModelManager::GetModelDataFromFileName("bomb.obj");
            auto bombObj = std::make_unique<Model>(modelData);
            bombObj->SetName("Bomb_" + std::to_string(bombObjects_.size()));

            // 位置を設定
            if (auto* tr = bombObj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(position);
                tr->SetScale(Vector3(bombScale_));
            }

            // レンダラーに登録
            if (screenBuffer_) {
                bombObj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            }
            if (shadowMapBuffer_) {
                bombObj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
            }

            // 爆弾オブジェクトのポインタを保持
            Object3DBase* bombPtr = bombObj.get();
            bombObjects_.push_back(bombPtr);

            // TestSceneのAddBombObjectメソッドを使用してシーンに追加
            auto* testScene = dynamic_cast<TestScene*>(scene_);
            if (testScene) {
                testScene->AddBombObject(std::move(bombObj));
            }
        }

        /// @brief 爆弾を削除（爆発時などに呼ばれる想定）
        /// @param index 削除する爆弾のインデックス
        void RemoveBomb(size_t index) {
            if (index < bombObjects_.size()) {
                // TODO: シーンからオブジェクトを削除する処理を追加
                // 現在はリストから削除するのみ
                bombObjects_.erase(bombObjects_.begin() + index);
            }
        }

        /// @brief すべての爆弾をクリア
        void ClearAllBombs() {
            bombObjects_.clear();
        }

        /// @brief 現在設置されている爆弾の数を取得
        int GetBombCount() const {
            return static_cast<int>(bombObjects_.size());
        }

        /// @brief 設置可能な最大爆弾数を設定
        void SetMaxBombs(int maxBombs) {
            maxBombs_ = maxBombs;
        }

        /// @brief 設置可能な最大爆弾数を取得
        int GetMaxBombs() const {
            return maxBombs_;
        }

        /// @brief 爆弾設置位置のオフセット距離を設定
        void SetBombOffset(float offset) {
            bombOffset_ = offset;
        }

        /// @brief 爆弾のスケールを設定
        void SetBombScale(float scale) {
            bombScale_ = scale;
        }

        /// @brief BPM進行度の設定（外部から更新される）
        void SetBPMProgress(float bpmProgress) {
            bpmProgress_ = bpmProgress;
        }

        /// @brief BPMの許容範囲の設定
        void SetBPMToleranceRange(float range) {
            bpmToleranceRange_ = range;
        }

        /// @brief 入力システムの設定
        void SetInput(const Input* input) {
            input_ = input;
        }

        /// @brief シーンとレンダーバッファの設定
        void SetScene(SceneBase* scene, ScreenBuffer* screenBuffer, ShadowMapBuffer* shadowMapBuffer) {
            scene_ = scene;
            screenBuffer_ = screenBuffer;
            shadowMapBuffer_ = shadowMapBuffer;
        }

        /// @brief 設置されている爆弾オブジェクトのリストを取得
        const std::vector<Object3DBase*>& GetBombObjects() const {
            return bombObjects_;
        }

#if defined(USE_IMGUI)
        void ShowImGui() override {
            ImGui::TextUnformatted("BombMaking");
            ImGui::DragInt("Max Bombs", &maxBombs_, 1.0f, 1, 10);
            ImGui::DragFloat("Bomb Offset", &bombOffset_, 0.1f, 0.0f, 10.0f);
            ImGui::DragFloat("Bomb Scale", &bombScale_, 0.1f, 0.1f, 5.0f);
            ImGui::Text("Current Bombs: %d / %d", GetBombCount(), maxBombs_);
            ImGui::DragFloat("BPM Tolerance", &bpmToleranceRange_, 0.01f, 0.0f, 0.5f);
            
            if (ImGui::Button("Clear All Bombs")) {
                ClearAllBombs();
            }
        }
#endif

    private:
        int maxBombs_ = 3;                      // 設置可能な最大爆弾数
        float bombOffset_ = 2.0f;               // 爆弾設置位置のオフセット距離
        float bombScale_ = 1.0f;                // 爆弾のスケール
        float bpmProgress_ = 0.0f;              // BPMに同期した進行度（0.0～1.0）
        float bpmToleranceRange_ = 0.25f;       // BPM進行度の許容範囲
        const Input* input_ = nullptr;          // 入力システムへのポインタ
        SceneBase* scene_ = nullptr;            // シーンへのポインタ
        ScreenBuffer* screenBuffer_ = nullptr;  // スクリーンバッファへのポインタ
        ShadowMapBuffer* shadowMapBuffer_ = nullptr; // シャドウマップバッファへのポインタ
        std::vector<Object3DBase*> bombObjects_; // 設置されている爆弾オブジェクトのリスト
    };

} // namespace KashipanEngine
