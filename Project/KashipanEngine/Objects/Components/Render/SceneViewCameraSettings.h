#pragma once
#if defined(USE_IMGUI)
#include "Debug/Logger.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Objects/ObjectComponentHeader.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief シーンビュー専用カメラ（3Dオービット/フライ・2Dパン/ズーム）のパラメータを
///        Hierarchy/Inspectorへ公開するためだけのコンポーネント
/// @details カメラの実体（位置・向き・レンズ設定等）はSceneEditorViewが直接所有しており、
///          このコンポーネントは値を一切自分では管理しない。SceneEditorViewが毎フレーム
///          「現在の値をここへ書き込み、Inspectorでの編集結果をここから読み戻す」ことで、
///          通常のコンポーネントと同じ感覚でHierarchy上のオブジェクトから操作できるようにする
///          窓口（プロキシ）に過ぎない。そのためシーンJSONへの永続化（SaveToJson/LoadFromJson）
///          は行わない。実際の永続化はEditorSettingsへシーンIDをキーに保存される
///          （SceneEditorView::PushCameraSettingsToComponent/BuildCameraSettingsKey参照）。
///          他コンポーネントと異なり、フィールドを private+getter/setter にせず public にして
///          いるのは、上記の通り「このクラス自身は値の意味・妥当性に責任を持たない、
///          SceneEditorViewとInspector間の単純な橋渡し役」であることを明示するため
class SceneViewCameraSettings final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(SceneViewCameraSettings, 1, )
    COMPONENT_CATEGORY("Render")
    ~SceneViewCameraSettings() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        // シーンに1つだけ存在するエディター専用オブジェクトのためのコンポーネントであり、
        // 複製・貼り付けの対象にはならない（値はSceneEditorViewが毎フレーム上書きする）
        return std::make_unique<SceneViewCameraSettings>();
    }

    //==================================================
    // 3Dカメラ（オービット/フライ）
    //==================================================
    Vector3 target{ 0.0f, 0.0f, 0.0f };
    Vector3 eye{ 0.0f, 0.0f, 10.0f };
    /// @brief オービットモードでの注視点までの距離
    float distance = 10.0f;
    float yawDegrees = 0.0f;
    float pitchDegrees = 17.2f;
    bool flyMode = false;
    float flySpeed = 5.0f;
    /// @brief 縦画角（ラジアン。Camera3Dコンポーネントと単位を揃えている）
    float fovY = 0.45f;
    float nearClip = 0.1f;
    float farClip = 1000.0f;
    bool enableJitter = false;

    //==================================================
    // 2Dカメラ（パン・ズーム）
    //==================================================
    Vector2 pan2D{ 0.0f, 0.0f };
    /// @brief 画面縦方向に見えるワールド半径
    float zoom2D = 5.0f;
    /// @brief 2DカメラのZ軸方向の位置（コンテンツより手前に置く距離。SceneEditorView::UpdateCamera2DBuffer参照）
    float cameraDistance2D = 500.0f;

    /// @brief SceneEditorViewが一度でもこのインスタンスへ現在のカメラ値を書き込んだか
    /// @details 新規生成された（AddComponent直後、またはPlay終了時のスナップショット復元等で
    ///          シーンJSONから読み込まれた直後の）インスタンスはfalseのまま＝上記フィールドは
    ///          すべて既定値。SceneEditorView::EnsureCameraSettingsComponentがこれを見て、
    ///          まだ値を書き込んでいない（＝既定値のままの）インスタンスを
    ///          PullCameraSettingsFromComponentで誤って読み取ってしまう
    ///          （＝カメラがリセットされたように見える）のを防ぐ。ShowImGuiには出さない
    bool hasSyncedFromEditorView = false;

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        ImGui::TextUnformatted(TranslationC("component.sceneviewcamerasettings.desc"));

        if (ImGui::TreeNodeEx(TranslationLabel("component.sceneviewcamerasettings.camera3d"), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox(TranslationLabel("component.sceneviewcamerasettings.flymode"), &flyMode);
            ImGuiCustom::EditValue(TranslationLabel("component.sceneviewcamerasettings.target"), target, { .vSpeed = 0.05f });
            ImGuiCustom::EditValue(TranslationLabel("component.sceneviewcamerasettings.eye"), eye, { .vSpeed = 0.05f });
            ImGuiCustom::EditValue(TranslationLabel("component.sceneviewcamerasettings.distance"), distance, { .vSpeed = 0.1f, .vMin = 0.1f, .vMax = 100000.0f });
            ImGuiCustom::EditValue(TranslationLabel("component.sceneviewcamerasettings.yaw"), yawDegrees, { .vSpeed = 0.5f });
            ImGuiCustom::EditValue(TranslationLabel("component.sceneviewcamerasettings.pitch"), pitchDegrees, { .vSpeed = 0.5f, .vMin = -89.9f, .vMax = 89.9f });
            ImGuiCustom::EditValue(TranslationLabel("component.sceneviewcamerasettings.flyspeed"), flySpeed, { .vSpeed = 0.05f, .vMin = 0.01f, .vMax = 1000.0f });
            ImGui::Separator();
            ImGuiCustom::EditValue(TranslationLabel("component.camera3d.fovy"), fovY, { .vSpeed = 0.01f });
            ImGuiCustom::EditValue(TranslationLabel("component.camera3d.near"), nearClip, { .vSpeed = 0.01f });
            ImGuiCustom::EditValue(TranslationLabel("component.camera3d.far"), farClip, { .vSpeed = 1.0f });
            ImGui::Checkbox(TranslationLabel("component.camera3d.enable_jitter"), &enableJitter);
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx(TranslationLabel("component.sceneviewcamerasettings.camera2d"), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiCustom::EditValue(TranslationLabel("component.sceneviewcamerasettings.pan2d"), pan2D, { .vSpeed = 0.5f });
            ImGuiCustom::EditValue(TranslationLabel("component.sceneviewcamerasettings.zoom2d"), zoom2D, { .vSpeed = 0.05f, .vMin = 0.01f, .vMax = 100000.0f });
            ImGuiCustom::EditValue(TranslationLabel("component.sceneviewcamerasettings.cameradistance2d"), cameraDistance2D, { .vSpeed = 1.0f, .vMin = 1.0f, .vMax = 1000000.0f });
            ImGui::TreePop();
        }
    }
#endif
};

REGISTER_COMPONENT_OBJECT(SceneViewCameraSettings)

} // namespace KashipanEngine

#endif // USE_IMGUI
