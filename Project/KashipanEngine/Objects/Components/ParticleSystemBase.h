#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/TargetLookAt.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Graphics/PipelineManager.h"
#include "Math/Vector3.h"
#include "Utilities/RandomValue.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#endif

namespace KashipanEngine {

/// @brief ParticleSystem2D / ParticleSystem3D 共通のパーティクル生成・寿命管理ロジックを持つ基底クラス
/// @details このクラス自体はコンポーネントとして登録されない（登録・COMPONENT_CATEGORYの実体は
///          このクラスで宣言し、Add Componentメニュー等では派生クラス名で表示される）。
///          毎フレーム一定間隔で自身の子オブジェクトとしてパーティクルを生成し、
///          Velocityコンポーネントで移動・重力を、寿命に応じたスケール変化を適用し、
///          寿命が来たら削除する。描画コンポーネントの追加方法（2D/3Dどちらを使うか）だけが
///          派生クラスごとに異なるため、生成直後のパーティクルへ描画コンポーネントを
///          追加するコールバック（setupVisual）だけを派生クラスから受け取る。
class ParticleSystemBase : public IObjectComponent {
public:
    COMPONENT_CATEGORY("Effect")

    /// @brief パーティクルの生成を開始する
    void Play() noexcept { isPlaying_ = true; }
    /// @brief パーティクルの生成を停止する（生成済みのパーティクルはそのまま寿命を迎えるまで残る）
    void Stop() noexcept { isPlaying_ = false; }
    bool IsPlaying() const noexcept { return isPlaying_; }
    /// @brief 現在生存中の全パーティクルを即座に削除する
    void Clear() {
        auto *sceneContext = GetOwnerSceneContext();
        for (auto &particle : particles_) {
            if (sceneContext && sceneContext->GetSceneObject(particle.object)) {
                sceneContext->DeleteObject(particle.object);
            }
        }
        particles_.clear();
        spawnTimer_ = 0.0f;
    }

    void SetEmissionRate(float rate) noexcept { emissionRate_ = rate; }
    float GetEmissionRate() const noexcept { return emissionRate_; }
    void SetMaxParticles(int count) noexcept { maxParticles_ = count; }
    int GetMaxParticles() const noexcept { return maxParticles_; }
    /// @brief ビルボード化（常に指定オブジェクトの方を向かせる）の有効/無効を設定する
    void SetBillboard(bool enabled) noexcept { billboard_ = enabled; }
    bool IsBillboard() const noexcept { return billboard_; }
    /// @brief ビルボードの向き先オブジェクトを設定する（未設定の場合はシーン内のカメラを自動で使う）
    void SetBillboardTarget(const EmptyObject *targetObject) {
        billboardTargetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    void SetBillboardTarget(const UUID128 &targetObjectID) { billboardTargetObjectID_ = targetObjectID; }
    const UUID128 &GetBillboardTargetObjectID() const noexcept { return billboardTargetObjectID_; }
    EmptyObject *GetBillboardTarget() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !billboardTargetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(billboardTargetObjectID_);
    }
    /// @brief ビルボードの向き方（TargetLookAtと同じ2種類）を設定する
    void SetBillboardRotationMode(TargetLookAt::RotationMode mode) noexcept { billboardRotationMode_ = mode; }
    TargetLookAt::RotationMode GetBillboardRotationMode() const noexcept { return billboardRotationMode_; }

protected:
    /// @brief ビルボード化（常にカメラの方を向かせる）に使うカメラオブジェクトを解決する
    /// @details 2D/3Dでどのカメラコンポーネント（Camera2D/Camera3D）を探すかが異なるため、
    ///          派生クラスで実装する。見つからない場合は nullptr を返せばよい
    ///          （その場合パーティクルは向きを変えない）
    virtual EmptyObject *ResolveBillboardCameraObject() const = 0;

    ParticleSystemBase(const std::string &typeName, size_t maxCount, size_t componentTypeID)
        : IObjectComponent(typeName, maxCount, componentTypeID) {
        ADD_MEMBER_VARIABLE(emissionRate_);
        ADD_MEMBER_VARIABLE(maxParticles_);
        ADD_MEMBER_VARIABLE(lifetimeMin_);
        ADD_MEMBER_VARIABLE(lifetimeMax_);
        ADD_MEMBER_VARIABLE(gravity_);
        ADD_MEMBER_VARIABLE(startScale_);
        ADD_MEMBER_VARIABLE(endScale_);
        ADD_MEMBER_VARIABLE(billboard_);
    }

    /// @brief 派生クラスのInitializeから呼ぶ
    void InitializeBase() {
        if (playOnStart_) isPlaying_ = true;
    }
    /// @brief 派生クラスのFinalizeから呼ぶ（生存中のパーティクルを全て削除する）
    void FinalizeBase() {
        Clear();
    }

    /// @brief 派生クラスのUpdateから呼ぶ。新規生成した子オブジェクトへ描画コンポーネントを
    ///        追加してもらうため、生成直後に setupVisual(生成したEmptyObject*) を呼び出す
    void UpdateParticles(const std::function<void(EmptyObject *)> &setupVisual) {
        const float dt = GetDeltaTime();
        auto *sceneContext = GetOwnerSceneContext();

        // --- 発生 ---
        if (isPlaying_ && emissionRate_ > 0.0f) {
            const float interval = 1.0f / emissionRate_;
            spawnTimer_ += dt;
            while (spawnTimer_ >= interval) {
                const bool underLiveCap = static_cast<int>(particles_.size()) < maxParticles_;
                const bool underTotalCap = loop_ || totalEmittedCount_ < maxParticles_;
                if (!underTotalCap) {
                    // ループしない場合は総生成数の上限に達したら発生を止める
                    isPlaying_ = false;
                    break;
                }
                spawnTimer_ -= interval;
                if (underLiveCap) SpawnParticle(setupVisual);
            }
        }

        // --- 寿命管理・スケール変化 ---
        for (size_t i = 0; i < particles_.size();) {
            auto &particle = particles_[i];
            if (!sceneContext || !sceneContext->GetSceneObject(particle.object)) {
                // 何らかの理由で既に削除されている（エディタ操作等）
                particles_.erase(particles_.begin() + i);
                continue;
            }
            particle.age += dt;
            if (particle.age >= particle.lifetime) {
                sceneContext->DeleteObject(particle.object);
                particles_.erase(particles_.begin() + i);
                continue;
            }
            if (auto *transform = particle.object->GetComponent<Transform>()) {
                const float t = particle.lifetime > 0.0f ? particle.age / particle.lifetime : 1.0f;
                transform->SetScale(Vector3::Lerp(startScale_, endScale_, t));
            }
            ++i;
        }
    }

    /// @brief SerializeField相当の値を他インスタンスからコピーする（Cloneで使用。実行時状態はコピーしない）
    void CopyBaseFieldsFrom(const ParticleSystemBase &other) {
        playOnStart_ = other.playOnStart_;
        loop_ = other.loop_;
        emissionRate_ = other.emissionRate_;
        maxParticles_ = other.maxParticles_;
        lifetimeMin_ = other.lifetimeMin_;
        lifetimeMax_ = other.lifetimeMax_;
        initialVelocityMin_ = other.initialVelocityMin_;
        initialVelocityMax_ = other.initialVelocityMax_;
        gravity_ = other.gravity_;
        startScale_ = other.startScale_;
        endScale_ = other.endScale_;
        targetObjectID_ = other.targetObjectID_;
        meshAssetPath_ = other.meshAssetPath_;
        pipelineName_ = other.pipelineName_;
        materialName_ = other.materialName_;
        billboard_ = other.billboard_;
        billboardTargetObjectID_ = other.billboardTargetObjectID_;
        billboardRotationMode_ = other.billboardRotationMode_;
    }

    JSON SaveBaseFieldsJson() const {
        JSON json = JSON::object();
        json["playOnStart"] = playOnStart_;
        json["loop"] = loop_;
        json["emissionRate"] = emissionRate_;
        json["maxParticles"] = maxParticles_;
        json["lifetimeMin"] = lifetimeMin_;
        json["lifetimeMax"] = lifetimeMax_;
        json["initialVelocityMin"] = ToJSON(initialVelocityMin_);
        json["initialVelocityMax"] = ToJSON(initialVelocityMax_);
        json["gravity"] = ToJSON(gravity_);
        json["startScale"] = ToJSON(startScale_);
        json["endScale"] = ToJSON(endScale_);
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["meshAssetPath"] = meshAssetPath_;
        json["pipelineName"] = pipelineName_;
        json["materialName"] = materialName_;
        json["billboard"] = billboard_;
        json["billboardTargetObjectID"] = ToJSON(billboardTargetObjectID_);
        json["billboardRotationMode"] = static_cast<int>(billboardRotationMode_);
        return json;
    }

    void LoadBaseFieldsJson(const JSON &json) {
        playOnStart_ = json.value("playOnStart", true);
        loop_ = json.value("loop", true);
        emissionRate_ = json.value("emissionRate", 10.0f);
        maxParticles_ = json.value("maxParticles", 100);
        lifetimeMin_ = json.value("lifetimeMin", 1.0f);
        lifetimeMax_ = json.value("lifetimeMax", 1.0f);
        if (json.contains("initialVelocityMin")) initialVelocityMin_ = FromJSON<Vector3>(json["initialVelocityMin"]);
        if (json.contains("initialVelocityMax")) initialVelocityMax_ = FromJSON<Vector3>(json["initialVelocityMax"]);
        if (json.contains("gravity")) gravity_ = FromJSON<Vector3>(json["gravity"]);
        if (json.contains("startScale")) startScale_ = FromJSON<Vector3>(json["startScale"]);
        if (json.contains("endScale")) endScale_ = FromJSON<Vector3>(json["endScale"]);
        if (json.contains("targetObjectID")) targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        meshAssetPath_ = json.value("meshAssetPath", std::string{});
        pipelineName_ = json.value("pipelineName", std::string{});
        materialName_ = json.value("materialName", std::string("White"));
        billboard_ = json.value("billboard", false);
        if (json.contains("billboardTargetObjectID")) billboardTargetObjectID_ = FromJSON<UUID128>(json["billboardTargetObjectID"]);
        billboardRotationMode_ = static_cast<TargetLookAt::RotationMode>(json.value("billboardRotationMode", 0));
    }

#if defined(USE_IMGUI)
    void ShowBaseFieldsImGui() {
        bool isPlayingLocal = isPlaying_;
        if (ImGui::Checkbox("Playing", &isPlayingLocal)) {
            isPlayingLocal ? Play() : Stop();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) Clear();
        ImGui::Text("Live Particles: %zu", particles_.size());

        ImGui::Checkbox("Play On Start", &playOnStart_);
        ImGui::Checkbox("Loop", &loop_);
        ImGui::DragFloat("Emission Rate", &emissionRate_, 0.1f, 0.0f, 1000.0f);
        ImGui::DragInt("Max Particles", &maxParticles_, 1.0f, 0, 10000);
        ImGui::DragFloat("Lifetime Min", &lifetimeMin_, 0.01f, 0.0f, 60.0f);
        ImGui::DragFloat("Lifetime Max", &lifetimeMax_, 0.01f, 0.0f, 60.0f);
        ImGui::DragFloat3("Initial Velocity Min", &initialVelocityMin_.x, 0.01f);
        ImGui::DragFloat3("Initial Velocity Max", &initialVelocityMax_.x, 0.01f);
        ImGui::DragFloat3("Gravity", &gravity_.x, 0.01f);
        ImGui::DragFloat3("Start Scale", &startScale_.x, 0.01f);
        ImGui::DragFloat3("End Scale", &endScale_.x, 0.01f);

        ImGui::SeparatorText("Rendering");
        // 描画先はシーン上のオブジェクトから選択（ヒエラルキーからのD&Dも受け付ける）
        TargetObjectSelector::ShowSelector("Target", GetOwnerSceneContext(), targetObjectID_);

        // メッシュ・パイプライン・マテリアルは、素の文字列入力ではなく
        // 読み込み済みのものから選択する（MeshFilter/SpriteRendererと同じ方式）
        std::vector<std::string> modelPaths;
        for (const auto &entry : ModelManager::GetLoadedModelListEntries()) {
            modelPaths.push_back(entry.assetPath);
        }
        ImGuiCustom::SelectString("Mesh", meshAssetPath_, modelPaths, true);
        ImGuiCustom::SelectString("Pipeline", pipelineName_, PipelineManager::GetLoadedRenderPipelineNames(), true);
        std::vector<std::string> materialNames;
        for (const auto &entry : MaterialManager::GetLoadedMaterialListEntries()) {
            materialNames.push_back(entry.material.name);
        }
        ImGuiCustom::SelectString("Material", materialName_, materialNames);

        ImGui::Checkbox("Billboard", &billboard_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("有効にすると、生成した各パーティクルが指定オブジェクトの方を向く（TargetLookAtを利用）");
        }
        if (billboard_) {
            ImGui::Indent();
            // 向き先を明示的に指定しない場合は、シーン内のカメラを自動で使う
            TargetObjectSelector::ShowSelector("Billboard Target", GetOwnerSceneContext(), billboardTargetObjectID_, true, false);
            if (!billboardTargetObjectID_.IsValid()) {
                ImGui::TextDisabled("(未設定: シーン内のカメラを自動で使用)");
            }

            static const char *kModeLabels[] = { "Sync Target Rotation", "Look At Target" };
            int mode = static_cast<int>(billboardRotationMode_);
            if (ImGui::Combo("Rotation Mode", &mode, kModeLabels, 2)) {
                billboardRotationMode_ = static_cast<TargetLookAt::RotationMode>(mode);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Sync Target Rotation: 向き先のワールド回転と同期する（カメラ向けのビルボード）\nLook At Target: 自身の+Z軸が常に向き先の方向を向く");
            }
            ImGui::Unindent();
        }
    }
#endif

    // 再生設定
    bool playOnStart_ = true;
    bool loop_ = true;
    bool isPlaying_ = false;

    // 発生設定
    float emissionRate_ = 10.0f;
    int maxParticles_ = 100;

    // 寿命（秒、min~maxの範囲でランダム）
    float lifetimeMin_ = 1.0f;
    float lifetimeMax_ = 1.0f;

    // 初速（各軸、min~maxの範囲でランダム）
    Vector3 initialVelocityMin_{ -1.0f, 1.0f, -1.0f };
    Vector3 initialVelocityMax_{ 1.0f, 3.0f, 1.0f };

    // 重力・加速度（生成したパーティクルのVelocityコンポーネントへそのまま渡す）
    Vector3 gravity_{ 0.0f, 0.0f, 0.0f };

    // 寿命に応じたスケール変化（開始スケール→終了スケールへ線形補間）
    Vector3 startScale_{ 1.0f, 1.0f, 1.0f };
    Vector3 endScale_{ 0.0f, 0.0f, 0.0f };

    // 描画設定
    UUID128 targetObjectID_;
    std::string meshAssetPath_;
    std::string pipelineName_;
    std::string materialName_ = "White";
    /// @brief 有効にすると、生成した各パーティクルが billboardTargetObjectID_ （未設定ならカメラ）の方を向く
    bool billboard_ = false;
    /// @brief ビルボードの向き先オブジェクト（未設定/無効な場合はシーン内のカメラを自動で使う）
    UUID128 billboardTargetObjectID_;
    /// @brief ビルボードの向き方（TargetLookAtと同じ2種類）
    TargetLookAt::RotationMode billboardRotationMode_ = TargetLookAt::RotationMode::SyncRotation;

private:
    /// @brief 自身のオーナーオブジェクトを、子オブジェクトの親付けに使える可変ポインタとして解決する
    EmptyObject *ResolveMutableOwner() const {
        auto *sceneContext = GetOwnerSceneContext();
        const auto *owner = GetOwnerObject();
        if (!sceneContext || !owner) return nullptr;
        return sceneContext->GetSceneObject(owner->GetObjectID());
    }

    void SpawnParticle(const std::function<void(EmptyObject *)> &setupVisual) {
        auto *sceneContext = GetOwnerSceneContext();
        auto *owner = ResolveMutableOwner();
        if (!sceneContext || !owner) return;

        auto *particleObj = sceneContext->CreateEmptyObject("Particle");
        if (!particleObj) return;

        if (auto *transform = particleObj->GetComponent<Transform>()) {
            transform->SetParentObject(owner);
            transform->SetScale(startScale_);
        }

        if (setupVisual) setupVisual(particleObj);

        const Vector3 initialVelocity(
            GetRandomFloat(initialVelocityMin_.x, initialVelocityMax_.x),
            GetRandomFloat(initialVelocityMin_.y, initialVelocityMax_.y),
            GetRandomFloat(initialVelocityMin_.z, initialVelocityMax_.z));
        if (auto *velocity = particleObj->AddComponent<Velocity>()) {
            velocity->SetVelocity(initialVelocity);
            velocity->SetAcceleration(gravity_);
        }

        if (billboard_) {
            if (auto *lookAt = particleObj->AddComponent<TargetLookAt>()) {
                lookAt->SetRotationMode(billboardRotationMode_);
                // 向き先が明示的に指定されていればそれを使い、未設定ならシーン内のカメラを自動で使う
                EmptyObject *billboardTarget = GetBillboardTarget();
                if (!billboardTarget) billboardTarget = ResolveBillboardCameraObject();
                if (billboardTarget) lookAt->SetTargetObject(billboardTarget);
            }
        }

        ParticleInstance instance;
        instance.object = particleObj;
        instance.lifetime = std::max(0.01f, GetRandomFloat(lifetimeMin_, lifetimeMax_));
        instance.age = 0.0f;
        particles_.push_back(instance);
        ++totalEmittedCount_;
    }

    float spawnTimer_ = 0.0f;
    int totalEmittedCount_ = 0;
    struct ParticleInstance {
        EmptyObject *object = nullptr;
        float age = 0.0f;
        float lifetime = 1.0f;
    };
    std::vector<ParticleInstance> particles_;
};

} // namespace KashipanEngine
