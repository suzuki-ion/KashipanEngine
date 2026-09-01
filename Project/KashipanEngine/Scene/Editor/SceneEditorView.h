#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Graphics/Renderer/EditorDebugDraw.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {

class SceneEditor;
class SceneEditorCommands;
class SceneObjectHierarchy;
class ScreenBuffer;
class ConstantBufferResource;
class CameraRenderer;
class Camera3D;
class ScreenBufferObject;
class Transform;
class TilemapRenderer;
struct ColliderInfo2D;

/// @brief シーンエディター用のシーンビュー
/// @details エディター専用の ScreenBuffer とデバッグカメラを持ち、
///          シーン上の全 MeshRenderer をこの ScreenBuffer に描画して ImGui ウィンドウへ表示する。
///          選択中オブジェクトには ImGuizmo によるギズモ操作が行える（複数選択時は選択群の
///          平均位置をピボットとしたグループ操作になる）。
///          カメラ・ScreenBufferの実体は、シーンに自動生成される editorOnly な「Scene View」
///          オブジェクト（Transform + Camera3D + ScreenBufferObject）が保持する。これにより
///          通常のオブジェクトと同様にHierarchy/Inspectorから編集・コンポーネント追加ができ、
///          シーンJSONへシーンごとに永続化される（詳細はEnsureSceneViewObject参照）。
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
    /// @brief gCamera2D 定数バッファと同レイアウトの構造体（CameraRenderer::Camera2DConstant参照）
    struct Camera2DConstant {
        Matrix4x4 view;
        Matrix4x4 projection;
        Matrix4x4 viewProjection;
    };

    void EnsureResources();
    /// @brief シーンビュー用の editorOnly オブジェクト（Transform + Camera3D + ScreenBufferObject）を
    ///        毎フレーム sceneViewObjectID_ から解決し直し、見つからなければシーン上を検索、
    ///        それでも無ければ新規作成して sceneViewObject_/sceneViewObjectID_ へ保持する
    /// @details 生ポインタをまたがるフレームでキャッシュしない（CameraRenderer::GetTargetObject等と
    ///          同じ方式）。Play開始時のEditorOnlyオブジェクト削除（Scene::DeleteEditorOnlyObjects）で
    ///          対象オブジェクトがシーンから取り除かれることがあるため、UUIDから毎回引き直すことで
    ///          ダングリングポインタ参照を避ける。新規作成時は現状と同じ既定値
    ///          （target(0,0,0)・distance 10・yaw 0・pitch 0.3）を用いる。
    ///          既存のオブジェクトが見つかった場合はそのTransformから軌道パラメータ
    ///          （yaw_/pitch_/target_）を逆算し、保存済みのカメラ位置から操作を再開できるようにする
    void EnsureSceneViewObject();
    /// @brief デバッグカメラの行列を計算して定数バッファへアップロードする
    void UpdateCameraBuffer();
    /// @brief 「2D」表示モード専用の正射影カメラ（パン・ズームのみ、Z軸方向を見る）の行列を計算して
    ///        定数バッファへアップロードする。3Dフリーカメラ（UpdateCameraBuffer）とは完全に独立しており、
    ///        シーン内の実際のCamera2Dコンポーネントの有無・内容に関係なく、エディター側で単独に
    ///        パン・ズーム操作できる（RegisterEditorTargetでgCamera2Dへ上書きバインドされる）
    void UpdateCamera2DBuffer();
    /// @brief SceneRenderer へエディター描画先として登録する
    void RegisterEditorTarget();
    void ShowSceneViewWindow(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, SceneObjectHierarchy *hierarchy);
    void HandleCameraInput();
    /// @brief 「2D」表示モード中のカメラ操作（左/中ドラッグでパン、ホイールでズーム）
    void HandleCamera2DInput();
    /// @brief 現在の表示モードに応じて、ピッキング・ギズモ・スクリーン投影で使うべきビュー行列を返す
    ///        （2Dモード中はUpdateCamera2DBufferが計算したview2D_、それ以外はUpdateCameraBufferのview_）
    Matrix4x4 GetActiveView() const noexcept {
        return (displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly) ? view2D_ : view_;
    }
    /// @brief GetActiveViewと対になる射影行列
    Matrix4x4 GetActiveProjection() const noexcept {
        return (displayMode_ == SceneRenderer::EditorDisplayMode::TwoDOnly) ? projection2D_ : projection_;
    }
    /// @brief シーンビュー画像上の左クリックでオブジェクトを選択する（メッシュ三角形との正確なレイ交差判定
    ///        ＋ Light/Cameraアイコンとのスクリーン座標判定）
    /// @details クリック位置からエディターカメラのレイを飛ばし、MeshFilterを持つ描画対象オブジェクトの
    ///          三角形と交差判定して最も手前のオブジェクトを選択する。メッシュを持たないLight/Camera等は
    ///          PickIconAtScreenPositionでアイコンとの距離判定を行い、そちらを優先する。Ctrlクリックで
    ///          トグル追加、何もない場所のクリックで選択解除（Unityのシーンビューと同じ挙動）
    void HandleObjectPicking(SceneObjectHierarchy *hierarchy, const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief スクリーン座標からエディターカメラのレイを飛ばし、MeshFilterを持つ描画対象オブジェクトの
    ///        三角形と交差判定して最も手前のオブジェクトを求める（HandleObjectPicking/
    ///        ComputeCursorWorldPositionの共通処理）
    /// @param outRayStart レイの始点（近平面上のワールド座標）
    /// @param outRayEnd レイの終点（遠平面上のワールド座標）
    /// @param outHitObject 交差したオブジェクト（見つからなければnullptr）
    /// @param outHitT 交差したオブジェクトがある場合、レイ上のパラメータt（0=始点、1=終点）
    /// @return いずれかのオブジェクトと交差した場合true
    bool RaycastSceneMeshes(const ImVec2 &screenPos, const ImVec2 &imagePos, const ImVec2 &imageSize,
        Vector3 &outRayStart, Vector3 &outRayEnd, EmptyObject *&outHitObject, float &outHitT) const;
    /// @brief シーンビュー画像上のスクリーン座標を、Prefab配置等に使うワールド座標へ変換する
    /// @details Unityのシーンビューと同様、既存のメッシュ表面があればそこへスナップし、
    ///          無ければY=0の地面平面との交点、それも無ければ（真上/真下を向いている等）
    ///          カメラから現在の注視距離だけ進めた点にフォールバックする
    Vector3 ComputeCursorWorldPosition(const ImVec2 &screenPos, const ImVec2 &imagePos, const ImVec2 &imageSize) const;

    //==================================================
    // TilemapRenderer タイルペイントツール
    //==================================================

    /// @brief シーンビューのメニューバーへ「Tile Paint」タブを表示する。選択中オブジェクトが
    ///        単体でTilemapRendererを持つ場合のみ内容を有効化し、それ以外は無効化して表示する
    /// @param paintableTilemap 選択中の単一オブジェクトが持つTilemapRenderer（無ければnullptr）
    void ShowTilemapPaintToolbar(TilemapRenderer *paintableTilemap);
    /// @brief タイルペイントの入力処理（ホバーセルのハイライト表示・クリック/ドラッグでの
    ///        SetTile適用・ストローク単位でのUndo登録）を行う
    /// @return このフレーム、シーンビューのマウス入力をペイント用に消費した（ホバー中含む）場合true。
    ///         trueの場合、呼び出し側は通常のオブジェクトピッキング・ギズモ操作を行わないこと
    bool HandleTilemapPaint(EmptyObject *owner, TilemapRenderer *tilemap, SceneEditorCommands *commands,
        const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief スクリーン座標からのレイと、対象オブジェクトのローカルZ=0平面（TilemapRendererのメッシュが
    ///        生成される平面）との交点を求め、タイルグリッドのセル座標へ変換する
    /// @return グリッド範囲内のセルが求まった場合true（範囲外・平面と非交差の場合false）
    bool ComputeTilemapCellUnderCursor(EmptyObject *owner, TilemapRenderer *tilemap, const ImVec2 &screenPos,
        const ImVec2 &imagePos, const ImVec2 &imageSize, int &outX, int &outY) const;
    /// @brief 指定セルの4隅をワールド→スクリーン座標へ投影し、枠線でハイライト表示する
    void DrawTilemapCellHighlight(EmptyObject *owner, TilemapRenderer *tilemap, int cellX, int cellY,
        const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color) const;

    /// @brief Assetsウィンドウからのプレハブファイル（.prefab）のドラッグ&ドロップを処理する
    /// @details ドラッグ中（未ドロップ）は毎フレームUpdateGhostPreviewでプレビューを更新し、
    ///          実際にドロップされた瞬間にInstantiatePrefabFileでシーンへ配置する。ドロップ/キャンセル/
    ///          シーンビュー範囲外への移動でこのウィンドウ上のドラッグが終わった場合はプレビューを消す
    void HandlePrefabDragDrop(SceneObjectHierarchy *hierarchy, const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief ドラッグ中のPrefabプレビューを更新する（カーソル直下の配置予定位置に半透明メッシュを表示）
    /// @details プレハブJSONのパースは対象パスが変わった時のみ行い（ドラッグ中の毎フレーム再パースを
    ///          避けるため）、位置計算とワールド行列の再計算のみ毎フレーム行う
    void UpdateGhostPreview(const std::string &prefabPath, const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief ドラッグ中のPrefabプレビューを消す
    void ClearGhostPreview();
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
    /// @brief 「2D」表示モード専用のグリッド線をImGuiオーバーレイで描画する（GPUデバッグライン描画の
    ///        3Dグリッドとは別実装。zoom2D_に応じて1-2-5系列で間隔を自動調整し、X=0/Y=0の軸は強調表示する）
    void DrawGrid2D(const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief 「2D」表示モード専用に、2Dコライダー（Is2D()なICollider）の当たり判定形状をImGui
    ///        オーバーレイで描画する。3D側のAppendColliderDebugLines（GPUデバッグライン）と違い、
    ///        2D専用の正射影カメラ（view2D_/projection2D_）に投影して表示する
    void DrawCollider2DOverlay(const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief 「2D」表示モード専用に、シーン内のCamera2Dが実際に映す範囲（矩形）をImGuiオーバーレイで
    ///        描画する。3D/2D3DモードのAppendCameraFrustumLines（GPUデバッグライン）と対になる表示
    void DrawCamera2DBoundsOverlay(const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief 選択中オブジェクト群に対してImGuizmoを表示・操作する
    /// @details 単一選択時は対象オブジェクト自身のワールド行列をそのまま渡す（Local/Worldモードの切り替えが有効）。
    ///          複数選択時は選択群の平均位置を中心としたワールド軸固定のグループ行列を渡し、
    ///          1フレームごとの増分（デルタ）を算出して全選択オブジェクトへ均等に適用する。
    void ShowGizmo(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, const ImVec2 &imagePos, const ImVec2 &imageSize);
    /// @brief ワールド行列を（親があればローカル空間へ変換したうえで）Transformへ書き戻す
    void ApplyWorldMatrixToTransform(Transform *transform, const Matrix4x4 &worldMatrix);
    /// @brief 現在のTransformの内容から、オービット/フライ双方の内部状態
    ///        （yaw_/pitch_/target_/eye_、distance_はSceneViewOrbitStateから）を再同期する
    /// @details EnsureSceneViewObjectで既存の「Scene View」オブジェクトを見つけた時、および
    ///          UpdateCameraBufferで前回自分が書き込んだ値からTransformが変わっていた
    ///          （Inspector編集・Undo/Redo等の外部変更）ときに呼ぶ
    void SyncCameraStateFromTransform(Transform *transform);

    SceneEditorContext *context_ = nullptr;

    /// @brief シーンビュー用の editorOnly オブジェクトのID（無ければ EnsureSceneViewObject で自動生成される）
    UUID128 sceneViewObjectID_;
    /// @brief sceneViewObjectID_ から毎フレーム解決し直したオブジェクト（フレームをまたいでキャッシュしない）
    EmptyObject *sceneViewObject_ = nullptr;
    /// @brief sceneViewObject_ が持つ ScreenBufferObject の ScreenBuffer（非所有、毎フレーム取得し直す）
    ScreenBuffer *screenBuffer_ = nullptr;
    std::unique_ptr<ConstantBufferResource> cameraBuffer_;

    // デバッグカメラ（マウス操作用の一時的なパラメータであり、実際のカメラ位置・向きは
    // sceneViewObject_ の Transform に都度書き戻して永続化される）。
    // オービットモードでは target_（注視点）が基準で、回転すると eye_ 側が軌道を描くように動く。
    // フライモードでは eye_（カメラ自身の位置）が基準で、回転すると target_ の方が向きの先へ
    // 追従して再計算される（distance_はその場合「target_が常にeye_の前方どれだけ先にあるか」の
    // 意味になる）。どちらのモードでも両方の値を毎フレーム維持するため、モード切り替え時に
    // 特別な補正処理は不要（切り替えた瞬間から自然に続きの操作ができる）
    Vector3 target_{ 0.0f, 0.0f, 0.0f };
    Vector3 eye_{ 0.0f, 0.0f, 10.0f };
    float distance_ = 10.0f;
    float yaw_ = 0.0f;
    float pitch_ = 0.3f;
    /// @brief オービットモード（既定）とフライモードの切り替え（再起動後も維持される）
    bool flyMode_ = false;
    /// @brief フライモードの移動速度（単位/秒。マウスホイールで調整可能。再起動後も維持される）
    float flySpeed_ = 5.0f;

    /// @brief 前回自分がTransformへ書き込んだ位置・回転（Inspector編集等での外部変更を検知する基準値）
    bool hasLastAppliedCameraTransform_ = false;
    Vector3 lastAppliedPosition_{ 0.0f, 0.0f, 0.0f };
    Quaternion lastAppliedRotation_ = Quaternion::Identity();

    Matrix4x4 view_ = Matrix4x4::Identity();
    Matrix4x4 projection_ = Matrix4x4::Identity();
    /// @brief 直近に計算したカメラ位置（シャドウマップ計算用にSceneRendererへ渡す）
    Vector3 cameraEye_{ 0.0f, 0.0f, 0.0f };

    // 「2D」表示モード専用のパン・ズームカメラ状態（再起動後も維持される）。3Dフリーカメラの
    // yaw_/pitch_/target_等とは完全に独立しており、モード切り替えの度にリセットされない
    Vector2 pan2D_{ 0.0f, 0.0f };
    /// @brief 画面縦方向に見えるワールド半径（Camera3Dのorthographic sizeと同じ考え方。既定5）
    float zoom2D_ = 5.0f;
    Matrix4x4 view2D_ = Matrix4x4::Identity();
    Matrix4x4 projection2D_ = Matrix4x4::Identity();
    std::unique_ptr<ConstantBufferResource> camera2DBuffer_;

    // ギズモ状態（ImGuizmo::OPERATION / ImGuizmo::MODE をヘッダーで公開しないため int で保持する）
    int gizmoOperation_;
    int gizmoMode_;
    /// @brief グリッドスナップの有効/無効（再起動後も維持される）。Ctrlキー押下中は一時的に反転する
    ///        （Unity等と同じ操作感）
    bool gizmoSnapEnabled_ = false;
    /// @brief 移動操作のグリッド間隔（ワールド単位、XYZ共通）
    float gizmoSnapTranslate_ = 1.0f;
    /// @brief 回転操作のスナップ角度（度）
    float gizmoSnapRotateDegrees_ = 15.0f;
    /// @brief 拡大縮小操作のスナップ間隔
    float gizmoSnapScale_ = 0.1f;
    /// @brief キーボードショートカット（W/E/R）によるギズモ操作切り替えを処理する
    /// @details 右ボタン押下中（フリーカメラ移動でW/E/Qを使用中）はショートカットを無効にする
    void HandleGizmoShortcuts(bool isSceneViewHovered);
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

    /// @brief シーンビューに表示・選択・ギズモ編集の対象とするオブジェクトの種類（再起動後も維持される）
    SceneRenderer::EditorDisplayMode displayMode_ = SceneRenderer::EditorDisplayMode::Combined;

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

    // TilemapRendererのタイルペイントツール用状態
    /// @brief Tile Paintトグルの有効/無効（選択がTilemapRenderer付きオブジェクト単体でなくなると自動でfalseへ戻る）
    bool tilemapPaintActive_ = false;
    /// @brief 現在選択中のブラシ（-1=消しゴム、0以降はTilemapRenderer::GetTileTypes()のインデックス）
    int paintBrushTileType_ = -1;
    /// @brief ストローク（マウス押下〜離すまで）を1つのUndo単位にするための状態
    bool isPaintStrokeActive_ = false;
    /// @brief 現在のストロークが右クリックで開始された（常に消しゴムとして塗る）ものかどうか
    bool paintStrokeIsErase_ = false;
    JSON paintStrokeBeforeJson_;
    /// @brief ドラッグ中、前フレームで塗ったセル座標（セル間の線補間に使う。ストローク開始時は無効値）
    int lastPaintCellX_ = 0;
    int lastPaintCellY_ = 0;

    // ドラッグ中のPrefabプレビュー用状態（UpdateGhostPreview/ClearGhostPreview参照）
    bool ghostPreviewActive_ = false;
    /// @brief ghostPreviewNodes_が対応しているプレハブのパス（変化した時だけ再パースするためのキャッシュキー）
    std::string ghostPreviewAssetPath_;
    /// @brief ghostPreviewAssetPathから読み込んだプレハブのノード列（ドラッグ中は使い回す）
    std::vector<PasteObjectCommand::Node> ghostPreviewNodes_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
