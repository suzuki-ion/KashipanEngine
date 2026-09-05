#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Core/DirectXCommon.h"
#include "Debug/Logger.h"
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Rotation.h"
#include "Objects/Components/TargetLookAt.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"
#include "Objects/EmptyObject.h"
#include "Scene/SceneContext.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils.h"
#include "Utilities/Passkeys.h"
#include "Utilities/RandomizableValue.h"
#include "Utilities/RandomValue.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Scene/Editor/ComponentAddMenu.h"
#include "Scene/Editor/EditorSettings.h"
#endif

namespace KashipanEngine {

class GameEngine;
class Renderer;

/// @brief GPUシミュレーション用の永続パーティクル状態（コンピュートシェーダーが読み書きする）
/// @details HLSL側の同名構造体（ParticleUpdateCS.hlsl）とバイト単位で一致させること
struct GPUParticleData final {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    float age = 0.0f;
    Vector3 velocity{ 0.0f, 0.0f, 0.0f };
    float lifetime = 1.0f;
    Vector3 acceleration{ 0.0f, 0.0f, 0.0f };
    std::uint32_t alive = 0;
    Vector3 rotation{ 0.0f, 0.0f, 0.0f };
    float _pad0 = 0.0f;
    Vector3 angularVelocity{ 0.0f, 0.0f, 0.0f };
    float _pad1 = 0.0f;
    Vector3 angularAcceleration{ 0.0f, 0.0f, 0.0f };
    float _pad2 = 0.0f;
    Vector3 startScale{ 1.0f, 1.0f, 1.0f };
    float _pad3 = 0.0f;
    Vector3 endScale{ 0.0f, 0.0f, 0.0f };
    float _pad4 = 0.0f;
};

/// @brief GPUシミュレーション用の新規スポーンリクエスト（CPUが今フレーム発生分だけ書き込む）
/// @details HLSL側の同名構造体（ParticleSpawnCS.hlsl）とバイト単位で一致させること。
///          書き込み先スロットはCPUでは決めず、ParticleSpawnCSがgFreeList（空きスロットの
///          手動スタック）からpopして決定する
struct GPUParticleSpawnRequest final {
    float lifetime = 1.0f;
    float _pad0 = 0.0f;
    float _pad1 = 0.0f;
    float _pad2 = 0.0f;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    float _pad3 = 0.0f;
    Vector3 velocity{ 0.0f, 0.0f, 0.0f };
    float _pad4 = 0.0f;
    Vector3 acceleration{ 0.0f, 0.0f, 0.0f };
    float _pad5 = 0.0f;
    Vector3 rotation{ 0.0f, 0.0f, 0.0f };
    float _pad6 = 0.0f;
    Vector3 angularVelocity{ 0.0f, 0.0f, 0.0f };
    float _pad7 = 0.0f;
    Vector3 angularAcceleration{ 0.0f, 0.0f, 0.0f };
    float _pad8 = 0.0f;
    Vector3 startScale{ 1.0f, 1.0f, 1.0f };
    float _pad9 = 0.0f;
    Vector3 endScale{ 0.0f, 0.0f, 0.0f };
    float _pad10 = 0.0f;
};

/// @brief "ParticleUpdate"パイプラインの定数バッファ（HLSL側 ParticleUpdateConstants と一致させること）
struct GPUParticleUpdateConstants final {
    float deltaTime = 0.0f;
    std::uint32_t particleCount = 0;
    std::uint32_t billboard = 0;
    std::uint32_t billboardMode = 0;
    Matrix4x4 cameraWorldMatrix = Matrix4x4::Identity();
    /// @brief 親への追従用。SpawnOrigin::ChildOfSelf/ChildOfOtherの場合、パーティクルは
    ///        親のローカル空間で位置・速度・回転をシミュレーションし、この行列を掛けて
    ///        ワールド空間へ変換する（恒等行列の場合は従来通りワールド空間そのまま）
    Matrix4x4 parentWorldMatrix = Matrix4x4::Identity();
};

/// @brief "ParticleSpawn"パイプラインの定数バッファ（HLSL側 ParticleSpawnConstants と一致させること）
struct GPUParticleSpawnConstants final {
    std::uint32_t spawnCount = 0;
    std::uint32_t _pad0 = 0;
    std::uint32_t _pad1 = 0;
    std::uint32_t _pad2 = 0;
};

/// @brief "ParticleFreeListInit"パイプラインの定数バッファ（HLSL側 ParticleFreeListInitConstants と一致させること）
/// @details GPUバッファ（再）生成直後に一度だけ実行し、gFreeListへ空きスロット 0..capacity-1 を積み直す
struct GPUParticleFreeListInitConstants final {
    std::uint32_t capacity = 0;
    std::uint32_t _pad0 = 0;
    std::uint32_t _pad1 = 0;
    std::uint32_t _pad2 = 0;
};

/// @brief パーティクルのスポーン範囲を構成する1つの形状の種類
/// @details 2D（ParticleSystem2D）では Box は Rect、Sphere は Circle として扱われ（Z軸は無視）、Capsuleは選択できない
enum class SpawnShape {
    Point,
    Box,
    Sphere,
    Capsule,
};

/// @brief スポーン範囲を構成する1つの形状のパラメータ
/// @details 「含める形状」「除外する形状」のどちらとしても使う共通の1エントリ。
///          種類ごとに使うフィールドが異なる（Point: centerのみ、Box: center+boxSize、
///          Sphere: center+sphereRadius、Capsule: center+capsuleRadius+capsuleHeight）
struct SpawnShapeEntry final {
    SpawnShape type = SpawnShape::Point;
    Vector3 center{ 0.0f, 0.0f, 0.0f };
    /// @brief Box/Rect用。全辺のサイズ（中心から±size/2の範囲）
    Vector3 boxSize{ 1.0f, 1.0f, 1.0f };
    /// @brief Sphere/Circle用の半径
    float sphereRadius = 1.0f;
    /// @brief Capsule用の半径
    float capsuleRadius = 0.5f;
    /// @brief Capsule用の高さ（円柱部分の長さ。Y軸方向固定）
    float capsuleHeight = 1.0f;
};

inline JSON ToJSON(const SpawnShapeEntry &shape) {
    return JSON({
        { "type", static_cast<int>(shape.type) },
        { "center", ToJSON(shape.center) },
        { "boxSize", ToJSON(shape.boxSize) },
        { "sphereRadius", shape.sphereRadius },
        { "capsuleRadius", shape.capsuleRadius },
        { "capsuleHeight", shape.capsuleHeight },
    });
}

template <>
struct FromJSONImpl<SpawnShapeEntry> {
    static SpawnShapeEntry invoke(const JSON &json) {
        SpawnShapeEntry shape;
        shape.type = static_cast<SpawnShape>(json.value("type", 0));
        if (json.contains("center")) shape.center = FromJSON<Vector3>(json["center"]);
        if (json.contains("boxSize")) shape.boxSize = FromJSON<Vector3>(json["boxSize"]);
        shape.sphereRadius = json.value("sphereRadius", 1.0f);
        shape.capsuleRadius = json.value("capsuleRadius", 0.5f);
        shape.capsuleHeight = json.value("capsuleHeight", 1.0f);
        return shape;
    }
};

/// @brief ParticleSystem2D / ParticleSystem3D 共通のパーティクル生成・寿命管理ロジックを持つ基底クラス
/// @details このクラス自体はコンポーネントとして登録されない（登録・COMPONENT_CATEGORYの実体は
///          このクラスで宣言し、Add Componentメニュー等では派生クラス名で表示される）。
///          毎フレーム一定間隔で自身の子オブジェクトとしてパーティクルを生成し、
///          Velocityコンポーネントで移動・重力を、寿命に応じたスケール変化を適用し、
///          寿命が来たら削除する。描画コンポーネントの追加方法（2D/3Dどちらを使うか）だけが
///          派生クラスごとに異なるため、生成直後のパーティクルへ描画コンポーネントを
///          追加するコールバック（setupVisual）だけを派生クラスから受け取る。
///
///          パーティクルオブジェクトは事前に Max Particles 分だけプールとして生成しておき、
///          発生・消滅は SetActive の有効/無効切り替えで管理する（生成・削除の繰り返しを避けるため）。
///          GPU Simulation を有効にした場合は、このオブジェクトプールを一切使わず、
///          コンピュートシェーダー（ParticleSpawn/ParticleUpdateパイプライン）が
///          位置・回転・スケール・寿命を計算し、専用の描画パス（Renderer::RenderGpuParticles）が
///          その結果を直接インスタンス描画する。
class ParticleSystemBase : public IObjectComponent {
public:
    COMPONENT_CATEGORY("Effect")

    /// @brief GameEngine から DirectXCommon を設定
    static void SetDirectXCommon(Passkey<GameEngine>, DirectXCommon *dx) { sDirectXCommon_ = dx; }

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
        for (size_t i = 0; i < slots_.size(); ++i) {
            if (!slots_[i].active) continue;
            slots_[i].active = false;
            if (i < pool_.size() && pool_[i]) pool_[i]->SetActive(false);
            freeIndices_.push_back(static_cast<int>(i));
        }
        spawnTimer_ = 0.0f;
        totalEmittedCount_ = 0;
    }

    void SetEmissionRate(float rate) noexcept { emissionRate_ = rate; }
    float GetEmissionRate() const noexcept { return emissionRate_; }
    void SetMaxParticles(int count) noexcept { maxParticles_ = std::max(0, count); }
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

    /// @brief シャドウマッピングのシャドウキャスターとして扱うかを設定する
    /// @details CPUモード（3D）では生成する各パーティクルのMeshRendererへそのまま伝播する
    ///          （プール生成時に一度だけ適用されるため、生存中のパーティクルには反映されない。
    ///          Pipeline/Material等、他のMeshRenderer設定と同じ挙動）。
    ///          GPUモードではRendererのシャドウ描画パスがこのフラグを見て対象に含めるかを判断する。
    ///          2D（SpriteRenderer）はそもそもシャドウを落とせないため実質的に影響しない
    void SetCastShadows(bool enabled) noexcept { castShadows_ = enabled; }
    bool GetCastShadows() const noexcept { return castShadows_; }

    /// @brief GPUシミュレーション（コンピュートシェーダーによる移動・寿命計算）の有効/無効を設定する
    /// @details 切り替え時に現在のシミュレーション方式のリソースを破棄し、新しい方式を初期化する
    void SetGPUSimulation(bool enabled) {
        if (gpuSimulation_ == enabled) return;
        gpuSimulation_ = enabled;
        SwitchSimulationMode();
    }
    bool IsGPUSimulation() const noexcept { return gpuSimulation_; }

    //==================================================
    // 描画情報取得（メッシュ/パイプライン/マテリアルはCPU/GPU両モードで共通）
    //==================================================

    const std::string &GetPipelineName() const noexcept { return pipelineName_; }
    MaterialManager::MaterialHandle GetMaterialHandle() const {
        return MaterialManager::GetMaterialHandleFromName(materialName_);
    }
    ModelManager::ModelHandle GetMeshHandle() const {
        return ModelManager::GetModelHandleFromAssetPath(meshAssetPath_);
    }
    EmptyObject *GetTargetObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }
    bool IsRenderTargetIncluded(const IRenderTarget *target) const {
        if (!target) return false;
        return !excludedRenderTargetNames_.contains(target->GetRenderTargetName());
    }

    //==================================================
    // Renderer専用: GPUパーティクル用リソース取得（Passkey<Renderer>限定）
    //==================================================

    RWStructuredBufferResource *GetGpuParticleBuffer(Passkey<Renderer>) const noexcept { return gpuParticleBuffer_.get(); }
    RWStructuredBufferResource *GetGpuInstanceMatrixBuffer(Passkey<Renderer>) const noexcept { return gpuInstanceMatrixBuffer_.get(); }
    StructuredBufferResource *GetGpuSpawnRequestBuffer(Passkey<Renderer>) const noexcept { return gpuSpawnRequestBuffer_.get(); }
    ConstantBufferResource *GetGpuSpawnConstantBuffer(Passkey<Renderer>) const noexcept { return gpuSpawnConstantBuffer_.get(); }
    ConstantBufferResource *GetGpuUpdateConstantBuffer(Passkey<Renderer>) const noexcept { return gpuUpdateConstantBuffer_.get(); }
    /// @brief 空きスロット番号を積んだ手動スタック（フリーリスト本体、要素数=capacity）
    RWStructuredBufferResource *GetGpuFreeListBuffer(Passkey<Renderer>) const noexcept { return gpuFreeListBuffer_.get(); }
    /// @brief フリーリストスタックの残数（1要素のint32バッファ。InterlockedAddで増減する）
    RWStructuredBufferResource *GetGpuFreeListCounterBuffer(Passkey<Renderer>) const noexcept { return gpuFreeListCounterBuffer_.get(); }
    ConstantBufferResource *GetGpuFreeListInitConstantBuffer(Passkey<Renderer>) const noexcept { return gpuFreeListInitConstantBuffer_.get(); }
    std::uint32_t GetGpuSpawnCount(Passkey<Renderer>) const noexcept { return gpuFrameSpawnCount_; }
    /// @brief フリーリストの初期化（0..capacity-1を積み直す）がまだ必要かを取得し、フラグをリセットする
    /// @details GPUバッファの新規生成・容量変更直後にtrueになる。Rendererはこれがtrueの間、
    ///          Spawn/Updateディスパッチより先に"ParticleFreeListInit"パイプラインを1回実行する
    bool ConsumeGpuFreeListNeedsInit(Passkey<Renderer>) noexcept {
        const bool needsInit = gpuFreeListNeedsInit_;
        gpuFreeListNeedsInit_ = false;
        return needsInit;
    }
    /// @brief このフレームにUpdateParticlesGPUが実行されたか（＝シミュレーションを進めてよいか）を取得し、フラグをリセットする
    /// @details シーンのポーズ中・停止中はコンポーネントのUpdateが呼ばれずこのフラグが立たない。
    ///          RendererはRenderFrameのたびにProcessGpuParticlesを呼ぶが、この値がfalseの場合は
    ///          スポーン/更新のDispatchを丸ごとスキップする（CPUパーティクルはUpdateスキップで
    ///          自然に止まるのに対し、GPUパーティクルだけが実時間で動き続けるのを防ぐため）
    bool ConsumeGpuFrameUpdated(Passkey<Renderer>) noexcept {
        const bool updated = gpuUpdatedThisFrame_;
        gpuUpdatedThisFrame_ = false;
        return updated;
    }
    /// @brief 実際に確保済みのGPUバッファの要素数を返す
    /// @details maxParticles_をそのまま返すと、ShowImGui（Update後）でMax Particlesが変更された
    ///          フレームに限り、まだEnsureGpuCapacity()が反映されていない（バッファは旧サイズのままの）
    ///          状態でRendererがこの値をDispatch/Draw件数に使ってしまい、実際のバッファ容量を超えた
    ///          範囲へUAV書き込みが発生して（Device Removedやメモリ破壊の原因になる）しまう。
    ///          実際に確保済みのバッファの要素数を返すことで、常に両者を一致させる
    std::uint32_t GetGpuParticleCapacity(Passkey<Renderer>) const noexcept {
        return gpuParticleBuffer_ ? static_cast<std::uint32_t>(gpuParticleBuffer_->GetElementCount()) : 0;
    }
    /// @brief ビルボードの向き先（明示指定がなければシーン内のカメラを自動解決）をRendererから取得する
    EmptyObject *ResolveBillboardCameraObject(Passkey<Renderer>) const {
        EmptyObject *target = GetBillboardTarget();
        return target ? target : ResolveBillboardCameraObject();
    }
    /// @brief GPUモードで親への追従に使う「現在のスポーン元の親オブジェクト」を解決する
    /// @details ResolveSpawnAnchorと同じ判定を使う（SpawnOrigin::ChildOfSelf/ChildOfOtherの場合のみ
    ///          非nullptrを返す）。AtOwnerPosition/AtFixedPositionの場合はnullptr
    ///          （親追従なし＝恒等行列扱い）を返す
    EmptyObject *ResolveGpuFollowParent(Passkey<Renderer>) const {
        auto *owner = ResolveMutableOwner();
        if (!owner) return nullptr;
        EmptyObject *parent = nullptr;
        Vector3 unusedBasePosition{ 0.0f, 0.0f, 0.0f };
        ResolveSpawnAnchor(owner, parent, unusedBasePosition);
        return parent;
    }

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
        ADD_MEMBER_VARIABLE(castShadows_);
        // includeShapes_/excludeShapes_（可変長のリスト）は、このKey-Value型のメンバー変数リフレクション
        // （プレハブ同期・キーフレームアニメーション用。安定した固定アドレスの単一値を前提とする）には
        // 登録できないため対象外（他の可変長コンテナ系メンバーと同様の扱い）
        ADD_MEMBER_VARIABLE(playOnStart_);
        ADD_MEMBER_VARIABLE(loop_);
        ADD_MEMBER_VARIABLE(fixedSpawnPosition_);
        ADD_MEMBER_VARIABLE(meshAssetPath_);
        ADD_MEMBER_VARIABLE(pipelineName_);
        ADD_MEMBER_VARIABLE(materialName_);
        // RandomizableValue<T>（固定値/ランダム範囲切り替え付きの値）はサブフィールド単位で登録する
        ADD_MEMBER_VARIABLE(spawnCount_.randomize);
        ADD_MEMBER_VARIABLE(spawnCount_.value);
        ADD_MEMBER_VARIABLE(spawnCount_.min);
        ADD_MEMBER_VARIABLE(spawnCount_.max);
        ADD_MEMBER_VARIABLE(lifetime_.randomize);
        ADD_MEMBER_VARIABLE(lifetime_.value);
        ADD_MEMBER_VARIABLE(lifetime_.min);
        ADD_MEMBER_VARIABLE(lifetime_.max);
        ADD_MEMBER_VARIABLE(initialVelocity_.randomize);
        ADD_MEMBER_VARIABLE(initialVelocity_.value);
        ADD_MEMBER_VARIABLE(initialVelocity_.min);
        ADD_MEMBER_VARIABLE(initialVelocity_.max);
        ADD_MEMBER_VARIABLE(acceleration_.randomize);
        ADD_MEMBER_VARIABLE(acceleration_.value);
        ADD_MEMBER_VARIABLE(acceleration_.min);
        ADD_MEMBER_VARIABLE(acceleration_.max);
        ADD_MEMBER_VARIABLE(startScale_.randomize);
        ADD_MEMBER_VARIABLE(startScale_.value);
        ADD_MEMBER_VARIABLE(startScale_.min);
        ADD_MEMBER_VARIABLE(startScale_.max);
        ADD_MEMBER_VARIABLE(endScale_.randomize);
        ADD_MEMBER_VARIABLE(endScale_.value);
        ADD_MEMBER_VARIABLE(endScale_.min);
        ADD_MEMBER_VARIABLE(endScale_.max);
        ADD_MEMBER_VARIABLE(initialRotation_.randomize);
        ADD_MEMBER_VARIABLE(initialRotation_.value);
        ADD_MEMBER_VARIABLE(initialRotation_.min);
        ADD_MEMBER_VARIABLE(initialRotation_.max);
        ADD_MEMBER_VARIABLE(initialRotationSpeed_.randomize);
        ADD_MEMBER_VARIABLE(initialRotationSpeed_.value);
        ADD_MEMBER_VARIABLE(initialRotationSpeed_.min);
        ADD_MEMBER_VARIABLE(initialRotationSpeed_.max);
        ADD_MEMBER_VARIABLE(rotationAcceleration_.randomize);
        ADD_MEMBER_VARIABLE(rotationAcceleration_.value);
        ADD_MEMBER_VARIABLE(rotationAcceleration_.min);
        ADD_MEMBER_VARIABLE(rotationAcceleration_.max);
    }

    /// @brief 派生クラスのInitializeから呼ぶ
    void InitializeBase() {
        if (playOnStart_) isPlaying_ = true;
        if (gpuSimulation_) {
            InitializeGpuResources();
            auto *sceneRenderer = GetOrAddSceneRenderer();
            if (sceneRenderer) {
                sceneRenderer->RegisterGpuParticleEmitter(this);
                char buf[128];
                std::snprintf(buf, sizeof(buf), "(this=%p, sceneRenderer=%p)", static_cast<const void *>(this), static_cast<const void *>(sceneRenderer));
                Log(Translation("engine.particlesystem.gpu.emitter.registered") + buf, LogSeverity::Info);
            } else {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "(this=%p)", static_cast<const void *>(this));
                Log(Translation("engine.particlesystem.gpu.scenerenderer.notfound") + buf, LogSeverity::Warning);
            }
        }
    }
    /// @brief 派生クラスのFinalizeから呼ぶ（生存中のパーティクル・プール・GPUリソースを全て破棄する）
    void FinalizeBase() {
        Clear();
        DestroyCpuPool();
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) sceneRenderer->UnregisterGpuParticleEmitter(this);
        DestroyGpuResources();
#if defined(USE_IMGUI)
        DestroyExtraComponentsTemplateObject();
#endif
    }

    /// @brief 派生クラスのUpdateから呼ぶ。新規生成した子オブジェクトへ描画コンポーネントを
    ///        追加してもらうため、プール生成時に一度だけ setupVisual(生成したEmptyObject*) を呼び出す
    ///        （GPU Simulation有効時はこちらは呼ばず、代わりにUpdateParticlesGPUを呼ぶこと）
    void UpdateParticles(const std::function<void(EmptyObject *)> &setupVisual) {
        EnsurePoolSize(maxParticles_, setupVisual);

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
                    if (!freeIndices_.empty()) {
                        SpawnParticle();
                    }
                }
                if (!isPlaying_) break;
            }
        }

        // --- 寿命管理・スケール変化 ---
        for (size_t i = 0; i < slots_.size(); ++i) {
            if (!slots_[i].active) continue;
            auto *particleObject = pool_[i];
            if (!particleObject || !sceneContext || !sceneContext->GetSceneObject(particleObject)) {
                // 何らかの理由で既に削除されている（エディタ操作等）
                slots_[i].active = false;
                freeIndices_.push_back(static_cast<int>(i));
                continue;
            }
            slots_[i].age += dt;
            if (slots_[i].age >= slots_[i].lifetime) {
                particleObject->SetActive(false);
                slots_[i].active = false;
                freeIndices_.push_back(static_cast<int>(i));
                continue;
            }
            if (auto *transform = particleObject->GetComponent<Transform>()) {
                const float t = slots_[i].lifetime > 0.0f ? slots_[i].age / slots_[i].lifetime : 1.0f;
                transform->SetScale(Vector3::Lerp(slots_[i].startScale, slots_[i].endScale, t));
            }
        }
    }

    /// @brief GPU Simulation有効時に派生クラスのUpdateから呼ぶ。発生タイミングの計算・スポーン
    ///        パラメータの抽選はCPU側で行い、実際の移動・寿命計算はコンピュートシェーダーへ委ねる
    void UpdateParticlesGPU() {
        // このフレームはゲームループが動いている（ポーズ中でない）ことをRendererへ伝える
        gpuUpdatedThisFrame_ = true;

        if (!gpuParticleBuffer_ || !gpuInstanceMatrixBuffer_) InitializeGpuResources();
        const std::uint32_t capacity = static_cast<std::uint32_t>(std::max(0, maxParticles_));
        if (capacity == 0) { gpuFrameSpawnCount_ = 0; return; }

        const float dt = GetDeltaTime();
        auto *owner = ResolveMutableOwner();

        std::vector<GPUParticleSpawnRequest> requests;

        if (isPlaying_ && emissionRate_ > 0.0f && owner) {
            const float interval = 1.0f / emissionRate_;
            spawnTimer_ += dt;
            while (spawnTimer_ >= interval) {
                spawnTimer_ -= interval;
                const int count = std::max(1, spawnCount_.Get());
                for (int i = 0; i < count; ++i) {
                    if (!loop_ && totalEmittedCount_ >= totalSpawnCount_) {
                        isPlaying_ = false;
                        break;
                    }
                    if (requests.size() >= capacity) break;

                    // ResolveSpawnAnchorが返す親（ChildOfSelf/ChildOfOtherの場合）は、GPUモードでは
                    // ここでは使わない。position/velocity/rotationは親のローカル空間のまま保持し、
                    // 毎フレームのUpdateパイプラインが親の現在のワールド行列（ResolveGpuFollowParent
                    // 経由でRendererが取得）を掛けてワールド空間へ変換するため、ここでワールド座標へ
                    // 焼き込んではいけない（焼き込むとその場に固定され、親の動き・回転に追従しなくなる）
                    EmptyObject *unusedParentObject = nullptr;
                    Vector3 basePosition{ 0.0f, 0.0f, 0.0f };
                    ResolveSpawnAnchor(owner, unusedParentObject, basePosition);

                    // 書き込み先スロットはここでは決めない（ParticleSpawnCSがgFreeListから
                    // 空きスロットをpopして決定する。空きが無ければそのリクエストは破棄される）
                    GPUParticleSpawnRequest request;
                    request.position = basePosition + ComputeSpawnOffset();
                    request.velocity = initialVelocity_.Get();
                    request.acceleration = acceleration_.Get();
                    request.lifetime = std::max(0.01f, lifetime_.Get());
                    const Vector3 rotationDegrees = initialRotation_.Get();
                    request.rotation = Vector3(ToRadians(rotationDegrees.x), ToRadians(rotationDegrees.y), ToRadians(rotationDegrees.z));
                    const Vector3 rotationSpeedDegrees = initialRotationSpeed_.Get();
                    request.angularVelocity = Vector3(ToRadians(rotationSpeedDegrees.x), ToRadians(rotationSpeedDegrees.y), ToRadians(rotationSpeedDegrees.z));
                    const Vector3 rotationAccelDegrees = rotationAcceleration_.Get();
                    request.angularAcceleration = Vector3(ToRadians(rotationAccelDegrees.x), ToRadians(rotationAccelDegrees.y), ToRadians(rotationAccelDegrees.z));
                    request.startScale = startScale_.Get();
                    request.endScale = endScale_.Get();
                    requests.push_back(request);
                    ++totalEmittedCount_;
                }
                if (!isPlaying_) break;
            }
        }

        UploadGpuSpawnRequests(requests);
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
        castShadows_ = other.castShadows_;
        includeShapes_ = other.includeShapes_;
        excludeShapes_ = other.excludeShapes_;
        MarkSpawnVoxelGridDirty();
        gpuSimulation_ = other.gpuSimulation_;
        extraComponentTemplates_ = other.extraComponentTemplates_;
#if defined(USE_IMGUI)
        extraComponentsTemplateDirty_ = true;
#endif
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
        json["castShadows"] = castShadows_;
        json["includeShapes"] = ToJSON(includeShapes_);
        json["excludeShapes"] = ToJSON(excludeShapes_);
        json["gpuSimulation"] = gpuSimulation_;
        json["extraComponents"] = extraComponentTemplates_;
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
        castShadows_ = json.value("castShadows", true);

        if (json.contains("includeShapes") || json.contains("excludeShapes")) {
            // 新形式（複数形状のリスト）
            includeShapes_.clear();
            if (json.contains("includeShapes")) includeShapes_ = FromJSON<std::vector<SpawnShapeEntry>>(json["includeShapes"]);
            excludeShapes_.clear();
            if (json.contains("excludeShapes")) excludeShapes_ = FromJSON<std::vector<SpawnShapeEntry>>(json["excludeShapes"]);
        } else {
            // 旧形式（単一形状＋Min/Maxのシェル）からの移行:
            // 単一のinclude形状（サイズ=Max）を作り、Minが0でなければ同じ中心・同じ種類でサイズ=Minの
            // exclude形状を1つ追加する（旧シェル表現を「含める形状＋除外形状」で近似的に再現する）
            includeShapes_.clear();
            excludeShapes_.clear();
            const auto legacyShape = static_cast<SpawnShape>(json.value("spawnShape", 0));
            const Vector3 legacyCenter = json.contains("spawnCenter") ? FromJSON<Vector3>(json["spawnCenter"]) : Vector3(0.0f, 0.0f, 0.0f);
            switch (legacyShape) {
            case SpawnShape::Point: {
                SpawnShapeEntry point;
                point.type = SpawnShape::Point;
                point.center = legacyCenter;
                includeShapes_.push_back(point);
                break;
            }
            case SpawnShape::Box: {
                const Vector3 sizeMax = json.contains("spawnBoxSizeMax") ? FromJSON<Vector3>(json["spawnBoxSizeMax"]) : Vector3(1.0f, 1.0f, 1.0f);
                const Vector3 sizeMin = json.contains("spawnBoxSizeMin") ? FromJSON<Vector3>(json["spawnBoxSizeMin"]) : Vector3(0.0f, 0.0f, 0.0f);
                SpawnShapeEntry outer; outer.type = SpawnShape::Box; outer.center = legacyCenter; outer.boxSize = sizeMax;
                includeShapes_.push_back(outer);
                if (sizeMin.x > 0.0f || sizeMin.y > 0.0f || sizeMin.z > 0.0f) {
                    SpawnShapeEntry inner; inner.type = SpawnShape::Box; inner.center = legacyCenter; inner.boxSize = sizeMin;
                    excludeShapes_.push_back(inner);
                }
                break;
            }
            case SpawnShape::Sphere: {
                const float rMax = json.value("spawnSphereRadiusMax", 1.0f);
                const float rMin = json.value("spawnSphereRadiusMin", 0.0f);
                SpawnShapeEntry outer; outer.type = SpawnShape::Sphere; outer.center = legacyCenter; outer.sphereRadius = rMax;
                includeShapes_.push_back(outer);
                if (rMin > 0.0f) {
                    SpawnShapeEntry inner; inner.type = SpawnShape::Sphere; inner.center = legacyCenter; inner.sphereRadius = rMin;
                    excludeShapes_.push_back(inner);
                }
                break;
            }
            case SpawnShape::Capsule: {
                const float rMax = json.value("spawnCapsuleRadiusMax", 0.5f);
                const float rMin = json.value("spawnCapsuleRadiusMin", 0.0f);
                const float hMax = json.value("spawnCapsuleHeightMax", 1.0f);
                const float hMin = json.value("spawnCapsuleHeightMin", 0.0f);
                SpawnShapeEntry outer; outer.type = SpawnShape::Capsule; outer.center = legacyCenter; outer.capsuleRadius = rMax; outer.capsuleHeight = hMax;
                includeShapes_.push_back(outer);
                if (rMin > 0.0f || hMin > 0.0f) {
                    SpawnShapeEntry inner; inner.type = SpawnShape::Capsule; inner.center = legacyCenter; inner.capsuleRadius = rMin; inner.capsuleHeight = hMin;
                    excludeShapes_.push_back(inner);
                }
                break;
            }
            }
        }
        MarkSpawnVoxelGridDirty();

        SetGPUSimulation(json.value("gpuSimulation", false));

        extraComponentTemplates_.clear();
        if (json.contains("extraComponents")) {
            for (const auto &entry : json["extraComponents"]) {
                extraComponentTemplates_.push_back(entry);
            }
        }
#if defined(USE_IMGUI)
        extraComponentsTemplateDirty_ = true;
#endif
    }

#if defined(USE_IMGUI)
    /// @brief TranslationLabelが返す"表示テキスト###翻訳キー"形式から、表示用テキスト部分の終端を求める。
    /// @details Checkbox/Button等のImGuiウィジェットは"##"以降をIDとして扱い表示しないが、
    ///          TextUnformatted等の素のテキスト表示関数はこの規約を解釈せず文字列をそのまま表示して
    ///          しまうため、そちらで使う場合はこの終端位置までを明示的に渡す必要がある
    static const char *FindImGuiLabelDisplayEnd(const char *label) {
        const char *end = label;
        while (end[0] != '\0' && !(end[0] == '#' && end[1] == '#')) ++end;
        return end;
    }

    /// @brief 「ランダムにするか否か」のトグルと、固定値 or Min/Max範囲の入力を表示する（float版）
    void ShowRandomizableImGui(const char *label, RandomizableValue<float> &v, float speed, float vMin = 0.0f, float vMax = 0.0f) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label, FindImGuiLabelDisplayEnd(label));
        ImGui::SameLine();
        ImGui::Checkbox(TranslationLabel("component.particlesystembase.random"), &v.randomize);
        if (v.randomize) {
            ImGui::DragFloat(TranslationLabel("component.particlesystembase.min"), &v.min, speed, vMin, vMax);
            ImGui::DragFloat(TranslationLabel("component.particlesystembase.max"), &v.max, speed, vMin, vMax);
        } else {
            ImGui::DragFloat(TranslationLabel("component.particlesystembase.value"), &v.value, speed, vMin, vMax);
        }
        ImGui::PopID();
    }

    /// @brief 「ランダムにするか否か」のトグルと、固定値 or Min/Max範囲の入力を表示する（int版）
    void ShowRandomizableImGui(const char *label, RandomizableValue<int> &v, int vMin = 0, int vMax = 0) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label, FindImGuiLabelDisplayEnd(label));
        ImGui::SameLine();
        ImGui::Checkbox(TranslationLabel("component.particlesystembase.random"), &v.randomize);
        if (v.randomize) {
            ImGui::DragInt(TranslationLabel("component.particlesystembase.min"), &v.min, 1.0f, vMin, vMax);
            ImGui::DragInt(TranslationLabel("component.particlesystembase.max"), &v.max, 1.0f, vMin, vMax);
        } else {
            ImGui::DragInt(TranslationLabel("component.particlesystembase.value"), &v.value, 1.0f, vMin, vMax);
        }
        ImGui::PopID();
    }

    /// @brief 「ランダムにするか否か」のトグルと、固定値 or Min/Max範囲の入力を表示する（Vector3版）
    void ShowRandomizableImGui(const char *label, RandomizableValue<Vector3> &v, float speed) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label, FindImGuiLabelDisplayEnd(label));
        ImGui::SameLine();
        ImGui::Checkbox(TranslationLabel("component.particlesystembase.random"), &v.randomize);
        if (v.randomize) {
            ImGui::DragFloat3(TranslationLabel("component.particlesystembase.min"), &v.min.x, speed);
            ImGui::DragFloat3(TranslationLabel("component.particlesystembase.max"), &v.max.x, speed);
        } else {
            ImGui::DragFloat3(TranslationLabel("component.particlesystembase.value"), &v.value.x, speed);
        }
        ImGui::PopID();
    }

    /// @brief 形状1個ぶんの種類・パラメータ入力を表示する（Include/Exclude共通）
    /// @return 値が変更された場合はtrue
    bool ShowSpawnShapeEntryImGui(SpawnShapeEntry &shape) {
        bool changed = false;
        if (is2D_) {
            const char *kShapeLabels[] = { TranslationC("component.particlesystembase.shape.point"), TranslationC("component.particlesystembase.shape.rect"), TranslationC("component.particlesystembase.shape.circle") };
            int shapeIndex = static_cast<int>(shape.type);
            if (ImGui::Combo(TranslationLabel("component.particlesystembase.type"), &shapeIndex, kShapeLabels, 3)) { shape.type = static_cast<SpawnShape>(shapeIndex); changed = true; }
        } else {
            const char *kShapeLabels[] = { TranslationC("component.particlesystembase.shape.point"), TranslationC("component.particlesystembase.shape.box"), TranslationC("component.particlesystembase.shape.sphere"), TranslationC("component.particlesystembase.shape.capsule") };
            int shapeIndex = static_cast<int>(shape.type);
            if (ImGui::Combo(TranslationLabel("component.particlesystembase.type"), &shapeIndex, kShapeLabels, 4)) { shape.type = static_cast<SpawnShape>(shapeIndex); changed = true; }
        }
        // Point（座標そのもの）を含め、常にCenterを編集できるようにする
        if (ImGui::DragFloat3(TranslationLabel("component.particlesystembase.center"), &shape.center.x, 0.01f)) changed = true;
        switch (shape.type) {
        case SpawnShape::Point:
            break;
        case SpawnShape::Box:
            if (is2D_) { if (ImGui::DragFloat2(TranslationLabel("component.particlesystembase.size"), &shape.boxSize.x, 0.01f, 0.0f)) changed = true; }
            else { if (ImGui::DragFloat3(TranslationLabel("component.particlesystembase.size"), &shape.boxSize.x, 0.01f, 0.0f)) changed = true; }
            break;
        case SpawnShape::Sphere:
            if (ImGui::DragFloat(TranslationLabel("component.particlesystembase.radius"), &shape.sphereRadius, 0.01f, 0.0f)) changed = true;
            break;
        case SpawnShape::Capsule:
            if (ImGui::DragFloat(TranslationLabel("component.particlesystembase.radius"), &shape.capsuleRadius, 0.01f, 0.0f)) changed = true;
            if (ImGui::DragFloat(TranslationLabel("component.particlesystembase.height"), &shape.capsuleHeight, 0.01f, 0.0f)) changed = true;
            break;
        }
        return changed;
    }

    /// @brief 形状のリスト（Include/Excludeどちらか）の追加・削除・編集UIを表示する
    void ShowSpawnShapeListImGui(const char *label, const char *addButtonLabel, std::vector<SpawnShapeEntry> &shapes) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label, FindImGuiLabelDisplayEnd(label));
        int removeIndex = -1;
        for (size_t i = 0; i < shapes.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Indent();
            if (ShowSpawnShapeEntryImGui(shapes[i])) MarkSpawnVoxelGridDirty();
            if (ImGui::Button(TranslationLabel("component.particlesystembase.remove"))) removeIndex = static_cast<int>(i);
            ImGui::Unindent();
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIndex >= 0) {
            shapes.erase(shapes.begin() + removeIndex);
            MarkSpawnVoxelGridDirty();
        }
        if (ImGui::Button(addButtonLabel)) {
            shapes.push_back(SpawnShapeEntry{});
            MarkSpawnVoxelGridDirty();
        }
        ImGui::PopID();
    }

    /// @brief スポーン範囲（含める形状の和集合から、除外する形状の和集合を除いた領域）のImGui表示
    void ShowSpawnShapeImGui() {
        ImGui::SeparatorText(TranslationLabel("component.particlesystembase.spawn_area"));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(TranslationC("component.particlesystembase.desc_1"));
        }
        ShowSpawnShapeListImGui(TranslationLabel("component.particlesystembase.include_shapes"), TranslationLabel("component.particlesystembase.add_include_shape"), includeShapes_);
        ShowSpawnShapeListImGui(TranslationLabel("component.particlesystembase.exclude_shapes"), TranslationLabel("component.particlesystembase.add_exclude_shape"), excludeShapes_);
    }

    /// @brief パーティクルオブジェクトの生成方法（親付け・生成位置）のImGui表示
    void ShowSpawnOriginImGui() {
        ImGui::SeparatorText(TranslationLabel("component.particlesystembase.spawn_origin"));
        static const char *kOriginLabels[] = {
            TranslationC("component.particlesystembase.origin.childofself"), TranslationC("component.particlesystembase.origin.childofother"), TranslationC("component.particlesystembase.origin.atownerposition"), TranslationC("component.particlesystembase.origin.atfixedposition"),
        };
        int originIndex = static_cast<int>(spawnOrigin_);
        if (ImGui::Combo(TranslationLabel("component.particlesystembase.origin"), &originIndex, kOriginLabels, 4)) {
            spawnOrigin_ = static_cast<SpawnOrigin>(originIndex);
        }
        if (gpuSimulation_ && (spawnOrigin_ == SpawnOrigin::ChildOfSelf || spawnOrigin_ == SpawnOrigin::ChildOfOther)) {
            ImGui::TextDisabled(TranslationC("component.particlesystembase.gpu_n"));
        }
        switch (spawnOrigin_) {
        case SpawnOrigin::ChildOfOther:
            TargetObjectSelector::ShowSelector(TranslationLabel("component.particlesystembase.parent_object"), GetOwnerSceneContext(), spawnParentObjectID_, true, false);
            if (!spawnParentObjectID_.IsValid()) {
                ImGui::TextDisabled("%s", TranslationC("component.particlesystembase.desc_2"));
            }
            break;
        case SpawnOrigin::AtFixedPosition:
            ImGui::DragFloat3(TranslationLabel("component.particlesystembase.fixed_position"), &fixedSpawnPosition_.x, 0.01f);
            break;
        case SpawnOrigin::ChildOfSelf:
        case SpawnOrigin::AtOwnerPosition:
        default:
            break;
        }
    }

    /// @brief 項目数が多いため、種類ごとに開閉可能なセクションへ分けて表示する。
    ///        各セクションの開閉状態はEditorSettingsにキーごと永続化される（2D/3Dで共通のキーを使うため
    ///        開閉状態も共有される）
    void ShowBaseFieldsImGui() {
        ShowPlaybackStatusSectionImGui();
        if (EditorSettings::PersistentCollapsingHeader(TranslationC("component.particlesystembase.section.emission"), "particlesystem.section.emission")) {
            ShowEmissionSectionImGui();
        }
        if (EditorSettings::PersistentCollapsingHeader(TranslationC("component.particlesystembase.section.motion"), "particlesystem.section.motion")) {
            ShowMotionSectionImGui();
        }
        if (EditorSettings::PersistentCollapsingHeader(TranslationC("component.particlesystembase.section.scale"), "particlesystem.section.scale")) {
            ShowRandomizableImGui(TranslationLabel("component.particlesystembase.start_scale"), startScale_, 0.01f);
            ShowRandomizableImGui(TranslationLabel("component.particlesystembase.end_scale"), endScale_, 0.01f);
        }
        if (EditorSettings::PersistentCollapsingHeader(TranslationC("component.particlesystembase.section.rotation"), "particlesystem.section.rotation")) {
            ShowRandomizableImGui(TranslationLabel("component.particlesystembase.initial_rotation_deg"), initialRotation_, 0.5f);
            ShowRandomizableImGui(TranslationLabel("component.particlesystembase.initial_rotation_speed"), initialRotationSpeed_, 0.5f);
            ShowRandomizableImGui(TranslationLabel("component.particlesystembase.rotation_acceleration"), rotationAcceleration_, 0.5f);
        }
        if (EditorSettings::PersistentCollapsingHeader(TranslationC("component.particlesystembase.rendering"), "particlesystem.section.rendering")) {
            ShowRenderingSectionImGui();
        }
        if (EditorSettings::PersistentCollapsingHeader(TranslationC("component.particlesystembase.section.extra_components"), "particlesystem.section.extracomponents")) {
            ShowExtraComponentsImGui();
        }
    }

    /// @brief 再生・状態セクション（他と異なり常時表示。折り畳むと再生状況が見えなくなるため）
    void ShowPlaybackStatusSectionImGui() {
        bool isPlayingLocal = isPlaying_;
        if (ImGui::Checkbox(TranslationLabel("component.particlesystembase.playing"), &isPlayingLocal)) {
            isPlayingLocal ? Play() : Stop();
        }
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("component.particlesystembase.clear"))) Clear();
        if (gpuSimulation_) {
            ImGui::Text(TranslationC("component.particlesystembase.gpu_simulation_capacity_d"), maxParticles_);
        } else {
            ImGui::Text(TranslationC("component.particlesystembase.live_particles_d"), maxParticles_ - static_cast<int>(freeIndices_.size()));
        }
        ImGui::Text(TranslationC("component.particlesystembase.total_emitted_d"), totalEmittedCount_);

        bool gpuSimulationLocal = gpuSimulation_;
        if (ImGui::Checkbox(TranslationLabel("component.particlesystembase.gpu_simulation"), &gpuSimulationLocal)) {
            SetGPUSimulation(gpuSimulationLocal);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.particlesystembase.desc_3"));
        }

        ImGui::Checkbox(TranslationLabel("component.particlesystembase.play_on_start"), &playOnStart_);
        ImGui::Checkbox(TranslationLabel("component.particlesystembase.loop"), &loop_);
    }

    /// @brief 発生セクション: 発生レート・最大数・生成範囲・生成方法
    void ShowEmissionSectionImGui() {
        ImGui::DragFloat(TranslationLabel("component.particlesystembase.emission_rate"), &emissionRate_, 0.1f, 0.0f);
        int maxParticlesLocal = maxParticles_;
        if (ImGui::DragInt(TranslationLabel("component.particlesystembase.max_particles"), &maxParticlesLocal, 1.0f, 0)) {
            SetMaxParticles(maxParticlesLocal);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.particlesystembase.desc_4"));
        }
        ImGui::BeginDisabled(loop_);
        ImGui::DragInt(TranslationLabel("component.particlesystembase.total_spawn_count"), &totalSpawnCount_, 1.0f, 0, 1000000);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.particlesystembase.loop_2"));
        }
        ShowRandomizableImGui(TranslationLabel("component.particlesystembase.spawn_count"), spawnCount_, 1, 100);

        ShowSpawnShapeImGui();
        ShowSpawnOriginImGui();
    }

    /// @brief 寿命・運動セクション: 寿命・初速・加速度
    void ShowMotionSectionImGui() {
        ShowRandomizableImGui(TranslationLabel("component.particlesystembase.lifetime"), lifetime_, 0.01f, 0.0f, 60.0f);
        ShowRandomizableImGui(TranslationLabel("component.particlesystembase.initial_velocity"), initialVelocity_, 0.01f);
        ShowRandomizableImGui(TranslationLabel("component.particlesystembase.acceleration"), acceleration_, 0.01f);
    }

    /// @brief 描画セクション: 描画先・メッシュ・パイプライン・マテリアル・シャドウ・ビルボード
    void ShowRenderingSectionImGui() {
        // 描画先はシーン上のオブジェクトから選択（ヒエラルキーからのD&Dも受け付ける）
        TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), targetObjectID_);

        // メッシュ・パイプライン・マテリアルは、素の文字列入力ではなく
        // 読み込み済みのものから選択する（MeshFilter/SpriteRendererと同じ方式）
        std::vector<std::string> modelPaths;
        for (const auto &entry : ModelManager::GetLoadedModelListEntries()) {
            modelPaths.push_back(entry.assetPath);
        }
        ImGuiCustom::SelectString(TranslationLabel("component.particlesystembase.mesh"), meshAssetPath_, modelPaths, true);
        ImGuiCustom::SelectString(TranslationLabel("component.particlesystembase.pipeline"), pipelineName_, PipelineManager::GetLoadedRenderPipelineNames(), true);
        std::vector<std::string> materialNames;
        for (const auto &entry : MaterialManager::GetLoadedMaterialListEntries()) {
            materialNames.push_back(entry.material.name);
        }
        ImGuiCustom::SelectString(TranslationLabel("component.particlesystembase.material"), materialName_, materialNames);

        ImGui::Checkbox(TranslationLabel("component.particlesystembase.cast_shadows"), &castShadows_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(TranslationC("component.particlesystembase.desc_5"));
        }

        ImGui::Checkbox(TranslationLabel("component.particlesystembase.billboard"), &billboard_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.particlesystembase.targetlookat"));
        }
        if (billboard_) {
            ImGui::Indent();
            // 向き先を明示的に指定しない場合は、シーン内のカメラを自動で使う
            TargetObjectSelector::ShowSelector(TranslationLabel("component.particlesystembase.billboard_target"), GetOwnerSceneContext(), billboardTargetObjectID_, true, false);
            if (!billboardTargetObjectID_.IsValid()) {
                ImGui::TextDisabled("%s", TranslationC("component.particlesystembase.desc_6"));
            }

            const char *kModeLabels[] = { TranslationC("component.common.mode.synctargetrotation"), TranslationC("component.common.mode.lookattarget") };
            int mode = static_cast<int>(billboardRotationMode_);
            if (ImGui::Combo(TranslationLabel("component.particlesystembase.rotation_mode"), &mode, kModeLabels, 2)) {
                billboardRotationMode_ = static_cast<TargetLookAt::RotationMode>(mode);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", TranslationC("component.particlesystembase.desc_7"));
            }
            ImGui::Unindent();
        }
    }

    /// @brief 付属コンポーネントセクション: 生成する各パーティクルへ追加で付けるコンポーネント（Light等）の編集UI。
    ///        通常のオブジェクトインスペクターと同じ「コンポーネント追加・編集・削除」操作を、
    ///        シーンに保存されない使い捨てのEmptyObject（extraComponentsTemplateObject_）を介して行う
    void ShowExtraComponentsImGui() {
        if (gpuSimulation_) {
            ImGui::TextWrapped("%s", TranslationC("component.particlesystembase.extra_components_gpu_disabled"));
            return;
        }
        ImGui::TextWrapped("%s", TranslationC("component.particlesystembase.extra_components_desc"));

        EnsureExtraComponentsTemplateObject();
        if (!extraComponentsTemplateObject_) return;

        IObjectComponent *componentToRemove = nullptr;
        for (const auto &compPair : extraComponentsTemplateObject_->GetAllComponents()) {
            IObjectComponent *comp = compPair.first;
            if (!comp) continue;
            ImGui::PushID(comp);
            if (ImGui::CollapsingHeader(comp->GetComponentType().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                extraComponentsTemplateObject_->ShowComponentImGui(comp);
                if (ImGui::Button(TranslationLabel("component.particlesystembase.remove"))) {
                    componentToRemove = comp;
                }
                ImGui::Unindent();
            }
            ImGui::PopID();
        }
        if (componentToRemove) {
            extraComponentsTemplateObject_->RemoveComponent(componentToRemove);
        }

        if (ImGui::Button(TranslationLabel("editor.component.add"))) {
            ImGui::OpenPopup("AddExtraComponentPopup");
        }
        if (ImGui::BeginPopup("AddExtraComponentPopup")) {
            static const std::unordered_set<std::string> kExcludedTypes = {
                "Transform", "Velocity", "Rotation", "MeshFilter", "MeshRenderer", "SpriteRenderer",
                "ParticleSystem2D", "ParticleSystem3D",
            };
            std::vector<std::string> candidateTypes;
            for (const auto &typeName : GetRegisteredObjectComponentTypes()) {
                if (!kExcludedTypes.contains(typeName)) candidateTypes.push_back(typeName);
            }
            std::string selectedType;
            if (ComponentAddMenu::Show(candidateTypes,
                    [](const std::string &typeName) -> const std::vector<std::string> & { return GetObjectComponentCategory(typeName); },
                    selectedType)) {
                auto newComp = CreateObjectComponentByType(selectedType);
                if (newComp) extraComponentsTemplateObject_->AddComponent(std::move(newComp));
            }
            ImGui::EndPopup();
        }

        SyncExtraComponentTemplatesFromTemplateObject();
    }

    /// @brief extraComponentsTemplateObject_を（無ければ生成し、extraComponentTemplates_から復元して）用意する
    void EnsureExtraComponentsTemplateObject() {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return;

        // EditorOnlyオブジェクトは再生開始時にエンジン側で自動削除されるため、キャッシュした
        // ポインタが既に無効（ダングリング）になっている可能性がある。UpdateParticlesの
        // パーティクルプールと同じ方法（GetSceneObjectでの生存確認）で検知し、無効なら
        // 「削除する」のではなく単にポインタを手放して作り直す（既に破棄済みのため
        // DeleteObjectを呼ぶと二重解放になる）
        if (extraComponentsTemplateObject_ && !sceneContext->GetSceneObject(extraComponentsTemplateObject_)) {
            extraComponentsTemplateObject_ = nullptr;
            extraComponentsTemplateDirty_ = true;
        }

        if (extraComponentsTemplateObject_ && !extraComponentsTemplateDirty_) return;

        if (extraComponentsTemplateObject_) {
            sceneContext->DeleteObject(extraComponentsTemplateObject_);
            extraComponentsTemplateObject_ = nullptr;
        }
        extraComponentsTemplateObject_ = sceneContext->CreateEmptyObject("(Particle Extra Components)");
        if (!extraComponentsTemplateObject_) return;
        // 編集用の内部データであり、シーンファイルへは保存しない・実行時ビルドにも存在させない
        extraComponentsTemplateObject_->SetSaveEnabled(false);
        extraComponentsTemplateObject_->SetEditorOnly(true);
        for (const auto &tmpl : extraComponentTemplates_) {
            extraComponentsTemplateObject_->AddComponentFromJson(tmpl);
        }
        extraComponentsTemplateDirty_ = false;
    }

    /// @brief テンプレートオブジェクトの現在のコンポーネント構成をextraComponentTemplates_（真の保存対象）へ反映する
    void SyncExtraComponentTemplatesFromTemplateObject() {
        if (!extraComponentsTemplateObject_) return;
        extraComponentTemplates_.clear();
        for (const auto &compPair : extraComponentsTemplateObject_->GetAllComponents()) {
            if (!compPair.first) continue;
            extraComponentTemplates_.push_back(extraComponentsTemplateObject_->SaveComponentToJson(compPair.first));
        }
    }

    void DestroyExtraComponentsTemplateObject() {
        if (!extraComponentsTemplateObject_) return;
        auto *sceneContext = GetOwnerSceneContext();
        // EditorOnlyオブジェクトは再生開始時に既に削除されている場合があるため、
        // 生存確認してからDeleteObjectを呼ぶ（二重解放防止）
        if (sceneContext && sceneContext->GetSceneObject(extraComponentsTemplateObject_)) {
            sceneContext->DeleteObject(extraComponentsTemplateObject_);
        }
        extraComponentsTemplateObject_ = nullptr;
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
    /// @brief シャドウマッピングのシャドウキャスターとして扱うか
    bool castShadows_ = true;

    // スポーン範囲: 「含める形状」の和集合から「除外する形状」の和集合を取り除いた領域にスポーンする。
    // 実際のサンプリングはボクセルグリッド方式（RebuildSpawnVoxelGridIfNeeded/ComputeSpawnOffset）で行う
    std::vector<SpawnShapeEntry> includeShapes_{ SpawnShapeEntry{} };
    std::vector<SpawnShapeEntry> excludeShapes_;

    /// @brief GPUシミュレーション（コンピュートシェーダーによる移動・寿命計算）が有効か
    bool gpuSimulation_ = false;

    /// @brief GPUパーティクル描画パスが対象外とする描画先名（現状ImGui/JSONからは編集不可、常に空）
    std::unordered_set<std::string> excludedRenderTargetNames_;

    /// @brief 生成した各パーティクルオブジェクトへ追加で付属させるコンポーネントのテンプレート
    /// @details 各要素は EmptyObject::SaveComponentToJson / AddComponentFromJson と同じ
    ///          {"type":..., "data":{...}} 形式。CPUシミュレーションのプール生成時（EnsurePoolSize）
    ///          にのみ適用され、GPUシミュレーションでは対象となるパーティクルオブジェクトが
    ///          存在しないため効果を持たない
    std::vector<JSON> extraComponentTemplates_;

private:
    /// @brief 2Dパーティクルシステムかどうか（true: Rect/Circleのみ選択可、Capsule非対応）
    const bool is2D_;

    //==================================================
    // CPUオブジェクトプール
    //==================================================

    struct ParticleSlot {
        bool active = false;
        float age = 0.0f;
        float lifetime = 1.0f;
        Vector3 startScale{ 1.0f, 1.0f, 1.0f };
        Vector3 endScale{ 0.0f, 0.0f, 0.0f };
    };
    std::vector<EmptyObject *> pool_;
    std::vector<ParticleSlot> slots_;
    std::vector<int> freeIndices_;

#if defined(USE_IMGUI)
    /// @brief 「付属コンポーネント」編集UI専用の使い捨てオブジェクト（extraComponentTemplates_の
    ///        内容を実際のコンポーネントとして保持し、通常のインスペクターと同じUIで編集できるようにする）。
    ///        SetSaveEnabled(false)のためシーンファイルには保存されず、実行時ビルドにも存在しない
    EmptyObject *extraComponentsTemplateObject_ = nullptr;
    /// @brief LoadBaseFieldsJson等でextraComponentTemplates_が外部から書き換わった際、
    ///        次回のShowExtraComponentsImGuiでテンプレートオブジェクトを作り直す必要があることを示す
    bool extraComponentsTemplateDirty_ = true;
#endif

    /// @brief プールを指定サイズへ伸縮する。拡張分は setupVisual を一度だけ実行してから非アクティブ化する
    void EnsurePoolSize(int desiredSize, const std::function<void(EmptyObject *)> &setupVisual) {
        desiredSize = std::max(0, desiredSize);
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return;

        while (static_cast<int>(pool_.size()) < desiredSize) {
            auto *particleObj = sceneContext->CreateEmptyObject("Particle");
            if (!particleObj) break;
            if (setupVisual) setupVisual(particleObj);
            // 付属コンポーネント（Light等）はCPUプール生成時に一度だけ付与する。
            // GPUシミュレーションはこのメソッド自体を通らないため、ここでの付与は自然にCPUモード限定になる
            for (const auto &extraComponentTemplate : extraComponentTemplates_) {
                particleObj->AddComponentFromJson(extraComponentTemplate);
            }
            if (!particleObj->GetComponent<Velocity>()) particleObj->AddComponent<Velocity>();
            if (!particleObj->GetComponent<Rotation>()) particleObj->AddComponent<Rotation>();
            particleObj->SetActive(false);
            freeIndices_.push_back(static_cast<int>(pool_.size()));
            pool_.push_back(particleObj);
            slots_.push_back(ParticleSlot{});
        }
        while (static_cast<int>(pool_.size()) > desiredSize) {
            const int removeIndex = static_cast<int>(pool_.size()) - 1;
            if (slots_[removeIndex].active) {
                pool_[removeIndex]->SetActive(false);
                slots_[removeIndex].active = false;
            }
            sceneContext->DeleteObject(pool_[removeIndex]);
            pool_.pop_back();
            slots_.pop_back();
            freeIndices_.erase(std::remove(freeIndices_.begin(), freeIndices_.end(), removeIndex), freeIndices_.end());
        }
    }

    /// @brief プールオブジェクトを全て破棄する（コンポーネント破棄時に呼ぶ）
    void DestroyCpuPool() {
        auto *sceneContext = GetOwnerSceneContext();
        if (sceneContext) {
            for (auto *obj : pool_) {
                if (obj && sceneContext->GetSceneObject(obj)) sceneContext->DeleteObject(obj);
            }
        }
        pool_.clear();
        slots_.clear();
        freeIndices_.clear();
    }

    //==================================================
    // GPUシミュレーション用リソース
    //==================================================

    static inline DirectXCommon *sDirectXCommon_ = nullptr;

    std::unique_ptr<RWStructuredBufferResource> gpuParticleBuffer_;
    std::unique_ptr<RWStructuredBufferResource> gpuInstanceMatrixBuffer_;
    std::unique_ptr<StructuredBufferResource> gpuSpawnRequestBuffer_;
    std::unique_ptr<ConstantBufferResource> gpuSpawnConstantBuffer_;
    std::unique_ptr<ConstantBufferResource> gpuUpdateConstantBuffer_;
    /// @brief 空きスロット番号を積む手動スタック（要素数=capacity）。ParticleUpdateCSが死亡時にpush、
    ///        ParticleSpawnCSがスポーン時にpopする（生存中のスロットを誤って上書きしないためのフリーリスト）
    std::unique_ptr<RWStructuredBufferResource> gpuFreeListBuffer_;
    /// @brief gpuFreeListBuffer_の残数を持つ1要素のint32バッファ（HLSL側でInterlockedAddにより増減）
    std::unique_ptr<RWStructuredBufferResource> gpuFreeListCounterBuffer_;
    std::unique_ptr<ConstantBufferResource> gpuFreeListInitConstantBuffer_;
    /// @brief フリーリストへ0..capacity-1を積み直す初期化が必要か（バッファ新規生成・容量変更直後にtrueになる）
    bool gpuFreeListNeedsInit_ = false;
    /// @brief 直近のUpdateParticlesGPUで書き込んだスポーン要求数（Rendererがこの数だけSpawnパイプラインを実行する）
    std::uint32_t gpuFrameSpawnCount_ = 0;
    /// @brief このフレームにUpdateParticlesGPUが実行されたか（ポーズ検出用。RendererがConsumeGpuFrameUpdatedで消費する）
    bool gpuUpdatedThisFrame_ = false;

    void InitializeGpuResources() {
        // 既存バッファを差し替える場合、直前フレームのDispatchがGPU側で完了していることを
        // 保証してから破棄する（未完了のまま破棄すると使用中のUAVを破壊し、GPUクラッシュ/
        // デバイスリムーブを引き起こしうる。Scene::PlayStart/PlayStopでの同種の対策と同じ理由）
        if (sDirectXCommon_ && gpuParticleBuffer_) sDirectXCommon_->WaitForGPUIdle(Passkey<ParticleSystemBase>{});
        const size_t capacity = static_cast<size_t>(std::max(1, maxParticles_));
        gpuParticleBuffer_ = std::make_unique<RWStructuredBufferResource>(sizeof(GPUParticleData), capacity, false);
        gpuInstanceMatrixBuffer_ = std::make_unique<RWStructuredBufferResource>(sizeof(Matrix4x4), capacity, true);
        gpuSpawnRequestBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(GPUParticleSpawnRequest), capacity);
        gpuSpawnConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(GPUParticleSpawnConstants));
        gpuUpdateConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(GPUParticleUpdateConstants));
        gpuFreeListBuffer_ = std::make_unique<RWStructuredBufferResource>(sizeof(std::uint32_t), capacity, false);
        gpuFreeListCounterBuffer_ = std::make_unique<RWStructuredBufferResource>(sizeof(std::int32_t), 1, false);
        gpuFreeListInitConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(GPUParticleFreeListInitConstants));
        gpuFreeListNeedsInit_ = true;
        gpuFrameSpawnCount_ = 0;
    }

    void DestroyGpuResources() {
        if (sDirectXCommon_ && gpuParticleBuffer_) sDirectXCommon_->WaitForGPUIdle(Passkey<ParticleSystemBase>{});
        gpuParticleBuffer_.reset();
        gpuInstanceMatrixBuffer_.reset();
        gpuSpawnRequestBuffer_.reset();
        gpuSpawnConstantBuffer_.reset();
        gpuUpdateConstantBuffer_.reset();
        gpuFreeListBuffer_.reset();
        gpuFreeListCounterBuffer_.reset();
        gpuFreeListInitConstantBuffer_.reset();
        gpuFreeListNeedsInit_ = false;
        gpuFrameSpawnCount_ = 0;
    }

    /// @brief Max Particles変更などでGPUバッファの容量が実際の設定と合わなくなった場合に再生成する
    void EnsureGpuCapacity() {
        if (!gpuSimulation_) return;
        const size_t desired = static_cast<size_t>(std::max(1, maxParticles_));
        if (gpuParticleBuffer_ && gpuParticleBuffer_->GetElementCount() == desired) return;
        InitializeGpuResources();
    }

    void UploadGpuSpawnRequests(const std::vector<GPUParticleSpawnRequest> &requests) {
        EnsureGpuCapacity();
        gpuFrameSpawnCount_ = static_cast<std::uint32_t>(requests.size());
        if (requests.empty() || !gpuSpawnRequestBuffer_) return;
        void *mapped = gpuSpawnRequestBuffer_->Map();
        if (!mapped) return;
        std::memcpy(mapped, requests.data(), requests.size() * sizeof(GPUParticleSpawnRequest));
    }

    /// @brief GPU/CPUシミュレーション方式の切り替え時に、現在の方式のリソースを破棄し新方式を初期化する
    void SwitchSimulationMode() {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (gpuSimulation_) {
            Clear();
            DestroyCpuPool();
            InitializeGpuResources();
            if (!sceneRenderer) sceneRenderer = GetOrAddSceneRenderer();
            if (sceneRenderer) {
                sceneRenderer->RegisterGpuParticleEmitter(this);
                char buf[128];
                std::snprintf(buf, sizeof(buf), "(this=%p, sceneRenderer=%p)", static_cast<const void *>(this), static_cast<const void *>(sceneRenderer));
                Log(Translation("engine.particlesystem.gpu.emitter.registered") + buf, LogSeverity::Info);
            } else {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "(sceneContext=%p, this=%p)", static_cast<const void *>(sceneContext), static_cast<const void *>(this));
                Log(Translation("engine.particlesystem.gpu.scenerenderer.notfound") + buf, LogSeverity::Warning);
            }
        } else {
            if (sceneRenderer) sceneRenderer->UnregisterGpuParticleEmitter(this);
            DestroyGpuResources();
            // CPUプールは次回UpdateParticles呼び出し時にEnsurePoolSizeで再構築される
        }
    }

    SceneRenderer *GetOrAddSceneRenderer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        return sceneRenderer;
    }

    //==================================================
    // スポーン位置・親の解決（CPU/GPU共通ロジック）
    //==================================================

    /// @brief 点がある1つの形状の内側にあるかを判定する（Pointは体積0のため常にfalseを返す。呼び出し側で別扱いする）
    static bool IsPointInShape(const Vector3 &point, const SpawnShapeEntry &shape, bool is2D) {
        const Vector3 local = point - shape.center;
        switch (shape.type) {
        case SpawnShape::Box: {
            const Vector3 half = shape.boxSize * 0.5f;
            if (is2D) return std::abs(local.x) <= half.x && std::abs(local.y) <= half.y;
            return std::abs(local.x) <= half.x && std::abs(local.y) <= half.y && std::abs(local.z) <= half.z;
        }
        case SpawnShape::Sphere: {
            const float r2 = shape.sphereRadius * shape.sphereRadius;
            if (is2D) return (local.x * local.x + local.y * local.y) <= r2;
            return local.LengthSquared() <= r2;
        }
        case SpawnShape::Capsule: {
            const float halfHeight = shape.capsuleHeight * 0.5f;
            const float closestY = std::clamp(local.y, -halfHeight, halfHeight);
            const Vector3 radial = local - Vector3(0.0f, closestY, 0.0f);
            return radial.LengthSquared() <= shape.capsuleRadius * shape.capsuleRadius;
        }
        case SpawnShape::Point:
        default:
            return false;
        }
    }

    /// @brief 形状1個ぶんの外接AABBを求める
    static void GetShapeBounds(const SpawnShapeEntry &shape, bool is2D, Vector3 &outMin, Vector3 &outMax) {
        switch (shape.type) {
        case SpawnShape::Box: {
            Vector3 half = shape.boxSize * 0.5f;
            if (is2D) half.z = 0.0f;
            outMin = shape.center - half;
            outMax = shape.center + half;
            break;
        }
        case SpawnShape::Sphere: {
            const Vector3 r(shape.sphereRadius, shape.sphereRadius, is2D ? 0.0f : shape.sphereRadius);
            outMin = shape.center - r;
            outMax = shape.center + r;
            break;
        }
        case SpawnShape::Capsule: {
            const float halfExtentY = shape.capsuleHeight * 0.5f + shape.capsuleRadius;
            const Vector3 ext(shape.capsuleRadius, halfExtentY, shape.capsuleRadius);
            outMin = shape.center - ext;
            outMax = shape.center + ext;
            break;
        }
        case SpawnShape::Point:
        default:
            outMin = outMax = shape.center;
            break;
        }
    }

    static Vector3 ComponentMin(const Vector3 &a, const Vector3 &b) {
        return Vector3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
    }
    static Vector3 ComponentMax(const Vector3 &a, const Vector3 &b) {
        return Vector3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
    }

    /// @brief 点がスポーン対象として有効か（含める形状のどれかの内側、かつ除外形状のどれの内側でもない）を判定する
    /// @details includeShapes_中のPoint形状は体積0のため、ここでの体積ベースの判定対象には含めない
    ///          （RebuildSpawnVoxelGridIfNeeded側で別途候補セルとして扱う）
    bool IsPointValidForSpawn(const Vector3 &point) const {
        bool insideInclude = false;
        for (const auto &shape : includeShapes_) {
            if (shape.type == SpawnShape::Point) continue;
            if (IsPointInShape(point, shape, is2D_)) { insideInclude = true; break; }
        }
        if (!insideInclude) return false;
        for (const auto &shape : excludeShapes_) {
            if (shape.type == SpawnShape::Point) continue;
            if (IsPointInShape(point, shape, is2D_)) return false;
        }
        return true;
    }

    /// @brief スポーン範囲のボクセルグリッド上の1候補セル（体積を持つボクセル、または体積0のPoint形状そのもの）
    struct SpawnVoxelCell {
        bool isPoint = false;
        /// @brief isPoint==trueの場合は厳密な位置。falseの場合はボクセルの最小コーナー（spawnVoxelSize_を加えた範囲内でランダムにサンプルする）
        Vector3 position{ 0.0f, 0.0f, 0.0f };
    };
    std::vector<SpawnVoxelCell> spawnVoxelCandidates_;
    Vector3 spawnVoxelSize_{ 0.0f, 0.0f, 0.0f };
    bool spawnVoxelGridDirty_ = true;
    /// @brief ボクセル1辺のサイズ
    static constexpr float kSpawnVoxelSize = 0.25f;
    /// @brief 1軸あたりの最大分割数（巨大なAABB×極小ボクセルサイズによるメモリ暴走を防ぐ）
    static constexpr int kSpawnVoxelGridMaxAxisCount = 48;

    void MarkSpawnVoxelGridDirty() noexcept { spawnVoxelGridDirty_ = true; }

    /// @brief includeShapes_/excludeShapes_からスポーン候補セル一覧を再構築する（形状が変わった時だけ実行する重い処理）
    void RebuildSpawnVoxelGridIfNeeded() {
        if (!spawnVoxelGridDirty_) return;
        spawnVoxelGridDirty_ = false;
        spawnVoxelCandidates_.clear();

        // Point形状は体積を持たないため、ボクセル化とは別に候補セルとして直接追加する
        // （除外形状の内側に入っている場合はスポーン対象から外す）
        for (const auto &shape : includeShapes_) {
            if (shape.type != SpawnShape::Point) continue;
            bool excluded = false;
            for (const auto &ex : excludeShapes_) {
                if (ex.type == SpawnShape::Point) continue;
                if (IsPointInShape(shape.center, ex, is2D_)) { excluded = true; break; }
            }
            if (!excluded) spawnVoxelCandidates_.push_back(SpawnVoxelCell{ true, shape.center });
        }

        bool hasVolumeShape = false;
        for (const auto &shape : includeShapes_) {
            if (shape.type != SpawnShape::Point) { hasVolumeShape = true; break; }
        }
        if (!hasVolumeShape) return;

        constexpr float kFloatMax = std::numeric_limits<float>::max();
        Vector3 aabbMin(kFloatMax, kFloatMax, kFloatMax);
        Vector3 aabbMax(-kFloatMax, -kFloatMax, -kFloatMax);
        for (const auto &shape : includeShapes_) {
            if (shape.type == SpawnShape::Point) continue;
            Vector3 shapeMin, shapeMax;
            GetShapeBounds(shape, is2D_, shapeMin, shapeMax);
            aabbMin = ComponentMin(aabbMin, shapeMin);
            aabbMax = ComponentMax(aabbMax, shapeMax);
        }
        if (is2D_) { aabbMin.z = 0.0f; aabbMax.z = 0.0f; }

        const Vector3 size = aabbMax - aabbMin;
        if (size.x <= 0.0f || size.y <= 0.0f || (!is2D_ && size.z <= 0.0f)) return;

        const int nx = std::clamp(static_cast<int>(std::ceil(size.x / kSpawnVoxelSize)), 1, kSpawnVoxelGridMaxAxisCount);
        const int ny = std::clamp(static_cast<int>(std::ceil(size.y / kSpawnVoxelSize)), 1, kSpawnVoxelGridMaxAxisCount);
        const int nz = is2D_ ? 1 : std::clamp(static_cast<int>(std::ceil(size.z / kSpawnVoxelSize)), 1, kSpawnVoxelGridMaxAxisCount);

        spawnVoxelSize_ = Vector3(size.x / static_cast<float>(nx), size.y / static_cast<float>(ny), is2D_ ? 0.0f : size.z / static_cast<float>(nz));

        for (int ix = 0; ix < nx; ++ix) {
            for (int iy = 0; iy < ny; ++iy) {
                for (int iz = 0; iz < nz; ++iz) {
                    const Vector3 voxelMin = aabbMin + Vector3(
                        static_cast<float>(ix) * spawnVoxelSize_.x,
                        static_cast<float>(iy) * spawnVoxelSize_.y,
                        is2D_ ? 0.0f : static_cast<float>(iz) * spawnVoxelSize_.z);
                    const Vector3 voxelCenter = voxelMin + spawnVoxelSize_ * 0.5f;
                    if (IsPointValidForSpawn(voxelCenter)) {
                        spawnVoxelCandidates_.push_back(SpawnVoxelCell{ false, voxelMin });
                    }
                }
            }
        }
    }

    /// @brief 現在のスポーン範囲設定（含める形状の和集合から除外形状の和集合を除いた領域）に基づき、
    ///        オーナー位置からのランダムなオフセットを求める
    /// @details 候補セル一覧（ボクセルグリッド）から1つをランダムに選び、その中でランダムな点を打つ方式。
    ///          有効なスポーン範囲が無い場合（形状未設定・除外形状で全て覆われている等）は原点を返す
    Vector3 ComputeSpawnOffset() {
        RebuildSpawnVoxelGridIfNeeded();
        if (spawnVoxelCandidates_.empty()) return Vector3(0.0f, 0.0f, 0.0f);

        const auto &cell = spawnVoxelCandidates_[GetRandomInt(0, static_cast<int>(spawnVoxelCandidates_.size()) - 1)];
        if (cell.isPoint) return cell.position;

        // ボクセル内でランダムな点を打つ。含める形状の境界・除外形状の境界をまたぐボクセルの場合のみ、
        // 小規模なリジェクション再試行を行う（ボクセルは十分小さいため、有効判定済みの中心付近を
        // 返せば大きく破綻しない）
        constexpr int kMaxAttempts = 8;
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            const Vector3 p = cell.position + Vector3(
                GetRandomFloat(0.0f, spawnVoxelSize_.x),
                GetRandomFloat(0.0f, spawnVoxelSize_.y),
                is2D_ ? 0.0f : GetRandomFloat(0.0f, spawnVoxelSize_.z));
            if (IsPointValidForSpawn(p)) return p;
        }
        return cell.position + spawnVoxelSize_ * 0.5f;
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
    /// @details GPUモードでは outParent は「生成時点の座標を取るためだけ」に使われ、実際の親付けは行われない
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

    void SpawnParticle() {
        if (freeIndices_.empty()) return;
        auto *owner = ResolveMutableOwner();
        if (!owner) return;

        const int slotIndex = freeIndices_.back();
        freeIndices_.pop_back();
        auto *particleObj = pool_[slotIndex];
        if (!particleObj) return;

        EmptyObject *parentObject = nullptr;
        Vector3 basePosition{ 0.0f, 0.0f, 0.0f };
        ResolveSpawnAnchor(owner, parentObject, basePosition);

        const Vector3 chosenStartScale = startScale_.Get();
        const Vector3 chosenEndScale = endScale_.Get();
        const Vector3 rotationDegrees = initialRotation_.Get();

        if (auto *transform = particleObj->GetComponent<Transform>()) {
            // 前回の生存時に別の親が設定されていた場合の取り残しを防ぐため、
            // 親付けしない生成方式の場合は明示的にnullptrへ戻す
            transform->SetParentObject(parentObject);
            transform->SetScale(chosenStartScale);
            transform->SetTranslate(basePosition + ComputeSpawnOffset());
            transform->SetRotate(Vector3(ToRadians(rotationDegrees.x), ToRadians(rotationDegrees.y), ToRadians(rotationDegrees.z)));
        }

        particleObj->SetActive(true);

        if (auto *velocity = particleObj->GetComponent<Velocity>()) {
            velocity->SetVelocity(initialVelocity_.Get());
            velocity->SetAcceleration(acceleration_.Get());
        }

        const Vector3 rotationSpeedDegrees = initialRotationSpeed_.Get();
        const Vector3 rotationAccelDegrees = rotationAcceleration_.Get();
        if (auto *rotation = particleObj->GetComponent<Rotation>()) {
            rotation->SetAngularVelocity(Vector3(ToRadians(rotationSpeedDegrees.x), ToRadians(rotationSpeedDegrees.y), ToRadians(rotationSpeedDegrees.z)));
            rotation->SetAngularAcceleration(Vector3(ToRadians(rotationAccelDegrees.x), ToRadians(rotationAccelDegrees.y), ToRadians(rotationAccelDegrees.z)));
        }

        if (billboard_) {
            auto *lookAt = particleObj->GetComponent<TargetLookAt>();
            if (!lookAt) lookAt = particleObj->AddComponent<TargetLookAt>();
            if (lookAt) {
                lookAt->SetRotationMode(billboardRotationMode_);
                // 向き先が明示的に指定されていればそれを使い、未設定ならシーン内のカメラを自動で使う
                EmptyObject *billboardTarget = GetBillboardTarget();
                if (!billboardTarget) billboardTarget = ResolveBillboardCameraObject();
                lookAt->SetTargetObject(billboardTarget);
            }
        } else {
            particleObj->RemoveComponent<TargetLookAt>();
        }

        slots_[slotIndex].active = true;
        slots_[slotIndex].age = 0.0f;
        slots_[slotIndex].lifetime = std::max(0.01f, lifetime_.Get());
        slots_[slotIndex].startScale = chosenStartScale;
        slots_[slotIndex].endScale = chosenEndScale;
        ++totalEmittedCount_;
    }

    float spawnTimer_ = 0.0f;
    int totalEmittedCount_ = 0;
};

} // namespace KashipanEngine
