#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Rotation.h"
#include "Objects/Components/TargetLookAt.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Graphics/PipelineManager.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils.h"
#include "Utilities/RandomizableValue.h"
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

    /// @brief パーティクルのスポーン範囲の形状
    /// @details Min/Maxの2つの同形状（同じ中心・向き）の間の領域（内側の形状の外側かつ外側の形状の内側）にスポーンする。
    ///          Min側のサイズ・半径を0にすれば、内側が空洞化しない通常の塗りつぶし範囲になる。
    ///          2D（ParticleSystem2D）では Box は Rect、Sphere は Circle として扱われ（Z軸は無視）、Capsuleは選択できない
    enum class SpawnShape {
        Point,
        Box,
        Sphere,
        Capsule,
    };

    /// @brief パーティクルオブジェクトの生成方法
    enum class SpawnOrigin {
        /// @brief 自身（コンポーネントが付いているオブジェクト）の子オブジェクトとして生成する
        ChildOfSelf,
        /// @brief 指定した他オブジェクトの子オブジェクトとして生成する（未指定/無効な場合はChildOfSelfと同様に振る舞う）
        ChildOfOther,
        /// @brief 自身の現在のワールド座標を起点に、どの親にも属さず生成する（生成後は自身が動いても追従しない）
        AtOwnerPosition,
        /// @brief 指定したワールド座標を起点に、どの親にも属さず生成する
        AtFixedPosition,
    };

    /// @brief パーティクルの生成を開始する（停止中に呼んだ場合、総発生数カウントをリセットする）
    void Play() noexcept {
        if (!isPlaying_) totalEmittedCount_ = 0;
        isPlaying_ = true;
    }
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
        totalEmittedCount_ = 0;
    }

    void SetEmissionRate(float rate) noexcept { emissionRate_ = rate; }
    float GetEmissionRate() const noexcept { return emissionRate_; }
    void SetMaxParticles(int count) noexcept { maxParticles_ = count; }
    int GetMaxParticles() const noexcept { return maxParticles_; }
    /// @brief Loopが無効の場合に、この数だけスポーンしたら自動的に再生を停止する
    void SetTotalSpawnCount(int count) noexcept { totalSpawnCount_ = count; }
    int GetTotalSpawnCount() const noexcept { return totalSpawnCount_; }
    /// @brief パーティクルオブジェクトの生成方法を設定する
    void SetSpawnOrigin(SpawnOrigin origin) noexcept { spawnOrigin_ = origin; }
    SpawnOrigin GetSpawnOrigin() const noexcept { return spawnOrigin_; }
    /// @brief SpawnOrigin::ChildOfOther で親にする対象オブジェクトを設定する
    void SetSpawnParentObject(const EmptyObject *targetObject) {
        spawnParentObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    void SetSpawnParentObject(const UUID128 &targetObjectID) { spawnParentObjectID_ = targetObjectID; }
    const UUID128 &GetSpawnParentObjectID() const noexcept { return spawnParentObjectID_; }
    /// @brief SpawnOrigin::AtFixedPosition で使用する生成先のワールド座標を設定する
    void SetFixedSpawnPosition(const Vector3 &position) noexcept { fixedSpawnPosition_ = position; }
    const Vector3 &GetFixedSpawnPosition() const noexcept { return fixedSpawnPosition_; }
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

    /// @param is2D trueの場合、スポーン形状の選択肢がBox/Sphere+CapsuleではなくRect/Circle（Capsuleなし）になる
    ParticleSystemBase(const std::string &typeName, size_t maxCount, size_t componentTypeID, bool is2D)
        : IObjectComponent(typeName, maxCount, componentTypeID), is2D_(is2D) {
        ADD_MEMBER_VARIABLE(emissionRate_);
        ADD_MEMBER_VARIABLE(maxParticles_);
        ADD_MEMBER_VARIABLE(totalSpawnCount_);
        ADD_MEMBER_VARIABLE(billboard_);
        ADD_MEMBER_VARIABLE(spawnCenter_);
        ADD_MEMBER_VARIABLE(spawnBoxSizeMin_);
        ADD_MEMBER_VARIABLE(spawnBoxSizeMax_);
        ADD_MEMBER_VARIABLE(spawnSphereRadiusMin_);
        ADD_MEMBER_VARIABLE(spawnSphereRadiusMax_);
        ADD_MEMBER_VARIABLE(spawnCapsuleRadiusMin_);
        ADD_MEMBER_VARIABLE(spawnCapsuleRadiusMax_);
        ADD_MEMBER_VARIABLE(spawnCapsuleHeightMin_);
        ADD_MEMBER_VARIABLE(spawnCapsuleHeightMax_);
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
                spawnTimer_ -= interval;
                // 一回のスポーンで生成する数（ランダム範囲を指定可能）
                const int count = std::max(1, spawnCount_.Get());
                for (int i = 0; i < count; ++i) {
                    if (!loop_ && totalEmittedCount_ >= totalSpawnCount_) {
                        // ループしない場合は、指定した総生成数に達したら発生を止める
                        isPlaying_ = false;
                        break;
                    }
                    if (static_cast<int>(particles_.size()) < maxParticles_) {
                        SpawnParticle(setupVisual);
                    }
                }
                if (!isPlaying_) break;
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
                transform->SetScale(Vector3::Lerp(particle.startScale, particle.endScale, t));
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
        totalSpawnCount_ = other.totalSpawnCount_;
        lifetime_ = other.lifetime_;
        initialVelocity_ = other.initialVelocity_;
        acceleration_ = other.acceleration_;
        startScale_ = other.startScale_;
        endScale_ = other.endScale_;
        spawnCount_ = other.spawnCount_;
        initialRotation_ = other.initialRotation_;
        initialRotationSpeed_ = other.initialRotationSpeed_;
        rotationAcceleration_ = other.rotationAcceleration_;
        spawnOrigin_ = other.spawnOrigin_;
        spawnParentObjectID_ = other.spawnParentObjectID_;
        fixedSpawnPosition_ = other.fixedSpawnPosition_;
        targetObjectID_ = other.targetObjectID_;
        meshAssetPath_ = other.meshAssetPath_;
        pipelineName_ = other.pipelineName_;
        materialName_ = other.materialName_;
        billboard_ = other.billboard_;
        billboardTargetObjectID_ = other.billboardTargetObjectID_;
        billboardRotationMode_ = other.billboardRotationMode_;
        spawnShape_ = other.spawnShape_;
        spawnCenter_ = other.spawnCenter_;
        spawnBoxSizeMin_ = other.spawnBoxSizeMin_;
        spawnBoxSizeMax_ = other.spawnBoxSizeMax_;
        spawnSphereRadiusMin_ = other.spawnSphereRadiusMin_;
        spawnSphereRadiusMax_ = other.spawnSphereRadiusMax_;
        spawnCapsuleRadiusMin_ = other.spawnCapsuleRadiusMin_;
        spawnCapsuleRadiusMax_ = other.spawnCapsuleRadiusMax_;
        spawnCapsuleHeightMin_ = other.spawnCapsuleHeightMin_;
        spawnCapsuleHeightMax_ = other.spawnCapsuleHeightMax_;
    }

    JSON SaveBaseFieldsJson() const {
        JSON json = JSON::object();
        json["playOnStart"] = playOnStart_;
        json["loop"] = loop_;
        json["emissionRate"] = emissionRate_;
        json["maxParticles"] = maxParticles_;
        json["totalSpawnCount"] = totalSpawnCount_;
        json["lifetime"] = ToJSON(lifetime_);
        json["initialVelocity"] = ToJSON(initialVelocity_);
        json["acceleration"] = ToJSON(acceleration_);
        json["startScale"] = ToJSON(startScale_);
        json["endScale"] = ToJSON(endScale_);
        json["spawnCount"] = ToJSON(spawnCount_);
        json["initialRotation"] = ToJSON(initialRotation_);
        json["initialRotationSpeed"] = ToJSON(initialRotationSpeed_);
        json["rotationAcceleration"] = ToJSON(rotationAcceleration_);
        json["spawnOrigin"] = static_cast<int>(spawnOrigin_);
        json["spawnParentObjectID"] = ToJSON(spawnParentObjectID_);
        json["fixedSpawnPosition"] = ToJSON(fixedSpawnPosition_);
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["meshAssetPath"] = meshAssetPath_;
        json["pipelineName"] = pipelineName_;
        json["materialName"] = materialName_;
        json["billboard"] = billboard_;
        json["billboardTargetObjectID"] = ToJSON(billboardTargetObjectID_);
        json["billboardRotationMode"] = static_cast<int>(billboardRotationMode_);
        json["spawnShape"] = static_cast<int>(spawnShape_);
        json["spawnCenter"] = ToJSON(spawnCenter_);
        json["spawnBoxSizeMin"] = ToJSON(spawnBoxSizeMin_);
        json["spawnBoxSizeMax"] = ToJSON(spawnBoxSizeMax_);
        json["spawnSphereRadiusMin"] = spawnSphereRadiusMin_;
        json["spawnSphereRadiusMax"] = spawnSphereRadiusMax_;
        json["spawnCapsuleRadiusMin"] = spawnCapsuleRadiusMin_;
        json["spawnCapsuleRadiusMax"] = spawnCapsuleRadiusMax_;
        json["spawnCapsuleHeightMin"] = spawnCapsuleHeightMin_;
        json["spawnCapsuleHeightMax"] = spawnCapsuleHeightMax_;
        return json;
    }

    void LoadBaseFieldsJson(const JSON &json) {
        playOnStart_ = json.value("playOnStart", true);
        loop_ = json.value("loop", true);
        emissionRate_ = json.value("emissionRate", 10.0f);
        maxParticles_ = json.value("maxParticles", 100);
        totalSpawnCount_ = json.value("totalSpawnCount", 100);
        if (json.contains("lifetime")) lifetime_ = FromJSON<RandomizableValue<float>>(json["lifetime"]);
        if (json.contains("initialVelocity")) initialVelocity_ = FromJSON<RandomizableValue<Vector3>>(json["initialVelocity"]);
        if (json.contains("acceleration")) acceleration_ = FromJSON<RandomizableValue<Vector3>>(json["acceleration"]);
        if (json.contains("startScale")) startScale_ = FromJSON<RandomizableValue<Vector3>>(json["startScale"]);
        if (json.contains("endScale")) endScale_ = FromJSON<RandomizableValue<Vector3>>(json["endScale"]);
        if (json.contains("spawnCount")) spawnCount_ = FromJSON<RandomizableValue<int>>(json["spawnCount"]);
        if (json.contains("initialRotation")) initialRotation_ = FromJSON<RandomizableValue<Vector3>>(json["initialRotation"]);
        if (json.contains("initialRotationSpeed")) initialRotationSpeed_ = FromJSON<RandomizableValue<Vector3>>(json["initialRotationSpeed"]);
        if (json.contains("rotationAcceleration")) rotationAcceleration_ = FromJSON<RandomizableValue<Vector3>>(json["rotationAcceleration"]);
        spawnOrigin_ = static_cast<SpawnOrigin>(json.value("spawnOrigin", 0));
        if (json.contains("spawnParentObjectID")) spawnParentObjectID_ = FromJSON<UUID128>(json["spawnParentObjectID"]);
        if (json.contains("fixedSpawnPosition")) fixedSpawnPosition_ = FromJSON<Vector3>(json["fixedSpawnPosition"]);
        if (json.contains("targetObjectID")) targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        meshAssetPath_ = json.value("meshAssetPath", std::string{});
        pipelineName_ = json.value("pipelineName", std::string{});
        materialName_ = json.value("materialName", std::string("White"));
        billboard_ = json.value("billboard", false);
        if (json.contains("billboardTargetObjectID")) billboardTargetObjectID_ = FromJSON<UUID128>(json["billboardTargetObjectID"]);
        billboardRotationMode_ = static_cast<TargetLookAt::RotationMode>(json.value("billboardRotationMode", 0));
        spawnShape_ = static_cast<SpawnShape>(json.value("spawnShape", 0));
        if (json.contains("spawnCenter")) spawnCenter_ = FromJSON<Vector3>(json["spawnCenter"]);
        if (json.contains("spawnBoxSizeMin")) spawnBoxSizeMin_ = FromJSON<Vector3>(json["spawnBoxSizeMin"]);
        if (json.contains("spawnBoxSizeMax")) spawnBoxSizeMax_ = FromJSON<Vector3>(json["spawnBoxSizeMax"]);
        spawnSphereRadiusMin_ = json.value("spawnSphereRadiusMin", 0.0f);
        spawnSphereRadiusMax_ = json.value("spawnSphereRadiusMax", 1.0f);
        spawnCapsuleRadiusMin_ = json.value("spawnCapsuleRadiusMin", 0.0f);
        spawnCapsuleRadiusMax_ = json.value("spawnCapsuleRadiusMax", 0.5f);
        spawnCapsuleHeightMin_ = json.value("spawnCapsuleHeightMin", 0.0f);
        spawnCapsuleHeightMax_ = json.value("spawnCapsuleHeightMax", 1.0f);
    }

#if defined(USE_IMGUI)
    /// @brief 「ランダムにするか否か」のトグルと、固定値 or Min/Max範囲の入力を表示する（float版）
    void ShowRandomizableImGui(const char *label, RandomizableValue<float> &v, float speed, float vMin = 0.0f, float vMax = 0.0f) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::Checkbox("Random", &v.randomize);
        if (v.randomize) {
            ImGui::DragFloat("Min", &v.min, speed, vMin, vMax);
            ImGui::DragFloat("Max", &v.max, speed, vMin, vMax);
        } else {
            ImGui::DragFloat("Value", &v.value, speed, vMin, vMax);
        }
        ImGui::PopID();
    }

    /// @brief 「ランダムにするか否か」のトグルと、固定値 or Min/Max範囲の入力を表示する（int版）
    void ShowRandomizableImGui(const char *label, RandomizableValue<int> &v, int vMin = 0, int vMax = 0) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::Checkbox("Random", &v.randomize);
        if (v.randomize) {
            ImGui::DragInt("Min", &v.min, 1.0f, vMin, vMax);
            ImGui::DragInt("Max", &v.max, 1.0f, vMin, vMax);
        } else {
            ImGui::DragInt("Value", &v.value, 1.0f, vMin, vMax);
        }
        ImGui::PopID();
    }

    /// @brief 「ランダムにするか否か」のトグルと、固定値 or Min/Max範囲の入力を表示する（Vector3版）
    void ShowRandomizableImGui(const char *label, RandomizableValue<Vector3> &v, float speed) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::Checkbox("Random", &v.randomize);
        if (v.randomize) {
            ImGui::DragFloat3("Min", &v.min.x, speed);
            ImGui::DragFloat3("Max", &v.max.x, speed);
        } else {
            ImGui::DragFloat3("Value", &v.value.x, speed);
        }
        ImGui::PopID();
    }

    /// @brief スポーン範囲（形状・Min/Max）のImGui表示
    void ShowSpawnShapeImGui() {
        ImGui::SeparatorText("Spawn Area");
        if (is2D_) {
            static const char *kShapeLabels[] = { "Point", "Rect", "Circle" };
            int shapeIndex = static_cast<int>(spawnShape_);
            if (ImGui::Combo("Spawn Shape", &shapeIndex, kShapeLabels, 3)) {
                spawnShape_ = static_cast<SpawnShape>(shapeIndex);
            }
        } else {
            static const char *kShapeLabels[] = { "Point", "Box", "Sphere", "Capsule" };
            int shapeIndex = static_cast<int>(spawnShape_);
            if (ImGui::Combo("Spawn Shape", &shapeIndex, kShapeLabels, 4)) {
                spawnShape_ = static_cast<SpawnShape>(shapeIndex);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Min/Max間の範囲（内側の形状の外側かつ外側の形状の内側）にスポーンする。Minを0にすれば通常の塗りつぶし範囲になる");
        }

        switch (spawnShape_) {
        case SpawnShape::Point:
            break;
        case SpawnShape::Box:
            ImGui::DragFloat3("Spawn Center", &spawnCenter_.x, 0.01f);
            if (is2D_) {
                ImGui::DragFloat2("Min Size", &spawnBoxSizeMin_.x, 0.01f, 0.0f);
                ImGui::DragFloat2("Max Size", &spawnBoxSizeMax_.x, 0.01f, 0.0f);
            } else {
                ImGui::DragFloat3("Min Size", &spawnBoxSizeMin_.x, 0.01f, 0.0f);
                ImGui::DragFloat3("Max Size", &spawnBoxSizeMax_.x, 0.01f, 0.0f);
            }
            break;
        case SpawnShape::Sphere:
            ImGui::DragFloat3("Spawn Center", &spawnCenter_.x, 0.01f);
            ImGui::DragFloat("Min Radius", &spawnSphereRadiusMin_, 0.01f, 0.0f);
            ImGui::DragFloat("Max Radius", &spawnSphereRadiusMax_, 0.01f, 0.0f);
            break;
        case SpawnShape::Capsule:
            ImGui::DragFloat3("Spawn Center", &spawnCenter_.x, 0.01f);
            ImGui::DragFloat("Min Radius", &spawnCapsuleRadiusMin_, 0.01f, 0.0f);
            ImGui::DragFloat("Max Radius", &spawnCapsuleRadiusMax_, 0.01f, 0.0f);
            ImGui::DragFloat("Min Height", &spawnCapsuleHeightMin_, 0.01f, 0.0f);
            ImGui::DragFloat("Max Height", &spawnCapsuleHeightMax_, 0.01f, 0.0f);
            break;
        }
    }

    /// @brief パーティクルオブジェクトの生成方法（親付け・生成位置）のImGui表示
    void ShowSpawnOriginImGui() {
        ImGui::SeparatorText("Spawn Origin");
        static const char *kOriginLabels[] = {
            "Child of Self", "Child of Other Object", "At Owner Position (Unparented)", "At Fixed Position",
        };
        int originIndex = static_cast<int>(spawnOrigin_);
        if (ImGui::Combo("Origin", &originIndex, kOriginLabels, 4)) {
            spawnOrigin_ = static_cast<SpawnOrigin>(originIndex);
        }
        switch (spawnOrigin_) {
        case SpawnOrigin::ChildOfOther:
            TargetObjectSelector::ShowSelector("Parent Object", GetOwnerSceneContext(), spawnParentObjectID_, true, false);
            if (!spawnParentObjectID_.IsValid()) {
                ImGui::TextDisabled("(未設定: 自身の子として生成されます)");
            }
            break;
        case SpawnOrigin::AtFixedPosition:
            ImGui::DragFloat3("Fixed Position", &fixedSpawnPosition_.x, 0.01f);
            break;
        case SpawnOrigin::ChildOfSelf:
        case SpawnOrigin::AtOwnerPosition:
        default:
            break;
        }
    }

    void ShowBaseFieldsImGui() {
        bool isPlayingLocal = isPlaying_;
        if (ImGui::Checkbox("Playing", &isPlayingLocal)) {
            isPlayingLocal ? Play() : Stop();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) Clear();
        ImGui::Text("Live Particles: %zu", particles_.size());
        ImGui::Text("Total Emitted: %d", totalEmittedCount_);

        ImGui::Checkbox("Play On Start", &playOnStart_);
        ImGui::Checkbox("Loop", &loop_);
        ImGui::DragFloat("Emission Rate", &emissionRate_, 0.1f, 0.0f, 1000.0f);
        ImGui::DragInt("Max Particles", &maxParticles_, 1.0f, 0, 10000);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("同時に生存できるパーティクルの最大数");
        }
        ImGui::BeginDisabled(loop_);
        ImGui::DragInt("Total Spawn Count", &totalSpawnCount_, 1.0f, 0, 1000000);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Loopが無効の場合、この数だけスポーンすると自動的に再生を停止する");
        }

        ShowRandomizableImGui("Spawn Count", spawnCount_, 1, 100);
        ShowRandomizableImGui("Lifetime", lifetime_, 0.01f, 0.0f, 60.0f);
        ShowRandomizableImGui("Initial Velocity", initialVelocity_, 0.01f);
        ShowRandomizableImGui("Acceleration", acceleration_, 0.01f);
        ShowRandomizableImGui("Start Scale", startScale_, 0.01f);
        ShowRandomizableImGui("End Scale", endScale_, 0.01f);
        ShowRandomizableImGui("Initial Rotation (deg)", initialRotation_, 0.5f);
        ShowRandomizableImGui("Initial Rotation Speed (deg/s)", initialRotationSpeed_, 0.5f);
        ShowRandomizableImGui("Rotation Acceleration (deg/s^2)", rotationAcceleration_, 0.5f);

        ShowSpawnShapeImGui();
        ShowSpawnOriginImGui();

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
    /// @brief 同時に生存できるパーティクルの最大数
    int maxParticles_ = 100;
    /// @brief Loopが無効の場合に、この数だけスポーンしたら自動的に再生を停止する（Max Particlesとは独立した「総発生数」の上限）
    int totalSpawnCount_ = 100;
    /// @brief 一回のスポーンで生成するオブジェクト数
    RandomizableValue<int> spawnCount_{ false, 1, 1, 1 };

    // 寿命（秒）
    RandomizableValue<float> lifetime_{ false, 1.0f, 1.0f, 1.0f };

    // 初速（各軸。既定ではMin/Max間でランダム）
    RandomizableValue<Vector3> initialVelocity_{ true, Vector3(0.0f, 1.0f, 0.0f), Vector3(-1.0f, 1.0f, -1.0f), Vector3(1.0f, 3.0f, 1.0f) };

    // 加速度（生成したパーティクルのVelocityコンポーネントへそのまま渡す）
    RandomizableValue<Vector3> acceleration_{ false, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };

    // 寿命に応じたスケール変化（開始スケール→終了スケールへ線形補間。値はパーティクルごとに1回抽選される）
    RandomizableValue<Vector3> startScale_{ false, Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f) };
    RandomizableValue<Vector3> endScale_{ false, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };

    // 初期回転・初期回転速度・回転加速度（度数法。Transform/Rotationコンポーネントへ渡す際にラジアンへ変換する）
    RandomizableValue<Vector3> initialRotation_{ false, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };
    RandomizableValue<Vector3> initialRotationSpeed_{ false, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };
    RandomizableValue<Vector3> rotationAcceleration_{ false, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f) };

    /// @brief パーティクルオブジェクトの生成方法
    SpawnOrigin spawnOrigin_ = SpawnOrigin::ChildOfSelf;
    /// @brief SpawnOrigin::ChildOfOther で親にする対象オブジェクト
    UUID128 spawnParentObjectID_;
    /// @brief SpawnOrigin::AtFixedPosition で使用する生成先のワールド座標
    Vector3 fixedSpawnPosition_{ 0.0f, 0.0f, 0.0f };

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

    // スポーン範囲（Point以外はMin/Max間の領域にランダムスポーンする）
    SpawnShape spawnShape_ = SpawnShape::Point;
    Vector3 spawnCenter_{ 0.0f, 0.0f, 0.0f };
    // Box/Rect: 全辺のサイズ（中心から±size/2の範囲）
    Vector3 spawnBoxSizeMin_{ 0.0f, 0.0f, 0.0f };
    Vector3 spawnBoxSizeMax_{ 1.0f, 1.0f, 1.0f };
    // Sphere/Circle: 半径
    float spawnSphereRadiusMin_ = 0.0f;
    float spawnSphereRadiusMax_ = 1.0f;
    // Capsule: 半径・高さ（円柱部分の長さ。Y軸方向固定）
    float spawnCapsuleRadiusMin_ = 0.0f;
    float spawnCapsuleRadiusMax_ = 0.5f;
    float spawnCapsuleHeightMin_ = 0.0f;
    float spawnCapsuleHeightMax_ = 1.0f;

private:
    /// @brief 2Dパーティクルシステムかどうか（true: Rect/Circleのみ選択可、Capsule非対応）
    const bool is2D_;

    /// @brief 現在のスポーン形状設定に基づき、オーナー位置からのランダムなオフセットを求める
    Vector3 ComputeSpawnOffset() const {
        switch (spawnShape_) {
        case SpawnShape::Box: {
            Vector3 halfMin = spawnBoxSizeMin_ * 0.5f;
            Vector3 halfMax = spawnBoxSizeMax_ * 0.5f;
            if (is2D_) {
                halfMin.z = 0.0f;
                halfMax.z = 0.0f;
            }
            return spawnCenter_ + RandomPointInBoxShell(halfMin, halfMax);
        }
        case SpawnShape::Sphere:
            if (is2D_) {
                const Vector2 p = RandomPointInCircleShell(spawnSphereRadiusMin_, spawnSphereRadiusMax_);
                return spawnCenter_ + Vector3(p.x, p.y, 0.0f);
            }
            return spawnCenter_ + RandomPointInSphereShell(spawnSphereRadiusMin_, spawnSphereRadiusMax_);
        case SpawnShape::Capsule:
            return spawnCenter_ + RandomPointInCapsuleShell(spawnCapsuleRadiusMin_, spawnCapsuleRadiusMax_, spawnCapsuleHeightMin_, spawnCapsuleHeightMax_);
        case SpawnShape::Point:
        default:
            return spawnCenter_;
        }
    }

    /// @brief 内側のBox（minHalf）の外側かつ外側のBox（maxHalf）の内側にあるランダムな点を返す（各half成分は絶対値化・min<=maxに補正される）
    static Vector3 RandomPointInBoxShell(Vector3 minHalf, Vector3 maxHalf) {
        minHalf.x = std::abs(minHalf.x); minHalf.y = std::abs(minHalf.y); minHalf.z = std::abs(minHalf.z);
        maxHalf.x = std::abs(maxHalf.x); maxHalf.y = std::abs(maxHalf.y); maxHalf.z = std::abs(maxHalf.z);
        if (minHalf.x > maxHalf.x) std::swap(minHalf.x, maxHalf.x);
        if (minHalf.y > maxHalf.y) std::swap(minHalf.y, maxHalf.y);
        if (minHalf.z > maxHalf.z) std::swap(minHalf.z, maxHalf.z);

        auto sampleInMax = [&]() {
            return Vector3(
                GetRandomFloat(-maxHalf.x, maxHalf.x),
                GetRandomFloat(-maxHalf.y, maxHalf.y),
                GetRandomFloat(-maxHalf.z, maxHalf.z));
        };

        constexpr int kMaxAttempts = 16;
        Vector3 p;
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            p = sampleInMax();
            if (std::abs(p.x) > minHalf.x || std::abs(p.y) > minHalf.y || std::abs(p.z) > minHalf.z) {
                return p;
            }
        }
        // 内箱と外箱がほぼ同サイズ等で外れなかった場合、最も余裕の少ない軸の面へ押し出す
        const float marginX = minHalf.x - std::abs(p.x);
        const float marginY = minHalf.y - std::abs(p.y);
        const float marginZ = minHalf.z - std::abs(p.z);
        if (marginX <= marginY && marginX <= marginZ) {
            p.x = (p.x >= 0.0f) ? minHalf.x : -minHalf.x;
        } else if (marginY <= marginZ) {
            p.y = (p.y >= 0.0f) ? minHalf.y : -minHalf.y;
        } else {
            p.z = (p.z >= 0.0f) ? minHalf.z : -minHalf.z;
        }
        return p;
    }

    /// @brief 半径minRadius〜maxRadiusの球殻内にある、体積が一様なランダムな点を返す
    static Vector3 RandomPointInSphereShell(float minRadius, float maxRadius) {
        const float minR = std::max(0.0f, std::min(minRadius, maxRadius));
        const float maxR = std::max(0.0f, std::max(minRadius, maxRadius));
        if (maxR <= 0.0f) return Vector3(0.0f, 0.0f, 0.0f);
        const float z = GetRandomFloat(-1.0f, 1.0f);
        const float theta = GetRandomFloat(0.0f, 2.0f * GetPI<float>());
        const float rXY = std::sqrt(std::max(0.0f, 1.0f - z * z));
        const Vector3 dir(rXY * std::cos(theta), rXY * std::sin(theta), z);
        const float radius = std::cbrt(GetRandomFloat(minR * minR * minR, maxR * maxR * maxR));
        return dir * radius;
    }

    /// @brief 半径minRadius〜maxRadiusの円環内にある、面積が一様なランダムな点を返す
    static Vector2 RandomPointInCircleShell(float minRadius, float maxRadius) {
        const float minR = std::max(0.0f, std::min(minRadius, maxRadius));
        const float maxR = std::max(0.0f, std::max(minRadius, maxRadius));
        if (maxR <= 0.0f) return Vector2(0.0f, 0.0f);
        const float theta = GetRandomFloat(0.0f, 2.0f * GetPI<float>());
        const float radius = std::sqrt(GetRandomFloat(minR * minR, maxR * maxR));
        return Vector2(std::cos(theta) * radius, std::sin(theta) * radius);
    }

    /// @brief 内側のカプセル（minRadius/minHeight）の外側かつ外側のカプセル（maxRadius/maxHeight）の
    ///        内側にあるランダムな点を返す（カプセルはY軸方向固定。heightは円柱部分の長さ）
    static Vector3 RandomPointInCapsuleShell(float minRadius, float maxRadius, float minHeight, float maxHeight) {
        const float minR = std::max(0.0f, std::min(minRadius, maxRadius));
        const float maxR = std::max(0.0f, std::max(minRadius, maxRadius));
        const float minH = std::max(0.0f, std::min(minHeight, maxHeight));
        const float maxH = std::max(0.0f, std::max(minHeight, maxHeight));
        if (maxR <= 0.0f && maxH <= 0.0f) return Vector3(0.0f, 0.0f, 0.0f);

        const float minHalfH = minH * 0.5f;
        const float maxHalfH = maxH * 0.5f;

        auto sampleInMaxCapsule = [&]() -> Vector3 {
            const float cylinderVolume = maxH * maxR * maxR;
            const float sphereVolume = (4.0f / 3.0f) * maxR * maxR * maxR;
            const float totalWeight = cylinderVolume + sphereVolume;
            if (totalWeight <= 0.0f) return Vector3(0.0f, 0.0f, 0.0f);
            if (GetRandomFloat(0.0f, totalWeight) < cylinderVolume) {
                // 円柱部分
                const float y = GetRandomFloat(-maxHalfH, maxHalfH);
                const float theta = GetRandomFloat(0.0f, 2.0f * GetPI<float>());
                const float r = maxR * std::sqrt(GetRandomFloat(0.0f, 1.0f));
                return Vector3(r * std::cos(theta), y, r * std::sin(theta));
            }
            // 半球キャップ部分（球内の一様な点をどちらかの半球へ振り分ける）
            const float theta = GetRandomFloat(0.0f, 2.0f * GetPI<float>());
            const float phi = std::acos(GetRandomFloat(-1.0f, 1.0f));
            const float r = maxR * std::cbrt(GetRandomFloat(0.0f, 1.0f));
            const float sy = std::abs(r * std::cos(phi));
            const float sxy = r * std::sin(phi);
            const float capSign = GetRandomBool() ? 1.0f : -1.0f;
            return Vector3(sxy * std::cos(theta), capSign * (maxHalfH + sy), sxy * std::sin(theta));
        };

        constexpr int kMaxAttempts = 16;
        Vector3 p;
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            p = sampleInMaxCapsule();
            const float closestY = std::clamp(p.y, -minHalfH, minHalfH);
            const float dist = (p - Vector3(0.0f, closestY, 0.0f)).Length();
            if (dist > minR) return p;
        }
        // 内カプセルとほぼ同サイズ等で外れなかった場合、内カプセルの外側へ押し出す
        const float closestY = std::clamp(p.y, -minHalfH, minHalfH);
        const Vector3 radial = p - Vector3(0.0f, closestY, 0.0f);
        const float len = radial.Length();
        const Vector3 dir = len > 1e-6f ? radial * (1.0f / len) : Vector3(1.0f, 0.0f, 0.0f);
        return Vector3(0.0f, closestY, 0.0f) + dir * minR;
    }

    /// @brief 自身のオーナーオブジェクトを、子オブジェクトの親付けに使える可変ポインタとして解決する
    EmptyObject *ResolveMutableOwner() const {
        auto *sceneContext = GetOwnerSceneContext();
        const auto *owner = GetOwnerObject();
        if (!sceneContext || !owner) return nullptr;
        return sceneContext->GetSceneObject(owner->GetObjectID());
    }

    /// @brief オブジェクトの現在のワールド座標を取得する（Transformが無い場合は原点）
    static Vector3 GetObjectWorldPosition(EmptyObject *object) {
        if (!object) return Vector3(0.0f, 0.0f, 0.0f);
        auto *transform = object->GetComponent<Transform>();
        if (!transform) return Vector3(0.0f, 0.0f, 0.0f);
        const Matrix4x4 &world = transform->GetWorldMatrix();
        return Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
    }

    /// @brief 現在のSpawnOrigin設定に基づき、パーティクルの親オブジェクト（無ければnullptr）とワールド基準位置を求める
    void ResolveSpawnAnchor(EmptyObject *owner, EmptyObject *&outParent, Vector3 &outBasePosition) const {
        auto *sceneContext = GetOwnerSceneContext();
        switch (spawnOrigin_) {
        case SpawnOrigin::ChildOfOther: {
            EmptyObject *other = sceneContext ? sceneContext->GetSceneObject(spawnParentObjectID_) : nullptr;
            outParent = other ? other : owner;
            outBasePosition = Vector3(0.0f, 0.0f, 0.0f);
            break;
        }
        case SpawnOrigin::AtOwnerPosition:
            outParent = nullptr;
            outBasePosition = GetObjectWorldPosition(owner);
            break;
        case SpawnOrigin::AtFixedPosition:
            outParent = nullptr;
            outBasePosition = fixedSpawnPosition_;
            break;
        case SpawnOrigin::ChildOfSelf:
        default:
            outParent = owner;
            outBasePosition = Vector3(0.0f, 0.0f, 0.0f);
            break;
        }
    }

    void SpawnParticle(const std::function<void(EmptyObject *)> &setupVisual) {
        auto *sceneContext = GetOwnerSceneContext();
        auto *owner = ResolveMutableOwner();
        if (!sceneContext || !owner) return;

        auto *particleObj = sceneContext->CreateEmptyObject("Particle");
        if (!particleObj) return;

        EmptyObject *parentObject = nullptr;
        Vector3 basePosition{ 0.0f, 0.0f, 0.0f };
        ResolveSpawnAnchor(owner, parentObject, basePosition);

        const Vector3 chosenStartScale = startScale_.Get();
        const Vector3 chosenEndScale = endScale_.Get();
        const Vector3 rotationDegrees = initialRotation_.Get();

        if (auto *transform = particleObj->GetComponent<Transform>()) {
            if (parentObject) transform->SetParentObject(parentObject);
            transform->SetScale(chosenStartScale);
            transform->SetTranslate(basePosition + ComputeSpawnOffset());
            transform->SetRotate(Vector3(ToRadians(rotationDegrees.x), ToRadians(rotationDegrees.y), ToRadians(rotationDegrees.z)));
        }

        if (setupVisual) setupVisual(particleObj);

        if (auto *velocity = particleObj->AddComponent<Velocity>()) {
            velocity->SetVelocity(initialVelocity_.Get());
            velocity->SetAcceleration(acceleration_.Get());
        }

        const Vector3 rotationSpeedDegrees = initialRotationSpeed_.Get();
        const Vector3 rotationAccelDegrees = rotationAcceleration_.Get();
        if (auto *rotation = particleObj->AddComponent<Rotation>()) {
            rotation->SetAngularVelocity(Vector3(ToRadians(rotationSpeedDegrees.x), ToRadians(rotationSpeedDegrees.y), ToRadians(rotationSpeedDegrees.z)));
            rotation->SetAngularAcceleration(Vector3(ToRadians(rotationAccelDegrees.x), ToRadians(rotationAccelDegrees.y), ToRadians(rotationAccelDegrees.z)));
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
        instance.lifetime = std::max(0.01f, lifetime_.Get());
        instance.age = 0.0f;
        instance.startScale = chosenStartScale;
        instance.endScale = chosenEndScale;
        particles_.push_back(instance);
        ++totalEmittedCount_;
    }

    float spawnTimer_ = 0.0f;
    int totalEmittedCount_ = 0;
    struct ParticleInstance {
        EmptyObject *object = nullptr;
        float age = 0.0f;
        float lifetime = 1.0f;
        Vector3 startScale{ 1.0f, 1.0f, 1.0f };
        Vector3 endScale{ 0.0f, 0.0f, 0.0f };
    };
    std::vector<ParticleInstance> particles_;
};

} // namespace KashipanEngine
