#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <memory>

#include "Scene/SceneEditorContext.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class SceneEditor;
class SceneEditorCommands;
class ScreenBuffer;
class ConstantBufferResource;
class CameraRenderer;
struct ColliderInfo2D;

/// @brief シーンエディター用のシーンビュー
/// @details エディター専用の ScreenBuffer とデバッグカメラを持ち、
///          シーン上の全 MeshRenderer をこの ScreenBuffer に描画して ImGui ウィンドウへ表示する。
///          選択中オブジェクトには ImGuizmo によるギズモ操作が行える。
class SceneEditorView final {
public:
    SceneEditorView(Passkey<SceneEditor>, SceneEditorContext *context);
    ~SceneEditorView();

    /// @brief シーンビューの描画（毎フレーム呼ぶ）
    /// @param selectedObject 選択中のシーンオブジェクト（ギズモ表示対象）
    /// @param commands ギズモ操作をUndo/Redo履歴へ積むためのコマンド管理
    void ShowImGui(EmptyObject *selectedObject, SceneEditorCommands *commands);

private:
    /// @brief gCamera3D 定数バッファと同レイアウトの構造体
    struct CameraConstant {
        Matrix4x4 view;
        Matrix4x4 projection;
        Matrix4x4 viewProjection;
        Vector4 eyePosition;
        float fov = 0.0f;
        float padding[3]{};
    };

    void EnsureResources();
    /// @brief デバッグカメラの行列を計算して定数バッファへアップロードする
    void UpdateCameraBuffer();
    /// @brief SceneRenderer へエディター描画先として登録する
    void RegisterEditorTarget();
    void ShowSceneViewWindow(EmptyObject *selectedObject, SceneEditorCommands *commands);
    void HandleCameraInput();
    /// @brief XZ平面のグリッド線を描画する
    void DrawGrid(const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief ワールド座標をシーンビュー画像上のスクリーン座標へ変換する
    /// @param clampToVisibleArea true の場合、NDC範囲を大きく超える点は false を返す（アイコン表示等、
    ///        画面外の点をそもそも描画したくない場合用）。線分の描画では端点がこの範囲外でも
    ///        線自体は ImGui のクリップ矩形で正しく切り取られるため false を渡すこと。
    /// @return カメラの背面にある場合は false
    bool ProjectToImage(const Vector3 &worldPosition, const ImVec2 &imagePos, const ImVec2 &imageSize, ImVec2 &outScreenPos, bool clampToVisibleArea = true) const;
    /// @brief LightRenderer が付いたオブジェクトをアイコンで描画する
    void DrawLightMarkers(const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief CameraRenderer が付いたオブジェクトをアイコンと視錐台で描画する
    void DrawCameraMarkers(const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief 指定カメラの視錐台をワールド座標から逆投影して描画する
    void DrawCameraFrustum(CameraRenderer *cameraRenderer, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color);
    /// @brief シーン上のICollider派生コンポーネントの当たり判定形状をワイヤーフレームで描画する
    void DrawColliderGizmos(const ImVec2 &imagePos, const ImVec2 &imageSize);
    void DrawWireBox3D(const Vector3 &center, const Vector3 &halfExtents, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color);
    void DrawWireSphere3D(const Vector3 &center, float radius, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color);
    void DrawWireCapsule3D(const Vector3 &center, float radius, float height, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color);
    void DrawCollider2DShape(const ColliderInfo2D &info, float worldZ, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color);
    void DrawRayGizmo(const Vector3 &origin, const Vector3 &direction, float length, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color);
    void ShowGizmo(EmptyObject *selectedObject, SceneEditorCommands *commands, const ImVec2 &imagePos, const ImVec2 &imageSize);

    SceneEditorContext *context_ = nullptr;

    ScreenBuffer *screenBuffer_ = nullptr;
    std::unique_ptr<ConstantBufferResource> cameraBuffer_;

    // デバッグカメラ（注視点周りのオービットカメラ）
    Vector3 target_{ 0.0f, 0.0f, 0.0f };
    float distance_ = 10.0f;
    float yaw_ = 0.0f;
    float pitch_ = 0.3f;
    float fovY_ = 0.45f;
    float nearClip_ = 0.1f;
    float farClip_ = 1000.0f;

    Matrix4x4 view_ = Matrix4x4::Identity();
    Matrix4x4 projection_ = Matrix4x4::Identity();

    // ギズモ状態（ImGuizmo::OPERATION / ImGuizmo::MODE をヘッダーで公開しないため int で保持する）
    int gizmoOperation_;
    int gizmoMode_;
    EmptyObject *gizmoTargetObject_ = nullptr;
    bool isGizmoEditing_ = false;
    JSON gizmoBefore_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
