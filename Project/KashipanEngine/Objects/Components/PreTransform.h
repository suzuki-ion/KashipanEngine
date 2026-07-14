#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4x4.h"

namespace KashipanEngine {

/// @brief 1フレーム前のTransform情報を保持するコンポーネント
/// @details 生成時（Initialize）にシーンの ScenePreTransform コンポーネントへポインタ登録され、
///          以後 ScenePreTransform が毎フレーム最後（全オブジェクト・全シーンコンポーネントの更新後）に
///          現在のTransformの値を「前フレームの値」としてこのコンポーネントへ書き込む。
///          モーションブラー等のGPU用途（ワールド行列）と、スクリプトからの補間・速度計算用途
///          （ローカルの位置・回転・スケール）の両方に対応する。
class PreTransform final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(PreTransform, 0xFF, )
    COMPONENT_CATEGORY("Physics")
    ~PreTransform() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<PreTransform>();
        ptr->previousTranslate_ = previousTranslate_;
        ptr->previousRotate_ = previousRotate_;
        ptr->previousRotateQuaternion_ = previousRotateQuaternion_;
        ptr->previousScale_ = previousScale_;
        ptr->previousWorldMatrix_ = previousWorldMatrix_;
        return ptr;
    }

    /// @brief 前フレームのローカル座標を取得する
    const Vector3 &GetPreviousTranslate() const noexcept { return previousTranslate_; }
    /// @brief 前フレームのローカル回転（オイラー角、ラジアン）を取得する
    const Vector3 &GetPreviousRotate() const noexcept { return previousRotate_; }
    /// @brief 前フレームのローカル回転（クォータニオン）を取得する
    const Quaternion &GetPreviousRotateQuaternion() const noexcept { return previousRotateQuaternion_; }
    /// @brief 前フレームのローカルスケールを取得する
    const Vector3 &GetPreviousScale() const noexcept { return previousScale_; }
    /// @brief 前フレームのワールド行列を取得する（モーションブラー等のGPU用途）
    const Matrix4x4 &GetPreviousWorldMatrix() const noexcept { return previousWorldMatrix_; }

    /// @brief 同オブジェクトのTransformの現在値を読み取り、前フレームの値として上書きする
    /// @details ScenePreTransformが毎フレーム最後に呼び出す（定義はTransformの完全な型が必要なため
    ///          PreTransform.cppにある）。他から呼ぶ必要はない
    void CaptureCurrentAsPrevious();

protected:
    /// @brief ScenePreTransformへ自身を登録する（定義はScenePreTransformの完全な型が必要なためPreTransform.cppにある）
    void Initialize() override;
    /// @brief ScenePreTransformから自身の登録を解除する（定義はPreTransform.cppにある）
    void Finalize() override;

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Text("Previous Translate: (%.3f, %.3f, %.3f)", previousTranslate_.x, previousTranslate_.y, previousTranslate_.z);
        ImGui::Text("Previous Rotate (rad): (%.3f, %.3f, %.3f)", previousRotate_.x, previousRotate_.y, previousRotate_.z);
        ImGui::Text("Previous Scale: (%.3f, %.3f, %.3f)", previousScale_.x, previousScale_.y, previousScale_.z);
    }
#endif

private:
    Vector3 previousTranslate_{ 0.0f, 0.0f, 0.0f };
    Vector3 previousRotate_{ 0.0f, 0.0f, 0.0f };
    Quaternion previousRotateQuaternion_ = Quaternion::Identity();
    Vector3 previousScale_{ 1.0f, 1.0f, 1.0f };
    Matrix4x4 previousWorldMatrix_ = Matrix4x4::Identity();
};

REGISTER_COMPONENT_OBJECT(PreTransform)

} // namespace KashipanEngine
