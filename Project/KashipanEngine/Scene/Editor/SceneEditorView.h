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
/// @details 3D/2Dシーンビューカメラ・ScreenBufferの実体はいずれもこのクラスが直接所有する。
///          シーンに自動生成される editorOnly な「Scene View」オブジェクトは何の所有権も持たず、
///          Transform（自動付与、未使用）と SceneViewCameraSettings コンポーネント（値を持たない
///          プロキシ。ShowImGui表示専用）のみが付与される。SceneEditorViewは毎フレーム、
///          自分が持つ現在のカメラ値をこのコンポーネントへ書き込み、Inspectorでの編集結果を
///          このコンポーネントから読み戻すことで、通常のコンポーネントと同じ感覚でHierarchy上の
///          オブジェクトからカメラを操作できるようにしている（詳細はSceneViewCameraSettings.h、
///          PullCameraSettingsFromComponent/PushCameraSettingsToComponent参照）。
///          カメラの実際の永続化（シーンごとの最後の位置・向き）はシーンJSONではなく、
///          EditorSettingsへシーンIDをキーに保存される（BuildCameraSettingsKey参照）。
///          選択中オブジェクトには ImGuizmo によるギズモ操作が行える（複数選択時は選択群の
///          平均位置をピボットとしたグループ操作になる）。
///          screenBuffer_ もシーンオブジェクト・コンポーネントとしては公開せず、このクラスが
///          直接所有する（ヒエラルキー/インスペクターには一切現れない）。ポストエフェクトの
///          適用先解決（SceneRenderer::GetTargetOwner）はSetEditorTarget()で明示的に渡す
///          オーナーオブジェクト（sceneViewObject_）経由で行われるため、screenBuffer_・カメラの
///          どちらもコンポーネントに実体を持たなくても、「Scene View」オブジェクトへ付与した
///          ポストエフェクトは従来通り適用される。
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
    /// @brief シーンビュー用の editorOnly オブジェクト（Transformのみ自動付与。カメラの実体は
    ///        持たない）を毎フレーム sceneViewObjectID_ から解決し直し、見つからなければシーン上を
    ///        検索、それでも無ければ新規作成して sceneViewObject_/sceneViewObjectID_ へ保持する
    /// @details 生ポインタをまたがるフレームでキャッシュしない（CameraRenderer::GetTargetObject等と
    ///          同じ方式）。Play開始時のEditorOnlyオブジェクト削除（Scene::DeleteEditorOnlyObjects）で
    ///          対象オブジェクトがシーンから取り除かれることがあるため、UUIDから毎回引き直すことで
    ///          ダングリングポインタ参照を避ける。カメラの値自体はこのクラスが直接持っているため、
    ///          対象オブジェクトが削除・再生成されてもカメラの状態には一切影響しない
    ///          （Play開始・終了をまたいでもカメラ位置が保持される）。
    ///          見つかったオブジェクトが旧バージョンのScreenBufferObjectを持っていた場合は
    ///          StripLegacyScreenBufferComponentで取り除く
    void EnsureSceneViewObject();
    /// @brief 旧バージョンで保存されたシーンJSONに残っている、「Scene View」オブジェクト上の
    ///        ScreenBufferObjectコンポーネントを取り除く
    /// @details screenBuffer_はこのクラスが直接所有する方式へ変更したため、残したままだと
    ///          同名（kScreenBufferName）バッファがこのクラスの分と二重に生成されてしまう
    static void StripLegacyScreenBufferComponent(EmptyObject *sceneViewObject);
    /// @brief sceneViewObject_へSceneViewCameraSettingsコンポーネントを確保する
    /// @details 未初期化（このSceneEditorViewインスタンスで初回）の場合、EditorSettingsに
    ///          このシーン用の保存値があればそれを読み込み、無ければ旧バージョンのCamera3D/
    ///          SceneViewOrbitStateが残っていないか探して見つかればそこから移行する
    ///          （MigrateLegacyCameraState参照）。いずれも無ければ既定値のまま。
    ///          旧バージョンのCamera3D/SceneViewOrbitStateが見つかった場合は、
    ///          移行の有無に関わらず必ず取り除く（残しておくとカメラ実体が二重に存在してしまうため）
    void EnsureCameraSettingsComponent();
    /// @brief 旧バージョンで保存されたシーンJSONに残っている「Scene View」オブジェクトの
    ///        Transform（位置・回転）・Camera3D（レンズ設定）・SceneViewOrbitState（距離）から、
    ///        カメラの最後の状態をこのクラスの内部状態（target_/eye_/yaw_/pitch_/distance_/
    ///        fovY_/nearClip_/farClip_/enableJitter_）へ一度だけ移行する
    /// @details 移行後の値はEnsureCameraSettingsComponentの呼び出し元がSceneViewCameraSettings
    ///          コンポーネントへ書き戻し、EditorSettingsへ保存する（PushCameraSettingsToComponent参照）
    void MigrateLegacyCameraState(Camera3D *legacyCamera3d);
    /// @brief SceneViewCameraSettingsコンポーネントの現在値を、このクラスの内部状態
    ///        （target_/eye_/yaw_/pitch_/...）へ読み戻す
    /// @details Inspectorでの編集結果を取り込むために毎フレーム呼ぶ。コンポーネント自体は
    ///          値を所有しないため、ここで読み取る値は前フレームの終わりにこのクラス自身が
    ///          PushCameraSettingsToComponentで書き込んだもの（＝Inspectorで編集されていなければ
    ///          前フレームと同じ値）である
    void PullCameraSettingsFromComponent();
    /// @brief このクラスの内部状態をSceneViewCameraSettingsコンポーネントへ書き込み、
    ///        EditorSettingsへも保存する（BuildCameraSettingsKey参照。値が変化していない場合は
    ///        EditorSettings::SetJSON内部の比較により実際のファイル書き込みは発生しない）
    /// @details マウス操作（HandleCameraInput等）による変更をInspector表示・永続化へ反映するため、
    ///          1フレームの処理の最後（ShowImGuiの末尾）で呼ぶ
    void PushCameraSettingsToComponent();
    /// @brief シーンごとのカメラ設定の保存キーを組み立てる（シーンIDを含むため、シーンをまたいで
    ///        別々の値が保存される。シーン名ではなくIDを使うのは、シーン名は変更されうるため）
    std::string BuildCameraSettingsKey() const;
    /// @brief EditorSettingsに保存済みのカメラ設定をこのシーン用のキーで読み込み、内部状態へ適用する
    /// @return 保存済みの値が見つかり適用した場合は true（見つからなかった場合はfalseを返し、
    ///         内部状態は変更しない）
    bool LoadCameraSettingsFromEditorSettings();
    /// @brief 投影行列にサブピクセル単位のジッターを適用する（Camera3D::ApplyProjectionJitterの
    ///        シーンビュー版。TemporalBlendEffect等のフレーム蓄積と組み合わせて使う）
    void ApplyCameraJitter(Matrix4x4 &projection, float aspect);
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

    SceneEditorContext *context_ = nullptr;

    /// @brief シーンビュー用の editorOnly オブジェクトのID（無ければ EnsureSceneViewObject で自動生成される）
    UUID128 sceneViewObjectID_;
    /// @brief sceneViewObjectID_ から毎フレーム解決し直したオブジェクト（フレームをまたいでキャッシュしない）
    EmptyObject *sceneViewObject_ = nullptr;
    /// @brief エディターのシーンビュー専用ScreenBuffer（このクラスが直接所有する。EnsureResourcesで
    ///        生成し、デストラクタでRenderTargetCarryOverRegistryへ預ける。シーンオブジェクトの
    ///        コンポーネントとしては一切公開しない）
    ScreenBuffer *screenBuffer_ = nullptr;
    /// @brief screenBuffer_ のTextureManager登録名・引き継ぎキーとして使う固定名
    static constexpr const char *kScreenBufferName = "EditorSceneView";
    std::unique_ptr<ConstantBufferResource> cameraBuffer_;

    // 3Dシーンビューカメラの状態。このクラスが直接所有する唯一の実体であり（sceneViewObject_の
    // Transformは使わない）、SceneViewCameraSettingsコンポーネント経由でHierarchy/Inspectorから
    // 読み書きできる（PullCameraSettingsFromComponent/PushCameraSettingsToComponent参照）。
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
    /// @brief オービットモード（既定）とフライモードの切り替え（シーンごとに保存される）
    bool flyMode_ = false;
    /// @brief フライモードの移動速度（単位/秒。マウスホイールで調整可能。シーンごとに保存される）
    float flySpeed_ = 5.0f;
    /// @brief 縦画角（ラジアン。以前はCamera3Dコンポーネントが持っていた値）
    float fovY_ = 0.45f;
    float nearClip_ = 0.1f;
    float farClip_ = 1000.0f;
    /// @brief 投影行列にサブピクセルジッターを乗せるか（ApplyCameraJitter参照）
    bool enableJitter_ = false;
    /// @brief ジッター列（Halton(2,3)）の現在位置。実行時状態のみで永続化対象外
    std::uint32_t jitterIndex_ = 0;
    /// @brief このシーンでのカメラ設定（EditorSettings保存値 or 旧データからの移行）を
    ///        読み込み済みかどうか（SceneEditorViewインスタンスの生存中に一度だけtrueになる）
    bool hasLoadedCameraSettings_ = false;

    Matrix4x4 view_ = Matrix4x4::Identity();
    Matrix4x4 projection_ = Matrix4x4::Identity();
    /// @brief 直近に計算したカメラ位置（シャドウマップ計算用にSceneRendererへ渡す）
    Vector3 cameraEye_{ 0.0f, 0.0f, 0.0f };

    // 「2D」表示モード専用のパン・ズームカメラ状態（3Dカメラと同様、シーンごとに保存される）。
    // 3Dフリーカメラのyaw_/pitch_/target_等とは完全に独立しており、モード切り替えの度に
    // リセットされない
    Vector2 pan2D_{ 0.0f, 0.0f };
    /// @brief 画面縦方向に見えるワールド半径（Camera3Dのorthographic sizeと同じ考え方。既定5）
    float zoom2D_ = 5.0f;
    /// @brief 2DカメラのZ軸方向の位置（コンテンツより手前に置く距離。UpdateCamera2DBuffer参照）
    float cameraDistance2D_ = 500.0f;
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
