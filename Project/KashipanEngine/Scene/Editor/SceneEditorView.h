#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Scene/SceneEditorContext.h"
#include "Graphics/Renderer/EditorDebugDraw.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class SceneEditor;
class SceneEditorCommands;
class SceneObjectHierarchy;
class ScreenBuffer;
class ConstantBufferResource;
class CameraRenderer;
class Transform;
struct ColliderInfo2D;

/// @brief シーンエディター用のシーンビュー
/// @details エディター専用の ScreenBuffer とデバッグカメラを持ち、
///          シーン上の全 MeshRenderer をこの ScreenBuffer に描画して ImGui ウィンドウへ表示する。
///          選択中オブジェクトには ImGuizmo によるギズモ操作が行える（複数選択時は選択群の
///          平均位置をピボットとしたグループ操作になる）。
class SceneEditorView final {
public:
    SceneEditorView(Passkey<SceneEditor>, SceneEditorContext *context);
    ~SceneEditorView();

    /// @brief シーンビューの描画（毎フレーム呼ぶ）
    /// @param selectedObjects 選択中のシーンオブジェクト集合（ギズモ表示対象）
    /// @param commands ギズモ操作をUndo/Redo履歴へ積むためのコマンド管理
    /// @param hierarchy クリックピッキングの選択結果を反映するヒエラルキー（nullptr可）
    void ShowImGui(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, SceneObjectHierarchy *hierarchy);

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
    void ShowSceneViewWindow(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, SceneObjectHierarchy *hierarchy);
    void HandleCameraInput();
    /// @brief シーンビュー画像上の左クリックでオブジェクトを選択する（メッシュ三角形との正確なレイ交差判定
    ///        ＋ Light/Cameraアイコンとのスクリーン座標判定）
    /// @details クリック位置からエディターカメラのレイを飛ばし、MeshFilterを持つ描画対象オブジェクトの
    ///          三角形と交差判定して最も手前のオブジェクトを選択する。メッシュを持たないLight/Camera等は
    ///          PickIconAtScreenPositionでアイコンとの距離判定を行い、そちらを優先する。Ctrlクリックで
    ///          トグル追加、何もない場所のクリックで選択解除（Unityのシーンビューと同じ挙動）
    void HandleObjectPicking(SceneObjectHierarchy *hierarchy, const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief クリック位置に最も近いLight/Cameraアイコンのオブジェクトを取得する
    /// @details DrawLightMarkers/DrawCameraMarkersで描画しているアイコンは深度テストせず常に手前に
    ///          表示されるため、メッシュの三角形ピッキングより先にこちらを優先して判定する。
    ///          マーカー表示が無効な種別（showLightMarkers_/showCameraMarkers_）は判定対象から除外する
    /// @return 判定範囲内にアイコンが見つからなければ nullptr
    EmptyObject *PickIconAtScreenPosition(const ImVec2 &screenPos, const ImVec2 &imagePos, const ImVec2 &imageSize) const;
    /// @brief グリッド表示・当たり判定ワイヤーフレームの描画設定を構築し、SceneRendererへ登録する
    /// @details 実際の描画はGPU側（DebugGrid/DebugLinesパイプライン）でscreenBuffer_へ直接行われる
    void UpdateEditorDebugDraw();
    /// @brief シーン上のICollider派生コンポーネントの当たり判定形状をワールド空間の線分として追加する
    void AppendColliderDebugLines(std::vector<DebugLineVertex> &out);
    void AppendWireBox3D(std::vector<DebugLineVertex> &out, const Vector3 &center, const Vector3 &halfExtents, const Quaternion &rotation, const Vector4 &color);
    void AppendWireSphere3D(std::vector<DebugLineVertex> &out, const Vector3 &center, float radius, const Vector4 &color);
    void AppendWireCapsule3D(std::vector<DebugLineVertex> &out, const Vector3 &center, float radius, float height, const Quaternion &rotation, const Vector4 &color);
    void AppendCollider2DShape(std::vector<DebugLineVertex> &out, const ColliderInfo2D &info, float worldZ, const Vector4 &color);
    void AppendRayGizmo(std::vector<DebugLineVertex> &out, const Vector3 &origin, const Vector3 &direction, float length, const Vector4 &color);
    /// @brief シーン上のCameraRendererが付いたオブジェクトの視錐台をワールド空間の線分として追加する
    void AppendCameraFrustumLines(std::vector<DebugLineVertex> &out);
    /// @brief 方向を持つライト（Directional/Spot）の向きをワールド空間の線分として追加する
    void AppendLightDirectionLines(std::vector<DebugLineVertex> &out);
    /// @brief シーン上のSkinnedMeshRendererのスケルトンのボーンを線と球（ジョイント）として追加する
    void AppendSkeletonBoneLines(std::vector<DebugLineVertex> &out);
    /// @brief ワールド座標をシーンビュー画像上のスクリーン座標へ変換する
    /// @param clampToVisibleArea true の場合、NDC範囲を大きく超える点は false を返す（アイコン表示等、
    ///        画面外の点をそもそも描画したくない場合用）。線分の描画では端点がこの範囲外でも
    ///        線自体は ImGui のクリップ矩形で正しく切り取られるため false を渡すこと。
    /// @return カメラの背面にある場合は false
    bool ProjectToImage(const Vector3 &worldPosition, const ImVec2 &imagePos, const ImVec2 &imageSize, ImVec2 &outScreenPos, bool clampToVisibleArea = true) const;
    /// @brief LightRenderer が付いたオブジェクトをアイコンで描画する
    void DrawLightMarkers(const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief CameraRenderer が付いたオブジェクトをアイコンで描画する（視錐台はGPUデバッグライン描画側で行う）
    void DrawCameraMarkers(const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief 選択中オブジェクト群に対してImGuizmoを表示・操作する
    /// @details 単一選択時は対象オブジェクト自身のワールド行列をそのまま渡す（Local/Worldモードの切り替えが有効）。
    ///          複数選択時は選択群の平均位置を中心としたワールド軸固定のグループ行列を渡し、
    ///          1フレームごとの増分（デルタ）を算出して全選択オブジェクトへ均等に適用する。
    void ShowGizmo(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief ワールド行列を（親があればローカル空間へ変換したうえで）Transformへ書き戻す
    void ApplyWorldMatrixToTransform(Transform *transform, const Matrix4x4 &worldMatrix);

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
    /// @brief 直近に計算したカメラ位置（シャドウマップ計算用にSceneRendererへ渡す）
    Vector3 cameraEye_{ 0.0f, 0.0f, 0.0f };

    // ギズモ状態（ImGuizmo::OPERATION / ImGuizmo::MODE をヘッダーで公開しないため int で保持する）
    int gizmoOperation_;
    int gizmoMode_;
    std::unordered_set<EmptyObject *> gizmoTargetObjects_;
    bool isGizmoEditing_ = false;
    // ギズモへ渡す現在の基準行列（単一選択時は対象オブジェクト自身のワールド行列、
    // 複数選択時は選択群の平均位置を中心としたグループ用の仮想行列）。
    // ドラッグ中はImGuizmo::Manipulateによって毎フレーム書き換えられる（前フレームの値を引き継ぐ）。
    Matrix4x4 groupGizmoMatrix_ = Matrix4x4::Identity();
    struct GizmoBeforeState {
        EmptyObject *object = nullptr;
        JSON before;
    };
    // 操作開始時の全対象オブジェクトのTransformスナップショット（Undo用）
    std::vector<GizmoBeforeState> gizmoBeforeStates_;

    // デバッグ表示の有効/無効（再起動後も維持される）
    bool showGrid_ = true;
    bool showLightMarkers_ = true;
    bool showCameraMarkers_ = true;
    bool showColliderGizmos_ = true;
    bool showBoneGizmos_ = false;

    // シーンビューの背景設定（再起動後も維持される）
    Vector4 backgroundColor_{ 0.0f, 0.0f, 0.0f, 1.0f };
    /// @brief 背景に使うテクスチャのAssetsルートからの相対パス（空文字の場合はbackgroundColor_を使用）
    std::string backgroundTexturePath_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
