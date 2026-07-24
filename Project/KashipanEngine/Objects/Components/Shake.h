#pragma once
#include <algorithm>
#include <array>

#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils/Easings.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief オブジェクトを揺らすシェイクコンポーネント
/// @details 位置・回転それぞれXYZ軸ごとに有効/無効・振れ幅・目標値へ到達する速度・
///          イージング種類を設定できる。各軸は独立して「ランダムな目標値」を選び直し、
///          指定した速度とイージングでその目標値へ向かって遷移し続けることで揺れを表現する。
///          - 処理タイミング（ProcessTiming）:
///            Immediate  = 自身のUpdate内でその場処理する
///            DeferredEnd= SceneShakeApplierへ登録し、全オブジェクトのUpdate/衝突解決が
///                         終わった後にまとめて処理する（既定。他のスクリプトの
///                         Transform操作と競合しない）
///          - 適用方法（ApplyTarget）:
///            ToTransform = Transformへ直接オフセットを加算する（次フレーム以降、揺れ分は
///                          必ず先に差し引かれてから新しいオフセットが乗るため、シェイクが
///                          終了すれば元のTransformの値に戻る）
///            RenderOnly  = Transform自体は一切変更せず、描画時（SceneRendererが
///                          ワールド行列を収集する際）にのみオフセットを乗算する（既定）
class Shake final : public IObjectComponent {
public:
    enum class ProcessTiming : int {
        Immediate = 0,
        DeferredEnd = 1,
    };
    enum class ApplyTarget : int {
        ToTransform = 0,
        RenderOnly = 1,
    };

    OBJECT_COMPONENT_CONSTRUCTOR(Shake, 0xFF,
        ADD_MEMBER_VARIABLE(positionEnableX_);
        ADD_MEMBER_VARIABLE(positionEnableY_);
        ADD_MEMBER_VARIABLE(positionEnableZ_);
        ADD_MEMBER_VARIABLE(positionAmplitude_);
        ADD_MEMBER_VARIABLE(positionSpeed_);
        ADD_MEMBER_VARIABLE(rotationEnableX_);
        ADD_MEMBER_VARIABLE(rotationEnableY_);
        ADD_MEMBER_VARIABLE(rotationEnableZ_);
        ADD_MEMBER_VARIABLE(rotationAmplitudeDeg_);
        ADD_MEMBER_VARIABLE(rotationSpeed_);
        ADD_MEMBER_VARIABLE(autoPlay_);
        ADD_MEMBER_VARIABLE(duration_);
    )
    COMPONENT_CATEGORY("Utility")
    ~Shake() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override;

    //==================================================
    // 再生制御
    //==================================================

    /// @brief シェイクを開始する
    /// @param duration 再生時間（秒）。0以下を指定すると Stop() が呼ばれるまで無期限に再生する
    void Play(float duration = 0.0f) noexcept {
        isPlaying_ = true;
        playDuration_ = duration;
        elapsedPlayTime_ = 0.0f;
    }
    /// @brief シェイクを停止する（ToTransform適用中だった場合、そのフレームでオフセットが差し引かれ元に戻る）
    void Stop() noexcept { isPlaying_ = false; }
    bool IsPlaying() const noexcept { return isPlaying_; }

    void SetProcessTiming(ProcessTiming timing) noexcept { processTiming_ = timing; }
    ProcessTiming GetProcessTiming() const noexcept { return processTiming_; }
    void SetProcessTimingInt(int timing) noexcept { processTiming_ = static_cast<ProcessTiming>(timing); }
    int GetProcessTimingInt() const noexcept { return static_cast<int>(processTiming_); }

    void SetApplyTarget(ApplyTarget target) noexcept { applyTarget_ = target; }
    ApplyTarget GetApplyTarget() const noexcept { return applyTarget_; }
    void SetApplyTargetInt(int target) noexcept { applyTarget_ = static_cast<ApplyTarget>(target); }
    int GetApplyTargetInt() const noexcept { return static_cast<int>(applyTarget_); }

    void SetPositionEaseTypeInt(int type) noexcept { positionEaseType_ = static_cast<EaseType>(type); }
    int GetPositionEaseTypeInt() const noexcept { return static_cast<int>(positionEaseType_); }
    void SetRotationEaseTypeInt(int type) noexcept { rotationEaseType_ = static_cast<EaseType>(type); }
    int GetRotationEaseTypeInt() const noexcept { return static_cast<int>(rotationEaseType_); }

    void SetPositionEnable(bool x, bool y, bool z) noexcept { positionEnableX_ = x; positionEnableY_ = y; positionEnableZ_ = z; }
    void SetPositionAmplitude(const Vector3 &amplitude) noexcept { positionAmplitude_ = amplitude; }
    const Vector3 &GetPositionAmplitude() const noexcept { return positionAmplitude_; }
    void SetPositionSpeed(const Vector3 &speed) noexcept { positionSpeed_ = speed; }
    const Vector3 &GetPositionSpeed() const noexcept { return positionSpeed_; }

    void SetRotationEnable(bool x, bool y, bool z) noexcept { rotationEnableX_ = x; rotationEnableY_ = y; rotationEnableZ_ = z; }
    /// @brief 回転の振れ幅を設定する（度）
    void SetRotationAmplitude(const Vector3 &amplitudeDeg) noexcept { rotationAmplitudeDeg_ = amplitudeDeg; }
    const Vector3 &GetRotationAmplitude() const noexcept { return rotationAmplitudeDeg_; }
    void SetRotationSpeed(const Vector3 &speed) noexcept { rotationSpeed_ = speed; }
    const Vector3 &GetRotationSpeed() const noexcept { return rotationSpeed_; }

    /// @brief 現在フレームの位置オフセット（ワールド空間、未適用のRenderOnly参照用）
    const Vector3 &GetCurrentPositionOffset() const noexcept { return currentPositionOffset_; }
    /// @brief 現在フレームの回転オフセット（ラジアン、オブジェクト自身のピボット周り）
    const Vector3 &GetCurrentRotationOffset() const noexcept { return currentRotationOffset_; }

    /// @brief SceneShakeApplierから呼ばれる、DeferredEnd+ToTransform時のTransform適用処理
    void ApplyToTransformInterface(Passkey<class SceneShakeApplier>) { ApplyToTransform(); }

protected:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

private:
    struct AxisRuntime {
        float previousValue = 0.0f;
        float targetValue = 0.0f;
        float timer = 0.0f;
        float currentValue = 0.0f;
    };

    // --- 位置シェイク設定 ---
    bool positionEnableX_ = false;
    bool positionEnableY_ = false;
    bool positionEnableZ_ = false;
    Vector3 positionAmplitude_{ 0.1f, 0.1f, 0.1f };
    Vector3 positionSpeed_{ 20.0f, 20.0f, 20.0f };
    EaseType positionEaseType_ = EaseType::Linear;

    // --- 回転シェイク設定（振れ幅は度で保持し、適用時にラジアンへ変換する） ---
    bool rotationEnableX_ = false;
    bool rotationEnableY_ = false;
    bool rotationEnableZ_ = false;
    Vector3 rotationAmplitudeDeg_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotationSpeed_{ 20.0f, 20.0f, 20.0f };
    EaseType rotationEaseType_ = EaseType::Linear;

    // --- 再生設定 ---
    bool autoPlay_ = true;
    /// @brief autoPlay_時の再生時間（秒）。0以下で無期限
    float duration_ = 0.0f;

    ProcessTiming processTiming_ = ProcessTiming::DeferredEnd;
    ApplyTarget applyTarget_ = ApplyTarget::RenderOnly;

    // --- 実行時状態（保存されない） ---
    bool isPlaying_ = false;
    float playDuration_ = 0.0f;
    float elapsedPlayTime_ = 0.0f;

    std::array<AxisRuntime, 3> positionRuntime_{};
    std::array<AxisRuntime, 3> rotationRuntime_{};

    Vector3 currentPositionOffset_{ 0.0f, 0.0f, 0.0f };
    Vector3 currentRotationOffset_{ 0.0f, 0.0f, 0.0f };

    // ToTransform適用時、直前に加算済みのオフセット（次回の適用前に必ず差し引く）
    Vector3 lastAppliedPositionOffset_{ 0.0f, 0.0f, 0.0f };
    Vector3 lastAppliedRotationOffset_{ 0.0f, 0.0f, 0.0f };
    bool appliedToTransformLastFrame_ = false;

    /// @brief 1軸分のランダム目標値追従を進める
    static float UpdateAxisRuntime(AxisRuntime &state, bool enabled, float amplitude, float speed, EaseType easeType, float dt);

    /// @brief 全軸のcurrentPositionOffset_/currentRotationOffset_を計算する（毎フレーム呼ぶ）
    void ComputeOffsets(float dt);

    /// @brief ToTransformモード時、前回分を差し引いてから今回分をTransformへ加算する
    void ApplyToTransform();

    class SceneShakeApplier *GetOrAddSceneShakeApplier() const;
};

REGISTER_COMPONENT_OBJECT(Shake)

} // namespace KashipanEngine
