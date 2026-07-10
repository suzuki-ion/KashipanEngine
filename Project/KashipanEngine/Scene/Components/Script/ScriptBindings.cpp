#include "Scene/Components/Script/ScriptBindings.h"

#include <fstream>
#include <map>
#include <string_view>
#include <unordered_map>

#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <asbind20/asbind.hpp>

#include "Assets/AudioManager.h"
#include "Debug/Logger.h"
#include "Input/InputCommand.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Objects/EmptyObject.h"
#include "Objects/ObjectContext.h"
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"

// オブジェクトコンポーネント（全種類をスクリプトへ登録する）
#include "Objects/Components/Animator.h"
#include "Objects/Components/AudioListener.h"
#include "Objects/Components/AudioSource.h"
#include "Objects/Components/Collider/Box2DCollider.h"
#include "Objects/Components/Collider/BoxCollider.h"
#include "Objects/Components/Collider/Capsule2DCollider.h"
#include "Objects/Components/Collider/CapsuleCollider.h"
#include "Objects/Components/Collider/Circle2DCollider.h"
#include "Objects/Components/Collider/MeshCollider.h"
#include "Objects/Components/Collider/Ray2DCollider.h"
#include "Objects/Components/Collider/RayCollider.h"
#include "Objects/Components/Collider/RigidBody2D.h"
#include "Objects/Components/Collider/RigidBody3D.h"
#include "Objects/Components/Collider/SphereCollider.h"
#include "Objects/Components/Compute/ComputeShaderProcessing.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/PostProcessing/BloomEffect.h"
#include "Objects/Components/PostProcessing/BoxFilterEffect.h"
#include "Objects/Components/PostProcessing/ChromaticAberrationEffect.h"
#include "Objects/Components/PostProcessing/ColorAdjustEffect.h"
#include "Objects/Components/PostProcessing/DissolveEffect.h"
#include "Objects/Components/PostProcessing/DitherEffect.h"
#include "Objects/Components/PostProcessing/DotMatrixEffect.h"
#include "Objects/Components/PostProcessing/FXAAEffect.h"
#include "Objects/Components/PostProcessing/GaussianFilterEffect.h"
#include "Objects/Components/PostProcessing/GrayscaleEffect.h"
#include "Objects/Components/PostProcessing/OutlineEffect.h"
#include "Objects/Components/PostProcessing/RadialBlurEffect.h"
#include "Objects/Components/PostProcessing/VignetteEffect.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/CameraController.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/ShadowMapObject.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/Text.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Velocity.h"

namespace KashipanEngine {

namespace {

/// @brief 現在実行中のスクリプトのオーナーコンテキスト（ScriptExecutionScopeで設定される）
ObjectContext *gCurrentObjectContext = nullptr;
SceneContext *gCurrentSceneContext = nullptr;

/// @brief スクリプトの型ID→コンポーネント取得処理のマップ（GetComponent(?&out)用）
struct ComponentTypeBinding {
    IObjectComponent *(*getOne)(EmptyObject &) = nullptr;
    std::vector<IObjectComponent *> (*getAll)(EmptyObject &) = nullptr;
};
std::unordered_map<int, ComponentTypeBinding> gComponentTypeBindings;

//==================================================
// 数学型
//==================================================

void RegisterMathTypes(asIScriptEngine *engine) {
    // 全メンバがfloatのPOD型はネイティブ呼び出し規約のためにALLFLOATSの指定が必要
    constexpr asQWORD kMathClassFlags = asOBJ_APP_CLASS_ALLFLOATS | asOBJ_APP_CLASS_MORE_CONSTRUCTORS;

    asbind20::value_class<Vector2>(engine, "Vector2", kMathClassFlags)
        .behaviours_by_traits()
        .constructor<float, float>("float x, float y")
        .property("float x", &Vector2::x)
        .property("float y", &Vector2::y)
        .opNeg()
        .opAdd()
        .opSub()
        .opDiv()
        .opEquals()
        .opAddAssign()
        .opSubAssign()
        .method("Vector2 opMul(float) const", [](const Vector2 &v, float s) -> Vector2 { return v * s; })
        .method("Vector2 opMul_r(float) const", [](const Vector2 &v, float s) -> Vector2 { return s * v; })
        .method("Vector2 opDiv(float) const", [](const Vector2 &v, float s) -> Vector2 { return v / s; })
        .method("float Dot(const Vector2 &in) const", &Vector2::Dot)
        .method("float Cross(const Vector2 &in) const", &Vector2::Cross)
        .method("float Length() const", &Vector2::Length)
        .method("float LengthSquared() const", &Vector2::LengthSquared)
        .method("Vector2 Normalize() const", &Vector2::Normalize)
        .method("float Distance(const Vector2 &in) const", &Vector2::Distance);

    asbind20::value_class<Vector3>(engine, "Vector3", kMathClassFlags)
        .behaviours_by_traits()
        .constructor<float, float, float>("float x, float y, float z")
        .property("float x", &Vector3::x)
        .property("float y", &Vector3::y)
        .property("float z", &Vector3::z)
        .opNeg()
        .opAdd()
        .opSub()
        .opDiv()
        .opEquals()
        .opAddAssign()
        .opSubAssign()
        .method("Vector3 opMul(float) const", [](const Vector3 &v, float s) -> Vector3 { return v * s; })
        .method("Vector3 opMul_r(float) const", [](const Vector3 &v, float s) -> Vector3 { return s * v; })
        .method("Vector3 opDiv(float) const", [](const Vector3 &v, float s) -> Vector3 { return v / s; })
        .method("float Dot(const Vector3 &in) const", &Vector3::Dot)
        .method("Vector3 Cross(const Vector3 &in) const", &Vector3::Cross)
        .method("float Length() const", &Vector3::Length)
        .method("float LengthSquared() const", &Vector3::LengthSquared)
        .method("Vector3 Normalize() const", &Vector3::Normalize)
        .method("float Distance(const Vector3 &in) const", &Vector3::Distance);

    asbind20::value_class<Vector4>(engine, "Vector4", kMathClassFlags)
        .behaviours_by_traits()
        .constructor<float, float, float, float>("float x, float y, float z, float w")
        .property("float x", &Vector4::x)
        .property("float y", &Vector4::y)
        .property("float z", &Vector4::z)
        .property("float w", &Vector4::w)
        .opNeg()
        .opAdd()
        .opSub()
        .opDiv()
        .opEquals()
        .opAddAssign()
        .opSubAssign()
        .method("Vector4 opMul(float) const", [](const Vector4 &v, float s) -> Vector4 { return v * s; })
        .method("Vector4 opMul_r(float) const", [](const Vector4 &v, float s) -> Vector4 { return s * v; })
        .method("Vector4 opDiv(float) const", [](const Vector4 &v, float s) -> Vector4 { return v / s; });

    asbind20::value_class<Quaternion>(engine, "Quaternion", kMathClassFlags)
        .behaviours_by_traits()
        .constructor<float, float, float, float>("float x, float y, float z, float w")
        .property("float x", &Quaternion::x)
        .property("float y", &Quaternion::y)
        .property("float z", &Quaternion::z)
        .property("float w", &Quaternion::w)
        .opAdd()
        .opSub()
        .opMul()
        .method("Quaternion opMul(float) const", [](const Quaternion &q, float s) -> Quaternion { return q * s; })
        .method("Quaternion opDiv(float) const", [](const Quaternion &q, float s) -> Quaternion { return q / s; })
        .method("Quaternion Conjugate() const", &Quaternion::Conjugate)
        .method("float Norm() const", &Quaternion::Norm)
        .method("float NormSquared() const", &Quaternion::NormSquared)
        .method("Quaternion Normalize() const", &Quaternion::Normalize)
        .method("Quaternion Inverse() const", &Quaternion::Inverse)
        .method("Vector3 RotateVector(const Vector3 &in) const", &Quaternion::RotateVector);

    asbind20::value_class<Matrix3x3>(engine, "Matrix3x3", kMathClassFlags)
        .behaviours_by_traits()
        .constructor<float, float, float, float, float, float, float, float, float>(
            "float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22")
        .opAdd()
        .opSub()
        .opMul()
        .method("Matrix3x3 opMul(float) const", [](const Matrix3x3 &m, float s) -> Matrix3x3 { return m * s; })
        .method("float GetElement(uint row, uint col) const", [](const Matrix3x3 &m, uint32_t row, uint32_t col) -> float {
            return (row < 3 && col < 3) ? m.m[row][col] : 0.0f;
        })
        .method("void SetElement(uint row, uint col, float value)", [](Matrix3x3 &m, uint32_t row, uint32_t col, float value) {
            if (row < 3 && col < 3) m.m[row][col] = value;
        })
        .method("Matrix3x3 Transpose() const", &Matrix3x3::Transpose)
        .method("float Determinant() const", &Matrix3x3::Determinant)
        .method("Matrix3x3 Inverse() const", &Matrix3x3::Inverse)
        .method("void MakeIdentity()", &Matrix3x3::MakeIdentity)
        .method("void MakeTranspose()", &Matrix3x3::MakeTranspose)
        .method("void MakeInverse()", &Matrix3x3::MakeInverse)
        .method("void MakeTranslate(const Vector2 &in)", &Matrix3x3::MakeTranslate)
        .method("void MakeScale(const Vector2 &in)", &Matrix3x3::MakeScale)
        .method("void MakeRotate(float)", &Matrix3x3::MakeRotate)
        .method("void MakeAffine(const Vector2 &in, float, const Vector2 &in)", &Matrix3x3::MakeAffine);

    asbind20::value_class<Matrix4x4>(engine, "Matrix4x4", kMathClassFlags)
        .behaviours_by_traits()
        .constructor<
            float, float, float, float, float, float, float, float,
            float, float, float, float, float, float, float, float>(
            "float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, "
            "float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33")
        .opAdd()
        .opSub()
        .opMul()
        .method("Matrix4x4 opMul(float) const", [](const Matrix4x4 &m, float s) -> Matrix4x4 { return m * s; })
        .method("float GetElement(uint row, uint col) const", [](const Matrix4x4 &m, uint32_t row, uint32_t col) -> float {
            return (row < 4 && col < 4) ? m.m[row][col] : 0.0f;
        })
        .method("void SetElement(uint row, uint col, float value)", [](Matrix4x4 &m, uint32_t row, uint32_t col, float value) {
            if (row < 4 && col < 4) m.m[row][col] = value;
        })
        .method("Matrix4x4 Transpose()", &Matrix4x4::Transpose)
        .method("float Determinant() const", &Matrix4x4::Determinant)
        .method("Matrix4x4 Inverse() const", &Matrix4x4::Inverse)
        .method("void MakeIdentity()", &Matrix4x4::MakeIdentity)
        .method("void MakeTranspose()", &Matrix4x4::MakeTranspose)
        .method("void MakeInverse()", &Matrix4x4::MakeInverse)
        .method("void MakeTranslate(const Vector3 &in)", &Matrix4x4::MakeTranslate)
        .method("void MakeScale(const Vector3 &in)", &Matrix4x4::MakeScale)
        .method("void MakeRotate(const Vector3 &in)", static_cast<void (Matrix4x4::*)(const Vector3 &) noexcept>(&Matrix4x4::MakeRotate))
        .method("void MakeRotate(float, float, float)", static_cast<void (Matrix4x4::*)(float, float, float) noexcept>(&Matrix4x4::MakeRotate))
        .method("void MakeRotateX(float)", &Matrix4x4::MakeRotateX)
        .method("void MakeRotateY(float)", &Matrix4x4::MakeRotateY)
        .method("void MakeRotateZ(float)", &Matrix4x4::MakeRotateZ)
        .method("void MakeAffine(const Vector3 &in, const Vector3 &in, const Vector3 &in)", &Matrix4x4::MakeAffine)
        .method("void MakeViewMatrix(const Vector3 &in, const Vector3 &in, const Vector3 &in)", &Matrix4x4::MakeViewMatrix)
        .method("void MakePerspectiveFovMatrix(float, float, float, float)", &Matrix4x4::MakePerspectiveFovMatrix)
        .method("void MakeOrthographicMatrix(float, float, float, float, float, float)", &Matrix4x4::MakeOrthographicMatrix)
        .method("void MakeViewportMatrix(float, float, float, float, float, float)", &Matrix4x4::MakeViewportMatrix);

    // 行列を使用する相互参照メソッドは行列登録後に追加する
    asbind20::value_class<Vector2>(asbind20::appending, engine, "Vector2")
        .method("Vector2 opMul(const Matrix3x3 &in) const", [](const Vector2 &v, const Matrix3x3 &m) -> Vector2 { return v * m; })
        .method("Vector2 opMul_r(const Matrix3x3 &in) const", [](const Vector2 &v, const Matrix3x3 &m) -> Vector2 { return m * v; });

    asbind20::value_class<Vector3>(asbind20::appending, engine, "Vector3")
        .method("Vector3 Transform(const Matrix4x4 &in) const", &Vector3::Transform)
        .method("Vector3 opMul(const Matrix4x4 &in) const", [](const Vector3 &v, const Matrix4x4 &m) -> Vector3 { return v * m; })
        .method("Vector3 opMul_r(const Matrix4x4 &in) const", [](const Vector3 &v, const Matrix4x4 &m) -> Vector3 { return m * v; });

    asbind20::value_class<Quaternion>(asbind20::appending, engine, "Quaternion")
        .method("Matrix4x4 MakeRotateMatrix() const", &Quaternion::MakeRotateMatrix);

    // 静的関数はMath名前空間へまとめて登録する（スクリプト側からは Math::Lerp(...) のように呼ぶ）
    asbind20::namespace_ mathNamespace(engine, "Math");
    asbind20::global(engine)
        .function("Vector2 Lerp(const Vector2 &in, const Vector2 &in, float)", &Vector2::Lerp)
        .function("Vector2 Slerp(const Vector2 &in, const Vector2 &in, float)", &Vector2::Slerp)
        .function("Vector3 Lerp(const Vector3 &in, const Vector3 &in, float)", &Vector3::Lerp)
        .function("Vector3 Slerp(const Vector3 &in, const Vector3 &in, float)", &Vector3::Slerp)
        .function("Vector4 Lerp(const Vector4 &in, const Vector4 &in, float)", &Vector4::Lerp)
        .function("Quaternion Slerp(const Quaternion &in, const Quaternion &in, float)", &Quaternion::Slerp)
        .function("Quaternion IdentityQuaternion()", &Quaternion::Identity)
        .function("Quaternion MakeRotateEuler(const Vector3 &in)", &Quaternion::MakeRotateEuler)
        .function("Quaternion MakeRotateAxisAngle(const Vector3 &in, float)", &Quaternion::MakeRotateAxisAngle)
        .function("Quaternion MakeFromRotationMatrix(const Matrix4x4 &in)", &Quaternion::MakeFromRotationMatrix)
        .function("Matrix3x3 IdentityMatrix3x3()", &Matrix3x3::Identity)
        .function("Matrix4x4 IdentityMatrix4x4()", &Matrix4x4::Identity);
}

//==================================================
// コンポーネント型
//==================================================

/// @brief コンポーネント型を参照型として登録し、GetComponent(?&out)用の取得処理をマップへ追加する
/// @details IObjectComponent共通のメソッドも合わせて登録する。戻り値のバインダで型固有のメソッドを追加できる
template <typename T>
auto RegisterComponentType(asIScriptEngine *engine, const char *name) {
    auto binder = asbind20::ref_class<T>(engine, name, asOBJ_NOCOUNT);
    binder
        .method("bool IsActive() const", static_cast<bool (T::*)() const>(&T::IsActive))
        .method("void SetActive(bool)", static_cast<void (T::*)(bool)>(&T::SetActive))
        .method("const string &GetComponentType() const", static_cast<const std::string &(T::*)() const>(&T::GetComponentType));

    const int typeId = engine->GetTypeIdByDecl(name);
    gComponentTypeBindings[typeId] = ComponentTypeBinding{
        +[](EmptyObject &obj) -> IObjectComponent * { return obj.GetComponent<T>(); },
        +[](EmptyObject &obj) -> std::vector<IObjectComponent *> {
            auto typed = obj.GetComponents<T>();
            return std::vector<IObjectComponent *>(typed.begin(), typed.end());
        },
    };
    return binder;
}

/// @brief コライダー型を登録する（ICollider共通のメソッドを追加で登録する）
template <typename T>
auto RegisterColliderType(asIScriptEngine *engine, const char *name) {
    auto binder = RegisterComponentType<T>(engine, name);
    binder
        .method("bool IsTrigger() const", static_cast<bool (T::*)() const noexcept>(&T::IsTrigger))
        .method("void SetTrigger(bool)", static_cast<void (T::*)(bool) noexcept>(&T::SetTrigger))
        .method("bool Is2D() const", static_cast<bool (T::*)() const noexcept>(&T::Is2D));
    return binder;
}

void RegisterComponentTypes(asIScriptEngine *engine) {
    gComponentTypeBindings.clear();

    RegisterComponentType<Transform>(engine, "Transform")
        .method("void SetTranslate(const Vector3 &in)", &Transform::SetTranslate)
        .method("const Vector3 &GetTranslate() const", &Transform::GetTranslate)
        .method("void SetRotate(const Vector3 &in)", &Transform::SetRotate)
        .method("const Vector3 &GetRotate() const", &Transform::GetRotate)
        .method("void SetRotateQuaternion(const Quaternion &in)", &Transform::SetRotateQuaternion)
        .method("const Quaternion &GetRotateQuaternion() const", &Transform::GetRotateQuaternion)
        .method("void SetScale(const Vector3 &in)", &Transform::SetScale)
        .method("const Vector3 &GetScale() const", &Transform::GetScale)
        .method("const Matrix4x4 &GetWorldMatrix()", &Transform::GetWorldMatrix);

    RegisterComponentType<Velocity>(engine, "Velocity")
        .method("void SetVelocity(const Vector3 &in)", &Velocity::SetVelocity)
        .method("const Vector3 &GetVelocity() const", &Velocity::GetVelocity)
        .method("void SetAcceleration(const Vector3 &in)", &Velocity::SetAcceleration)
        .method("const Vector3 &GetAcceleration() const", &Velocity::GetAcceleration)
        .method("void AddVelocity(const Vector3 &in)", &Velocity::AddVelocity);

    RegisterComponentType<AudioSource>(engine, "AudioSource")
        .method("uint Play()", &AudioSource::Play)
        .method("void Stop()", &AudioSource::Stop)
        .method("bool Pause()", &AudioSource::Pause)
        .method("bool Resume()", &AudioSource::Resume)
        .method("bool IsPlaying() const", &AudioSource::IsPlaying)
        .method("bool IsPaused() const", &AudioSource::IsPaused)
        .method("void SetSoundName(const string &in)", &AudioSource::SetSoundName)
        .method("const string &GetSoundName() const", &AudioSource::GetSoundName)
        .method("void SetVolume(float)", &AudioSource::SetVolume)
        .method("float GetVolume() const", &AudioSource::GetVolume)
        .method("void SetPitch(float)", &AudioSource::SetPitch)
        .method("float GetPitch() const", &AudioSource::GetPitch)
        .method("void SetLoop(bool)", &AudioSource::SetLoop)
        .method("bool GetLoop() const", &AudioSource::GetLoop);

    RegisterComponentType<AudioListener>(engine, "AudioListener")
        .method("void SetUsed(bool)", &AudioListener::SetUsed)
        .method("bool GetUsed() const", &AudioListener::GetUsed)
        .method("Vector3 GetWorldPosition() const", &AudioListener::GetWorldPosition);

    RegisterComponentType<Camera3D>(engine, "Camera3D")
        .method("void SetFovY(float)", &Camera3D::SetFovY)
        .method("float GetFovY() const", &Camera3D::GetFovY)
        .method("void SetNearClip(float)", &Camera3D::SetNearClip)
        .method("float GetNearClip() const", &Camera3D::GetNearClip)
        .method("void SetFarClip(float)", &Camera3D::SetFarClip)
        .method("float GetFarClip() const", &Camera3D::GetFarClip)
        .method("void SetAspectRatio(float)", &Camera3D::SetAspectRatio)
        .method("float GetAspectRatio() const", &Camera3D::GetAspectRatio)
        .method("void SetOrthographic(bool)", &Camera3D::SetOrthographic)
        .method("bool IsOrthographic() const", &Camera3D::IsOrthographic)
        .method("void SetOrthoSize(float)", &Camera3D::SetOrthoSize)
        .method("float GetOrthoSize() const", &Camera3D::GetOrthoSize);

    RegisterComponentType<SpriteRenderer>(engine, "SpriteRenderer")
        .method("void SetAnchor(const Vector2 &in)", &SpriteRenderer::SetAnchor)
        .method("const Vector2 &GetAnchor() const", &SpriteRenderer::GetAnchor)
        .method("void SetPivot(const Vector2 &in)", &SpriteRenderer::SetPivot)
        .method("const Vector2 &GetPivot() const", &SpriteRenderer::GetPivot)
        .method("void SetPipelineName(const string &in)", &SpriteRenderer::SetPipelineName)
        .method("void SetMaterialName(const string &in)", &SpriteRenderer::SetMaterialName);

    RegisterComponentType<ScriptComponent>(engine, "ScriptComponent")
        .method("void SetScriptPath(const string &in)", &ScriptComponent::SetScriptPath)
        .method("const string &GetScriptPath() const", &ScriptComponent::GetScriptPath)
        .method("bool Reload()", &ScriptComponent::Reload);

    // 型固有メソッド無しで登録するコンポーネント（GetComponentでの取得と共通メソッドのみ）
    RegisterComponentType<MeshFilter>(engine, "MeshFilter");
    RegisterComponentType<Animator>(engine, "Animator");
    RegisterComponentType<Text>(engine, "Text");
    RegisterComponentType<ComputeShaderProcessing>(engine, "ComputeShaderProcessing");
    RegisterComponentType<RigidBody2D>(engine, "RigidBody2D");
    RegisterComponentType<RigidBody3D>(engine, "RigidBody3D");
    RegisterComponentType<MeshRenderer>(engine, "MeshRenderer");
    RegisterComponentType<SkinnedMeshRenderer>(engine, "SkinnedMeshRenderer");
    RegisterComponentType<Camera2D>(engine, "Camera2D");
    RegisterComponentType<CameraRenderer>(engine, "CameraRenderer");
    RegisterComponentType<CameraController>(engine, "CameraController");
    RegisterComponentType<Light>(engine, "Light");
    RegisterComponentType<LightRenderer>(engine, "LightRenderer");
    RegisterComponentType<NormalWindowObject>(engine, "NormalWindowObject");
    RegisterComponentType<OverlayWindowObject>(engine, "OverlayWindowObject");
    RegisterComponentType<ScreenBufferObject>(engine, "ScreenBufferObject");
    RegisterComponentType<ShadowMapObject>(engine, "ShadowMapObject");

    // コライダー
    RegisterColliderType<BoxCollider>(engine, "BoxCollider");
    RegisterColliderType<SphereCollider>(engine, "SphereCollider");
    RegisterColliderType<CapsuleCollider>(engine, "CapsuleCollider");
    RegisterColliderType<MeshCollider>(engine, "MeshCollider");
    RegisterColliderType<RayCollider>(engine, "RayCollider");
    RegisterColliderType<Box2DCollider>(engine, "Box2DCollider");
    RegisterColliderType<Circle2DCollider>(engine, "Circle2DCollider");
    RegisterColliderType<Capsule2DCollider>(engine, "Capsule2DCollider");
    RegisterColliderType<Ray2DCollider>(engine, "Ray2DCollider");

    // ポストプロセスエフェクト
    RegisterComponentType<BloomEffect>(engine, "BloomEffect");
    RegisterComponentType<BoxFilterEffect>(engine, "BoxFilterEffect");
    RegisterComponentType<ChromaticAberrationEffect>(engine, "ChromaticAberrationEffect");
    RegisterComponentType<ColorAdjustEffect>(engine, "ColorAdjustEffect");
    RegisterComponentType<DissolveEffect>(engine, "DissolveEffect");
    RegisterComponentType<DitherEffect>(engine, "DitherEffect");
    RegisterComponentType<DotMatrixEffect>(engine, "DotMatrixEffect");
    RegisterComponentType<FXAAEffect>(engine, "FXAAEffect");
    RegisterComponentType<GaussianFilterEffect>(engine, "GaussianFilterEffect");
    RegisterComponentType<GrayscaleEffect>(engine, "GrayscaleEffect");
    RegisterComponentType<OutlineEffect>(engine, "OutlineEffect");
    RegisterComponentType<RadialBlurEffect>(engine, "RadialBlurEffect");
    RegisterComponentType<VignetteEffect>(engine, "VignetteEffect");
}

//==================================================
// GetComponent(?&out) の実装
//==================================================

/// @brief 型IDに対応するコンポーネントを取得して出力ハンドルへ格納する
/// @param ref 出力先（T@ 変数へのポインタ）
/// @param typeId 出力先のスクリプト型ID
/// @return コンポーネントが見つかった場合は true
bool GetComponentIntoHandle(EmptyObject &obj, void *ref, int typeId) {
    if (!ref || !(typeId & asTYPEID_OBJHANDLE)) return false;
    const int baseTypeId = typeId & ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST);
    auto it = gComponentTypeBindings.find(baseTypeId);
    if (it == gComponentTypeBindings.end()) return false;

    IObjectComponent *component = it->second.getOne(obj);
    // コンポーネント型は参照カウント無し(asOBJ_NOCOUNT)のためAddRefは不要
    *static_cast<void **>(ref) = component;
    return component != nullptr;
}

/// @brief 配列の要素型に対応する全コンポーネントを取得して出力ハンドル(array<T@>@)へ格納する
/// @param ref 出力先（array<T@>@ 変数へのポインタ）
/// @param typeId 出力先のスクリプト型ID
/// @return 取得に成功した場合は true（0個でも配列は生成される）
bool GetComponentsIntoArray(EmptyObject &obj, void *ref, int typeId) {
    if (!ref || !(typeId & asTYPEID_OBJHANDLE)) return false;

    asIScriptContext *context = asGetActiveContext();
    asIScriptEngine *engine = context ? context->GetEngine() : nullptr;
    if (!engine) return false;

    asITypeInfo *arrayType = engine->GetTypeInfoById(typeId);
    if (!arrayType || std::string_view(arrayType->GetName()) != "array") return false;

    const int subTypeId = arrayType->GetSubTypeId();
    if (!(subTypeId & asTYPEID_OBJHANDLE)) return false;
    const int baseTypeId = subTypeId & ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST);
    auto it = gComponentTypeBindings.find(baseTypeId);
    if (it == gComponentTypeBindings.end()) return false;

    auto components = it->second.getAll(obj);
    CScriptArray *array = CScriptArray::Create(arrayType, static_cast<asUINT>(components.size()));
    if (!array) return false;
    for (asUINT i = 0; i < components.size(); ++i) {
        void *handle = components[i];
        array->SetValue(i, &handle);
    }
    // Createで得た参照をそのまま出力ハンドルへ移譲する（解放はスクリプト側で行われる）
    *static_cast<CScriptArray **>(ref) = array;
    return true;
}

//==================================================
// オブジェクト・シーン・衝突情報
//==================================================

void RegisterObjectTypes(asIScriptEngine *engine) {
    // エンジン側が所有権を持つ型は参照カウント無しの参照型として登録する
    asbind20::ref_class<EmptyObject>(engine, "Object", asOBJ_NOCOUNT)
        .method("const string &GetName() const", &EmptyObject::GetName)
        .method("void SetName(const string &in)", &EmptyObject::SetName)
        .method("bool IsActive() const", &EmptyObject::IsActive)
        .method("void SetActive(bool)", &EmptyObject::SetActive)
        .method("Transform@ GetTransform()", [](EmptyObject &obj) -> Transform * { return obj.GetComponent<Transform>(); })
        .method("bool GetComponent(?&out)", [](EmptyObject &obj, void *ref, int typeId) -> bool {
            return GetComponentIntoHandle(obj, ref, typeId);
        })
        .method("bool GetComponents(?&out)", [](EmptyObject &obj, void *ref, int typeId) -> bool {
            return GetComponentsIntoArray(obj, ref, typeId);
        });

    asbind20::value_class<ScriptHitInfo>(engine, "HitInfo")
        .behaviours_by_traits()
        .property("Vector3 normal", &ScriptHitInfo::normal)
        .property("float penetration", &ScriptHitInfo::penetration)
        .property("Object@ selfObject", &ScriptHitInfo::selfObject)
        .property("Object@ otherObject", &ScriptHitInfo::otherObject);

    asbind20::ref_class<SceneContext>(engine, "Scene", asOBJ_NOCOUNT)
        .method("const string &GetName() const", &SceneContext::GetName)
        .method("Object@ GetObject(const string &in) const",
            [](const SceneContext &scene, const std::string &name) -> EmptyObject * { return scene.GetSceneObject(name); })
        .method("void SetNextSceneName(const string &in)", &SceneContext::SetNextSceneName)
        .method("bool ChangeToNextScene()", &SceneContext::ChangeToNextScene)
        .method("bool HasNextSceneName() const", &SceneContext::HasNextSceneName)
        .method("void ClearNextSceneName()", &SceneContext::ClearNextSceneName);

    // スクリプト側でコンポーネントの動作を定義するためのインターフェース
    // （ScriptComponentはこのインターフェースを実装したクラスを探して実行する）
    engine->RegisterInterface("ScriptComponentBehavior");
}

//==================================================
// グローバル関数
//==================================================

void RegisterGlobalFunctions(asIScriptEngine *engine) {
    asbind20::global(engine)
        // ログ
        .function("void Log(const string &in)", [](const std::string &text) { Log(text, LogSeverity::Info); })
        .function("void LogWarning(const string &in)", [](const std::string &text) { Log(text, LogSeverity::Warning); })
        .function("void LogError(const string &in)", [](const std::string &text) { Log(text, LogSeverity::Error); })
        // 時間
        .function("float GetDeltaTime()", &GetDeltaTime)
        .function("float GetGameSpeed()", &GetGameSpeed)
        .function("void SetGameSpeed(float)", &SetGameSpeed)
        // 入力コマンド
        .function("bool IsCommandTriggered(const string &in)", [](const std::string &action) -> bool {
            auto *command = gCurrentSceneContext ? gCurrentSceneContext->GetInputCommand() : nullptr;
            return command ? command->Evaluate(action).Triggered() : false;
        })
        .function("float GetCommandValue(const string &in)", [](const std::string &action) -> float {
            auto *command = gCurrentSceneContext ? gCurrentSceneContext->GetInputCommand() : nullptr;
            return command ? command->Evaluate(action).Value() : 0.0f;
        })
        // 音声
        .function("uint PlayAudio(const string &in, float volume = 1.0f)", [](const std::string &path, float volume) -> uint32_t {
            auto sound = AudioManager::GetSoundHandleFromAssetPath(path);
            if (sound == AudioManager::kInvalidSoundHandle) {
                sound = AudioManager::GetSoundHandleFromFileName(path);
            }
            if (sound == AudioManager::kInvalidSoundHandle) return AudioManager::kInvalidPlayHandle;
            return AudioManager::Play(sound, volume);
        })
        .function("bool StopAudio(uint)", [](uint32_t play) -> bool { return AudioManager::Stop(play); })
        .function("bool IsAudioPlaying(uint)", [](uint32_t play) -> bool { return AudioManager::IsPlaying(play); })
        // 実行コンテキスト
        .function("Object@ GetOwnerObject()", []() -> EmptyObject * {
            // ObjectContext::GetOwner はconstポインタを返すが、スクリプトからは自身のオブジェクトを操作できてよい
            return gCurrentObjectContext ? const_cast<EmptyObject *>(gCurrentObjectContext->GetOwner()) : nullptr;
        })
        .function("Transform@ GetTransform()", []() -> Transform * {
            return gCurrentObjectContext ? gCurrentObjectContext->GetComponent<Transform>() : nullptr;
        })
        .function("Scene@ GetScene()", []() -> SceneContext * { return gCurrentSceneContext; })
        .function("Object@ FindObject(const string &in)", [](const std::string &name) -> EmptyObject * {
            return gCurrentSceneContext ? gCurrentSceneContext->GetSceneObject(name) : nullptr;
        })
        // 自身のオブジェクトからのコンポーネント取得（obj.GetComponent(...)の省略形）
        .function("bool GetComponent(?&out)", [](void *ref, int typeId) -> bool {
            auto *owner = gCurrentObjectContext ? const_cast<EmptyObject *>(gCurrentObjectContext->GetOwner()) : nullptr;
            return owner ? GetComponentIntoHandle(*owner, ref, typeId) : false;
        })
        .function("bool GetComponents(?&out)", [](void *ref, int typeId) -> bool {
            auto *owner = gCurrentObjectContext ? const_cast<EmptyObject *>(gCurrentObjectContext->GetOwner()) : nullptr;
            return owner ? GetComponentsIntoArray(*owner, ref, typeId) : false;
        });
}

//==================================================
// as.predefined 生成
//==================================================

/// @brief 型名を取得する（テンプレート型は "array<T>" のようにサブタイプ付きで返す）
std::string GetScriptTypeName(const asITypeInfo *typeInfo) {
    std::string name = typeInfo->GetName();
    if (typeInfo->GetFlags() & asOBJ_TEMPLATE) {
        name += "<";
        const asUINT subTypeCount = typeInfo->GetSubTypeCount();
        for (asUINT i = 0; i < subTypeCount; ++i) {
            if (i > 0) name += ", ";
            const asITypeInfo *subType = typeInfo->GetSubType(i);
            name += subType ? subType->GetName() : "T";
        }
        name += ">";
    }
    return name;
}

/// @brief コンストラクタ/ファクトリのビヘイビア宣言を "TypeName(params)" 形式へ変換する
/// @details テンプレート型のファクトリ先頭に入る隠し引数(int&in)は取り除く
std::string MakeConstructorDeclaration(const std::string &typeName, const asIScriptFunction *function, bool isTemplate) {
    std::string decl = function->GetDeclaration(false, false, true);
    const auto parenPos = decl.find('(');
    if (parenPos == std::string::npos) return {};
    std::string params = decl.substr(parenPos);
    if (isTemplate) {
        // "(int&in)" または "(int&in, ..." の隠し引数を除去する
        if (params.rfind("(int&in)", 0) == 0) {
            params = "()" + params.substr(8);
        } else if (params.rfind("(int&in, ", 0) == 0) {
            params = "(" + params.substr(9);
        }
    }
    return typeName + params;
}

} // namespace

bool GenerateScriptPredefinedFile(asIScriptEngine *engine, const std::string &filePath) {
    if (!engine) return false;

    std::string out;
    out += "// このファイルはKashipanEngineが起動時に自動生成したものです。直接編集しないでください。\n";
    out += "// VSCodeのAngelScript Language Serverがコード補完に使用する型定義ファイルです。\n\n";

    // 列挙型
    const asUINT enumCount = engine->GetEnumCount();
    for (asUINT i = 0; i < enumCount; ++i) {
        const asITypeInfo *enumType = engine->GetEnumByIndex(i);
        if (!enumType) continue;
        out += "enum " + std::string(enumType->GetName()) + " {\n";
        const asUINT valueCount = enumType->GetEnumValueCount();
        for (asUINT v = 0; v < valueCount; ++v) {
            int value = 0;
            const char *valueName = enumType->GetEnumValueByIndex(v, &value);
            out += "\t" + std::string(valueName ? valueName : "") + " = " + std::to_string(value);
            out += (v + 1 < valueCount) ? ",\n" : "\n";
        }
        out += "}\n\n";
    }

    // funcdef
    const asUINT funcdefCount = engine->GetFuncdefCount();
    for (asUINT i = 0; i < funcdefCount; ++i) {
        const asITypeInfo *funcdefType = engine->GetFuncdefByIndex(i);
        const asIScriptFunction *signature = funcdefType ? funcdefType->GetFuncdefSignature() : nullptr;
        if (!signature) continue;
        out += "funcdef " + std::string(signature->GetDeclaration(false, false, true)) + ";\n";
    }
    if (funcdefCount > 0) out += "\n";

    // オブジェクト型（インターフェース・クラス）
    const asUINT typeCount = engine->GetObjectTypeCount();
    for (asUINT i = 0; i < typeCount; ++i) {
        const asITypeInfo *typeInfo = engine->GetObjectTypeByIndex(i);
        if (!typeInfo) continue;
        const asQWORD flags = typeInfo->GetFlags();
        const std::string typeName = GetScriptTypeName(typeInfo);

        // RegisterInterfaceで登録されたインターフェース
        if (flags & asOBJ_SCRIPT_OBJECT) {
            out += "interface " + typeName + " {\n}\n\n";
            continue;
        }

        out += "class " + typeName + " {\n";

        // コンストラクタ/ファクトリ
        const asUINT behaviourCount = typeInfo->GetBehaviourCount();
        for (asUINT b = 0; b < behaviourCount; ++b) {
            asEBehaviours behaviour = asBEHAVE_CONSTRUCT;
            const asIScriptFunction *function = typeInfo->GetBehaviourByIndex(b, &behaviour);
            if (!function) continue;
            if (behaviour != asBEHAVE_CONSTRUCT && behaviour != asBEHAVE_FACTORY) continue;
            const std::string decl = MakeConstructorDeclaration(typeInfo->GetName(), function, (flags & asOBJ_TEMPLATE) != 0);
            if (!decl.empty()) out += "\t" + decl + ";\n";
        }

        // プロパティ
        const asUINT propertyCount = typeInfo->GetPropertyCount();
        for (asUINT p = 0; p < propertyCount; ++p) {
            const char *decl = typeInfo->GetPropertyDeclaration(p);
            if (decl) out += "\t" + std::string(decl) + ";\n";
        }

        // メソッド
        const asUINT methodCount = typeInfo->GetMethodCount();
        for (asUINT m = 0; m < methodCount; ++m) {
            const asIScriptFunction *method = typeInfo->GetMethodByIndex(m);
            if (!method) continue;
            out += "\t" + std::string(method->GetDeclaration(false, false, true)) + ";\n";
        }

        out += "}\n\n";
    }

    // グローバルプロパティ
    const asUINT globalPropertyCount = engine->GetGlobalPropertyCount();
    for (asUINT i = 0; i < globalPropertyCount; ++i) {
        const char *name = nullptr;
        const char *nameSpace = nullptr;
        int typeId = 0;
        bool isConst = false;
        if (engine->GetGlobalPropertyByIndex(i, &name, &nameSpace, &typeId, &isConst) < 0 || !name) continue;
        const char *typeDecl = engine->GetTypeDeclaration(typeId);
        if (!typeDecl) continue;
        std::string decl = std::string(isConst ? "const " : "") + typeDecl + " " + name + ";";
        if (nameSpace && nameSpace[0] != '\0') {
            out += "namespace " + std::string(nameSpace) + " { " + decl + " }\n";
        } else {
            out += decl + "\n";
        }
    }
    if (globalPropertyCount > 0) out += "\n";

    // グローバル関数（名前空間ごとにまとめる）
    std::map<std::string, std::vector<std::string>> functionsByNamespace;
    const asUINT globalFunctionCount = engine->GetGlobalFunctionCount();
    for (asUINT i = 0; i < globalFunctionCount; ++i) {
        const asIScriptFunction *function = engine->GetGlobalFunctionByIndex(i);
        if (!function) continue;
        const char *nameSpace = function->GetNamespace();
        functionsByNamespace[nameSpace ? nameSpace : ""].push_back(
            std::string(function->GetDeclaration(false, false, true)) + ";");
    }
    for (const auto &[nameSpace, declarations] : functionsByNamespace) {
        if (nameSpace.empty()) {
            for (const auto &decl : declarations) out += decl + "\n";
            out += "\n";
        } else {
            out += "namespace " + nameSpace + " {\n";
            for (const auto &decl : declarations) out += "\t" + decl + "\n";
            out += "}\n\n";
        }
    }

    std::ofstream file(filePath, std::ios::binary);
    if (!file) return false;
    file.write(out.data(), static_cast<std::streamsize>(out.size()));
    return file.good();
}

void RegisterEngineScriptBindings(asIScriptEngine *engine) {
    if (!engine) return;
    RegisterMathTypes(engine);
    RegisterComponentTypes(engine);
    RegisterObjectTypes(engine);
    RegisterGlobalFunctions(engine);
}

ScriptExecutionScope::ScriptExecutionScope(ObjectContext *objectContext, SceneContext *sceneContext)
    : prevObjectContext_(gCurrentObjectContext), prevSceneContext_(gCurrentSceneContext) {
    gCurrentObjectContext = objectContext;
    gCurrentSceneContext = sceneContext;
}

ScriptExecutionScope::~ScriptExecutionScope() {
    gCurrentObjectContext = prevObjectContext_;
    gCurrentSceneContext = prevSceneContext_;
}

ObjectContext *ScriptExecutionScope::GetCurrentObjectContext() { return gCurrentObjectContext; }
SceneContext *ScriptExecutionScope::GetCurrentSceneContext() { return gCurrentSceneContext; }

} // namespace KashipanEngine
