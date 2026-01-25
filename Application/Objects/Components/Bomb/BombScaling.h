#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

    /// @brief BPMに合わせてオブジェクトを拡縮させるコンポーネント
    class BombScaling final : public IObjectComponent3D {
    public:
        /// @brief コンストラクタ
        /// @param minScale 最小スケール（BPM進行度0.0のとき）
        /// @param maxScale 最大スケール（BPM進行度1.0のとき）
        explicit BombScaling(Vector3 minScale = { 0.9f,0.9f,0.9f }, Vector3 maxScale = { 1.0f,1.0f,1.0f }, EaseType easeType = EaseType::Linear)
            : IObjectComponent3D("BPMScaling", 1)
            , minScale_(minScale)
            , maxScale_(maxScale)
            , easeType_(easeType) {}

        /// @brief コンストラクタ（全軸同一スケール版）
        /// @param minScaleAll 全軸の最小スケール
        /// @param maxScaleAll 全軸の最大スケール
        explicit BombScaling(float minScaleAll, float maxScaleAll)
            : IObjectComponent3D("BPMScaling", 1)
            , minScale_(minScaleAll, minScaleAll, minScaleAll)
            , maxScale_(maxScaleAll, maxScaleAll, maxScaleAll) {}

        ~BombScaling() override = default;

        std::unique_ptr<IObjectComponent> Clone() const override {
            auto clone = std::make_unique<BombScaling>(minScale_, maxScale_);
            return clone;
        }

        std::optional<bool> Update() override;

        /// @brief BPM進行度の設定（外部から更新される）
        /// @param bpmProgress BPM進行度（0.0～1.0）
        void SetBPMProgress(float bpmProgress) { bpmProgress_ = bpmProgress; }

        void SetEaseType(EaseType easeType) { easeType_ = easeType; }

        void SetElapsedBeats(int beats) { elapsedBeats = beats; }
        void SetMaxBeats(int beats) { maxBeats = beats; }
        
        float GetSpeedProgress() { return speedScaleTimer_.GetProgress(); }
        float GetSpeed2Progress() { return speed2ScaleTimer_.GetProgress(); }
        float GetSpeed3Progress() { return speed3ScaleTimer_.GetProgress(); }
		bool IsSpeedScaling() { return speedScaleTimer_.IsActive(); }
        bool IsSpeed2Scaling() { return speed2ScaleTimer_.IsActive(); }
        bool IsSpeed3Scaling() { return speed3ScaleTimer_.IsActive(); }
#if defined(USE_IMGUI)
        void ShowImGui() override;
#endif

    private:
        Vector3 minScale_ = { 0.9f,0.9f,0.9f };          // 最小スケール係数
        Vector3 maxScale_ = { 1.1f,1.1f,1.1f };          // 最大スケール係数

        Vector3 minSpeedScale_ = { 0.8f,0.8f,0.8f };          // 最小スケール係数
        Vector3 maxSpeedScale_ = { 1.0f,1.0f,1.0f };          // 最大スケール係数

        Vector3 detonationScale_ = { 1.5f, 1.5f, 1.5f }; // 起爆時のスケール係数

        float bpmProgress_ = 0.0f; // BPM進行度（0.0～1.0）

        int elapsedBeats = 0; // 経過拍数
        int maxBeats = 4;     // 最大拍数
		int countBeats_ = 0;   // カウント用拍数

		GameTimer speedScaleTimer_; // 速度スケールタイマー
        GameTimer speed2ScaleTimer_; // 速度スケールタイマー
        GameTimer speed3ScaleTimer_;

        EaseType easeType_ = EaseType::Linear;   // イージングタイプ
    };

} // namespace KashipanEngine