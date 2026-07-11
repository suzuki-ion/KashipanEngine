#include "Scene/Components/Script/ScriptBindings.h"

#include <fstream>
#include <map>
#include <string_view>
#include <unordered_map>

#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <asbind20/asbind.hpp>

#include "Assets/AudioManager.h"
#include "ComponentSerialize/ComponentRegistry.h"
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
#include "Utilities/MathUtils/Easings.h"
#include "Utilities/MyAny.h"
#include "Utilities/RandomValue.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/ValueType.h"

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

/// @brief スクリプトの型ID→コンポーネント取得/生成処理のマップ（GetComponent/AddComponent(?&out)用）
struct ComponentTypeBinding {
    /// @brief ComponentRegistryへ登録されている型名（CreateObjectComponentByTypeへ渡す）
    std::string engineTypeName;
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
        name,
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

/// @brief Light::Type をスクリプト用の LightType 列挙型として登録する
void RegisterLightTypeEnum(asIScriptEngine *engine) {
    engine->RegisterEnum("LightType");
    engine->RegisterEnumValue("LightType", "Directional", static_cast<int>(Light::Type::Directional));
    engine->RegisterEnumValue("LightType", "Point", static_cast<int>(Light::Type::Point));
    engine->RegisterEnumValue("LightType", "Spot", static_cast<int>(Light::Type::Spot));
}

/// @brief Transformコンポーネントを登録する
/// @details Object::GetTransform() が Transform@ を返すため、Object/Scene（RegisterObjectTypes）より
///          先に登録しておく必要がある。gComponentTypeBindings のクリアもここで行う（最初に呼ばれるため）
void RegisterTransformType(asIScriptEngine *engine) {
    gComponentTypeBindings.clear();
    RegisterLightTypeEnum(engine);

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
}

void RegisterComponentTypes(asIScriptEngine *engine) {
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

    RegisterComponentType<MeshFilter>(engine, "MeshFilter")
        .method("void SetMeshHandle(uint)", [](MeshFilter &c, uint32_t handle) { c.SetMeshHandle(handle); })
        .method("uint GetMeshHandle() const", [](const MeshFilter &c) -> uint32_t { return c.GetMeshHandle(); })
        .method("bool HasMesh() const", &MeshFilter::HasMesh);

    RegisterComponentType<Animator>(engine, "Animator")
        .method("void SetAnimationName(const string &in)", &Animator::SetAnimationName)
        .method("const string &GetAnimationName() const", &Animator::GetAnimationName)
        .method("void SetPlayOnStart(bool)", &Animator::SetPlayOnStart)
        .method("bool GetPlayOnStart() const", &Animator::GetPlayOnStart);

    RegisterComponentType<Text>(engine, "Text")
        .method("void SetText(const string &in)", &Text::SetText)
        .method("const string &GetText() const", &Text::GetText)
        .method("void SetColor(const Vector4 &in)", &Text::SetColor)
        .method("const Vector4 &GetColor() const", &Text::GetColor);

    RegisterComponentType<ComputeShaderProcessing>(engine, "ComputeShaderProcessing")
        .method("void SetPipelineName(const string &in)", &ComputeShaderProcessing::SetPipelineName)
        .method("const string &GetPipelineName() const", &ComputeShaderProcessing::GetPipelineName)
        .method("void SetGroupCounts(uint, uint, uint)", &ComputeShaderProcessing::SetGroupCounts)
        .method("void GetGroupCounts(uint &out, uint &out, uint &out) const", &ComputeShaderProcessing::GetGroupCounts);

    RegisterComponentType<RigidBody2D>(engine, "RigidBody2D")
        .method("void SetVelocity(const Vector2 &in)", &RigidBody2D::SetVelocity)
        .method("const Vector2 &GetVelocity() const", &RigidBody2D::GetVelocity)
        .method("void SetMass(float)", &RigidBody2D::SetMass)
        .method("float GetMass() const", &RigidBody2D::GetMass)
        .method("void SetUseGravity(bool)", &RigidBody2D::SetUseGravity)
        .method("bool IsGravityEnabled() const", &RigidBody2D::IsGravityEnabled);

    RegisterComponentType<RigidBody3D>(engine, "RigidBody3D")
        .method("void SetBodyType(int)", [](RigidBody3D &rb, int type) { rb.SetBodyType(static_cast<reactphysics3d::BodyType>(type)); })
        .method("int GetBodyType() const", [](const RigidBody3D &rb) -> int { return static_cast<int>(rb.GetBodyType()); })
        .method("void SetMass(float)", &RigidBody3D::SetMass)
        .method("float GetMass() const", &RigidBody3D::GetMass)
        .method("void SetUseGravity(bool)", &RigidBody3D::SetUseGravity)
        .method("bool IsGravityEnabled() const", &RigidBody3D::IsGravityEnabled)
        .method("void SetInterpolate(bool)", &RigidBody3D::SetInterpolate)
        .method("bool IsInterpolateEnabled() const", &RigidBody3D::IsInterpolateEnabled)
        .method("void SyncFromTransform()", &RigidBody3D::SyncFromTransform);

    RegisterComponentType<MeshRenderer>(engine, "MeshRenderer")
        .method("void SetPipelineName(const string &in)", &MeshRenderer::SetPipelineName)
        .method("const string &GetPipelineName() const", &MeshRenderer::GetPipelineName)
        .method("void SetMaterialName(const string &in)", &MeshRenderer::SetMaterialName)
        .method("const string &GetMaterialName() const", &MeshRenderer::GetMaterialName)
        .method("Object@ GetTargetObject() const", &MeshRenderer::GetTargetObject)
        .method("void SetTargetObject(Object@)", [](MeshRenderer &c, EmptyObject *obj) { c.SetTargetObject(obj); });

    RegisterComponentType<SkinnedMeshRenderer>(engine, "SkinnedMeshRenderer")
        .method("void SetPipelineName(const string &in)", &SkinnedMeshRenderer::SetPipelineName)
        .method("const string &GetPipelineName() const", &SkinnedMeshRenderer::GetPipelineName)
        .method("void SetMaterialName(const string &in)", &SkinnedMeshRenderer::SetMaterialName)
        .method("const string &GetMaterialName() const", &SkinnedMeshRenderer::GetMaterialName)
        .method("void SetAnimationClipName(const string &in)", &SkinnedMeshRenderer::SetAnimationClipName)
        .method("const string &GetAnimationClipName() const", &SkinnedMeshRenderer::GetAnimationClipName)
        .method("void SetPlayOnStart(bool)", &SkinnedMeshRenderer::SetPlayOnStart)
        .method("bool GetPlayOnStart() const", &SkinnedMeshRenderer::GetPlayOnStart)
        .method("void SetLoop(bool)", &SkinnedMeshRenderer::SetLoop)
        .method("bool GetLoop() const", &SkinnedMeshRenderer::GetLoop)
        .method("void SetPlaybackSpeed(float)", &SkinnedMeshRenderer::SetPlaybackSpeed)
        .method("float GetPlaybackSpeed() const", &SkinnedMeshRenderer::GetPlaybackSpeed)
        .method("void Play()", &SkinnedMeshRenderer::Play)
        .method("void Stop()", &SkinnedMeshRenderer::Stop)
        .method("bool IsPlaying() const", &SkinnedMeshRenderer::IsPlaying)
        .method("void SetBlendShapeWeight(const string &in, float)", &SkinnedMeshRenderer::SetBlendShapeWeight)
        .method("float GetBlendShapeWeight(const string &in) const", &SkinnedMeshRenderer::GetBlendShapeWeight);

    RegisterComponentType<Camera2D>(engine, "Camera2D")
        .method("void SetSize(float, float)", &Camera2D::SetSize)
        .method("void SetNearClip(float)", &Camera2D::SetNearClip)
        .method("void SetFarClip(float)", &Camera2D::SetFarClip)
        .method("float GetWidth() const", &Camera2D::GetWidth)
        .method("float GetHeight() const", &Camera2D::GetHeight)
        .method("float GetNearClip() const", &Camera2D::GetNearClip)
        .method("float GetFarClip() const", &Camera2D::GetFarClip);

    RegisterComponentType<CameraRenderer>(engine, "CameraRenderer")
        .method("void SetPipelineName(const string &in)", &CameraRenderer::SetPipelineName)
        .method("const string &GetPipelineName() const", &CameraRenderer::GetPipelineName)
        .method("Vector3 GetWorldPosition() const", &CameraRenderer::GetWorldPosition)
        .method("const Matrix4x4 &GetViewProjectionMatrix() const", &CameraRenderer::GetViewProjectionMatrix)
        .method("float GetNearClip() const", &CameraRenderer::GetNearClip)
        .method("float GetFarClip() const", &CameraRenderer::GetFarClip);

    RegisterComponentType<CameraController>(engine, "CameraController")
        .method("bool IsControllable() const", &CameraController::IsControllable)
        .method("void AddFollowTarget(Object@)", [](CameraController &c, EmptyObject *obj) {
            if (obj) c.AddFollowTarget(obj->GetObjectID());
        })
        .method("void RemoveFollowTarget(uint)", [](CameraController &c, uint32_t index) { c.RemoveFollowTarget(index); })
        .method("void SetPositionOffset(const Vector3 &in)", &CameraController::SetPositionOffset)
        .method("const Vector3 &GetPositionOffset() const", &CameraController::GetPositionOffset)
        .method("void SetRotationOffset(const Vector3 &in)", &CameraController::SetRotationOffset)
        .method("const Vector3 &GetRotationOffset() const", &CameraController::GetRotationOffset)
        .method("void SetTargetFovY(float)", &CameraController::SetTargetFovY)
        .method("float GetTargetFovY() const", &CameraController::GetTargetFovY)
        .method("void SetMoveStrength(float)", [](CameraController &c, float v) {
            c.GetMoveStrength().usePerAxis = false;
            c.GetMoveStrength().all = v;
        })
        .method("float GetMoveStrength() const", [](const CameraController &c) -> float { return c.GetMoveStrength().all; })
        .method("void SetRotateStrength(float)", [](CameraController &c, float v) {
            c.GetRotateStrength().usePerAxis = false;
            c.GetRotateStrength().all = v;
        })
        .method("float GetRotateStrength() const", [](const CameraController &c) -> float { return c.GetRotateStrength().all; })
        .method("void SetFovLerpFactor(float)", &CameraController::SetFovLerpFactor)
        .method("float GetFovLerpFactor() const", &CameraController::GetFovLerpFactor);

    RegisterComponentType<Light>(engine, "Light")
        .method("void SetType(LightType)", &Light::SetType)
        .method("LightType GetType() const", &Light::GetType)
        .method("void SetColor(const Vector4 &in)", &Light::SetColor)
        .method("const Vector4 &GetColor() const", &Light::GetColor)
        .method("void SetIntensity(float)", &Light::SetIntensity)
        .method("float GetIntensity() const", &Light::GetIntensity)
        .method("void SetRadius(float)", &Light::SetRadius)
        .method("float GetRadius() const", &Light::GetRadius)
        .method("void SetDistance(float)", &Light::SetDistance)
        .method("float GetDistance() const", &Light::GetDistance)
        .method("void SetDecay(float)", &Light::SetDecay)
        .method("float GetDecay() const", &Light::GetDecay)
        .method("void SetInnerAngle(float)", &Light::SetInnerAngle)
        .method("float GetInnerAngle() const", &Light::GetInnerAngle)
        .method("void SetOuterAngle(float)", &Light::SetOuterAngle)
        .method("float GetOuterAngle() const", &Light::GetOuterAngle);

    RegisterComponentType<LightRenderer>(engine, "LightRenderer")
        .method("void SetPipelineName(const string &in)", &LightRenderer::SetPipelineName)
        .method("const string &GetPipelineName() const", &LightRenderer::GetPipelineName)
        .method("Light@ GetLight() const", &LightRenderer::GetLight)
        .method("LightType GetLightType() const", &LightRenderer::GetLightType)
        .method("Vector3 GetWorldPosition() const", &LightRenderer::GetWorldPosition)
        .method("Vector3 GetWorldDirection() const", &LightRenderer::GetWorldDirection);

    RegisterComponentType<NormalWindowObject>(engine, "NormalWindowObject")
        .method("void SetTitle(const string &in)", &NormalWindowObject::SetTitle)
        .method("void SetSize(uint, uint)", &NormalWindowObject::SetSize);

    RegisterComponentType<OverlayWindowObject>(engine, "OverlayWindowObject")
        .method("void SetTitle(const string &in)", &OverlayWindowObject::SetTitle)
        .method("void SetSize(uint, uint)", &OverlayWindowObject::SetSize);

    RegisterComponentType<ScreenBufferObject>(engine, "ScreenBufferObject")
        .method("void SetName(const string &in)", &ScreenBufferObject::SetName)
        .method("const string &GetName() const", &ScreenBufferObject::GetName)
        .method("void SetSize(uint, uint)", &ScreenBufferObject::SetSize);

    RegisterComponentType<ShadowMapObject>(engine, "ShadowMapObject")
        .method("void SetName(const string &in)", &ShadowMapObject::SetName)
        .method("const string &GetName() const", &ShadowMapObject::GetName)
        .method("void SetSize(uint, uint)", &ShadowMapObject::SetSize);

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

    //==================================================
    // ポストプロセスエフェクト
    //==================================================
    // それぞれ内部の Params 構造体を直接は公開せず、フィールドごとの Get/Set をラムダで提供する

    RegisterComponentType<BloomEffect>(engine, "BloomEffect")
        .method("float GetThreshold() const", [](const BloomEffect &e) { return e.GetParams().threshold; })
        .method("void SetThreshold(float)", [](BloomEffect &e, float v) { auto p = e.GetParams(); p.threshold = v; e.SetParams(p); })
        .method("float GetSoftKnee() const", [](const BloomEffect &e) { return e.GetParams().softKnee; })
        .method("void SetSoftKnee(float)", [](BloomEffect &e, float v) { auto p = e.GetParams(); p.softKnee = v; e.SetParams(p); })
        .method("float GetIntensity() const", [](const BloomEffect &e) { return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](BloomEffect &e, float v) { auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("float GetBlurRadius() const", [](const BloomEffect &e) { return e.GetParams().blurRadius; })
        .method("void SetBlurRadius(float)", [](BloomEffect &e, float v) { auto p = e.GetParams(); p.blurRadius = v; e.SetParams(p); })
        .method("uint GetIterations() const", [](const BloomEffect &e) -> uint32_t { return e.GetParams().iterations; })
        .method("void SetIterations(uint)", [](BloomEffect &e, uint32_t v) { auto p = e.GetParams(); p.iterations = v; e.SetParams(p); });

    RegisterComponentType<BoxFilterEffect>(engine, "BoxFilterEffect")
        .method("float GetIntensity() const", [](const BoxFilterEffect &e) { return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](BoxFilterEffect &e, float v) { auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("void SetHalfSize(int, int)", [](BoxFilterEffect &e, int x, int y) {
            auto p = e.GetParams(); p.halfSize[0] = x; p.halfSize[1] = y; e.SetParams(p);
        })
        .method("int GetHalfSizeX() const", [](const BoxFilterEffect &e) { return e.GetParams().halfSize[0]; })
        .method("int GetHalfSizeY() const", [](const BoxFilterEffect &e) { return e.GetParams().halfSize[1]; });

    RegisterComponentType<ChromaticAberrationEffect>(engine, "ChromaticAberrationEffect")
        .method("void SetDirection(const Vector2 &in)", [](ChromaticAberrationEffect &e, const Vector2 &dir) {
            auto p = e.GetParams(); p.directionX = dir.x; p.directionY = dir.y; e.SetParams(p);
        })
        .method("Vector2 GetDirection() const", [](const ChromaticAberrationEffect &e) -> Vector2 {
            const auto &p = e.GetParams(); return Vector2(p.directionX, p.directionY);
        })
        .method("float GetStrength() const", [](const ChromaticAberrationEffect &e) { return e.GetParams().strength; })
        .method("void SetStrength(float)", [](ChromaticAberrationEffect &e, float v) { auto p = e.GetParams(); p.strength = v; e.SetParams(p); });

    RegisterComponentType<ColorAdjustEffect>(engine, "ColorAdjustEffect")
        .method("float GetBrightness() const", [](const ColorAdjustEffect &e) { return e.GetParams().brightness; })
        .method("void SetBrightness(float)", [](ColorAdjustEffect &e, float v) { auto p = e.GetParams(); p.brightness = v; e.SetParams(p); })
        .method("float GetContrast() const", [](const ColorAdjustEffect &e) { return e.GetParams().contrast; })
        .method("void SetContrast(float)", [](ColorAdjustEffect &e, float v) { auto p = e.GetParams(); p.contrast = v; e.SetParams(p); })
        .method("float GetSaturation() const", [](const ColorAdjustEffect &e) { return e.GetParams().saturation; })
        .method("void SetSaturation(float)", [](ColorAdjustEffect &e, float v) { auto p = e.GetParams(); p.saturation = v; e.SetParams(p); })
        .method("float GetTemperature() const", [](const ColorAdjustEffect &e) { return e.GetParams().temperature; })
        .method("void SetTemperature(float)", [](ColorAdjustEffect &e, float v) { auto p = e.GetParams(); p.temperature = v; e.SetParams(p); })
        .method("Vector3 GetColorBalance() const", [](const ColorAdjustEffect &e) -> Vector3 {
            const auto &p = e.GetParams(); return Vector3(p.colorBalance[0], p.colorBalance[1], p.colorBalance[2]);
        })
        .method("void SetColorBalance(const Vector3 &in)", [](ColorAdjustEffect &e, const Vector3 &v) {
            auto p = e.GetParams(); p.colorBalance[0] = v.x; p.colorBalance[1] = v.y; p.colorBalance[2] = v.z; e.SetParams(p);
        });

    RegisterComponentType<DissolveEffect>(engine, "DissolveEffect")
        .method("float GetMaskThreshold() const", [](const DissolveEffect &e) { return e.GetParams().maskThreshold; })
        .method("void SetMaskThreshold(float)", [](DissolveEffect &e, float v) { auto p = e.GetParams(); p.maskThreshold = v; e.SetParams(p); })
        .method("float GetEdgeThickness() const", [](const DissolveEffect &e) { return e.GetParams().edgeThickness; })
        .method("void SetEdgeThickness(float)", [](DissolveEffect &e, float v) { auto p = e.GetParams(); p.edgeThickness = v; e.SetParams(p); })
        .method("string GetBaseTexturePath() const", [](const DissolveEffect &e) -> std::string {
            return TextureManager::GetTextureAssetPath(e.GetParams().baseTexture);
        })
        .method("void SetBaseTexturePath(const string &in)", [](DissolveEffect &e, const std::string &path) {
            auto p = e.GetParams();
            p.baseTexture = path.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(path);
            e.SetParams(p);
        })
        .method("string GetMaskTexturePath() const", [](const DissolveEffect &e) -> std::string {
            return TextureManager::GetTextureAssetPath(e.GetParams().maskTexture);
        })
        .method("void SetMaskTexturePath(const string &in)", [](DissolveEffect &e, const std::string &path) {
            auto p = e.GetParams();
            p.maskTexture = path.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(path);
            e.SetParams(p);
        })
        .method("Vector4 GetBaseTextureColor() const", [](const DissolveEffect &e) -> Vector4 {
            const auto &c = e.GetParams().baseTextureColor; return Vector4(c[0], c[1], c[2], c[3]);
        })
        .method("void SetBaseTextureColor(const Vector4 &in)", [](DissolveEffect &e, const Vector4 &c) {
            auto p = e.GetParams();
            p.baseTextureColor[0] = c.x; p.baseTextureColor[1] = c.y; p.baseTextureColor[2] = c.z; p.baseTextureColor[3] = c.w;
            e.SetParams(p);
        })
        .method("Vector4 GetEdgeColor() const", [](const DissolveEffect &e) -> Vector4 {
            const auto &c = e.GetParams().edgeColor; return Vector4(c[0], c[1], c[2], c[3]);
        })
        .method("void SetEdgeColor(const Vector4 &in)", [](DissolveEffect &e, const Vector4 &c) {
            auto p = e.GetParams();
            p.edgeColor[0] = c.x; p.edgeColor[1] = c.y; p.edgeColor[2] = c.z; p.edgeColor[3] = c.w;
            e.SetParams(p);
        });

    RegisterComponentType<DitherEffect>(engine, "DitherEffect")
        .method("float GetIntensity() const", [](const DitherEffect &e) { return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](DitherEffect &e, float v) { auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("bool IsColorDither() const", [](const DitherEffect &e) { return e.GetParams().color; })
        .method("void SetColorDither(bool)", [](DitherEffect &e, bool v) { auto p = e.GetParams(); p.color = v; e.SetParams(p); });

    RegisterComponentType<DotMatrixEffect>(engine, "DotMatrixEffect")
        .method("float GetDotSpacing() const", [](const DotMatrixEffect &e) { return e.GetParams().dotSpacing; })
        .method("void SetDotSpacing(float)", [](DotMatrixEffect &e, float v) { auto p = e.GetParams(); p.dotSpacing = v; e.SetParams(p); })
        .method("float GetDotRadius() const", [](const DotMatrixEffect &e) { return e.GetParams().dotRadius; })
        .method("void SetDotRadius(float)", [](DotMatrixEffect &e, float v) { auto p = e.GetParams(); p.dotRadius = v; e.SetParams(p); })
        .method("float GetThreshold() const", [](const DotMatrixEffect &e) { return e.GetParams().threshold; })
        .method("void SetThreshold(float)", [](DotMatrixEffect &e, float v) { auto p = e.GetParams(); p.threshold = v; e.SetParams(p); })
        .method("float GetIntensity() const", [](const DotMatrixEffect &e) { return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](DotMatrixEffect &e, float v) { auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("bool IsMonochrome() const", [](const DotMatrixEffect &e) { return e.GetParams().monochrome; })
        .method("void SetMonochrome(bool)", [](DotMatrixEffect &e, bool v) { auto p = e.GetParams(); p.monochrome = v; e.SetParams(p); });

    RegisterComponentType<FXAAEffect>(engine, "FXAAEffect")
        .method("float GetThreshold() const", [](const FXAAEffect &e) { return e.GetParams().threshold; })
        .method("void SetThreshold(float)", [](FXAAEffect &e, float v) { auto p = e.GetParams(); p.threshold = v; e.SetParams(p); })
        .method("float GetThresholdMin() const", [](const FXAAEffect &e) { return e.GetParams().thresholdMin; })
        .method("void SetThresholdMin(float)", [](FXAAEffect &e, float v) { auto p = e.GetParams(); p.thresholdMin = v; e.SetParams(p); })
        .method("float GetStrength() const", [](const FXAAEffect &e) { return e.GetParams().strength; })
        .method("void SetStrength(float)", [](FXAAEffect &e, float v) { auto p = e.GetParams(); p.strength = v; e.SetParams(p); });

    RegisterComponentType<GaussianFilterEffect>(engine, "GaussianFilterEffect")
        .method("int GetRadius() const", [](const GaussianFilterEffect &e) { return e.GetParams().radius; })
        .method("void SetRadius(int)", [](GaussianFilterEffect &e, int v) { auto p = e.GetParams(); p.radius = v; e.SetParams(p); })
        .method("float GetSigma() const", [](const GaussianFilterEffect &e) { return e.GetParams().sigma; })
        .method("void SetSigma(float)", [](GaussianFilterEffect &e, float v) { auto p = e.GetParams(); p.sigma = v; e.SetParams(p); });

    RegisterComponentType<GrayscaleEffect>(engine, "GrayscaleEffect")
        .method("float GetIntensity() const", [](const GrayscaleEffect &e) { return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](GrayscaleEffect &e, float v) { auto p = e.GetParams(); p.intensity = v; e.SetParams(p); });

    RegisterComponentType<OutlineEffect>(engine, "OutlineEffect")
        .method("float GetThreshold() const", [](const OutlineEffect &e) { return e.GetParams().threshold; })
        .method("void SetThreshold(float)", [](OutlineEffect &e, float v) { auto p = e.GetParams(); p.threshold = v; e.SetParams(p); })
        .method("float GetThickness() const", [](const OutlineEffect &e) { return e.GetParams().thickness; })
        .method("void SetThickness(float)", [](OutlineEffect &e, float v) { auto p = e.GetParams(); p.thickness = v; e.SetParams(p); })
        .method("Vector4 GetColor() const", [](const OutlineEffect &e) -> Vector4 {
            const auto &c = e.GetParams().color; return Vector4(c[0], c[1], c[2], c[3]);
        })
        .method("void SetColor(const Vector4 &in)", [](OutlineEffect &e, const Vector4 &c) {
            auto p = e.GetParams();
            p.color[0] = c.x; p.color[1] = c.y; p.color[2] = c.z; p.color[3] = c.w;
            e.SetParams(p);
        })
        .method("float GetCameraNear() const", [](const OutlineEffect &e) { return e.GetParams().cameraNear; })
        .method("void SetCameraNear(float)", [](OutlineEffect &e, float v) { auto p = e.GetParams(); p.cameraNear = v; e.SetParams(p); })
        .method("float GetCameraFar() const", [](const OutlineEffect &e) { return e.GetParams().cameraFar; })
        .method("void SetCameraFar(float)", [](OutlineEffect &e, float v) { auto p = e.GetParams(); p.cameraFar = v; e.SetParams(p); });

    RegisterComponentType<RadialBlurEffect>(engine, "RadialBlurEffect")
        .method("float GetIntensity() const", [](const RadialBlurEffect &e) { return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](RadialBlurEffect &e, float v) { auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("int GetSampleCount() const", [](const RadialBlurEffect &e) { return e.GetParams().sampleCount; })
        .method("void SetSampleCount(int)", [](RadialBlurEffect &e, int v) { auto p = e.GetParams(); p.sampleCount = v; e.SetParams(p); })
        .method("Vector2 GetCenter() const", [](const RadialBlurEffect &e) -> Vector2 {
            const auto &c = e.GetParams().radialCenter; return Vector2(c[0], c[1]);
        })
        .method("void SetCenter(const Vector2 &in)", [](RadialBlurEffect &e, const Vector2 &c) {
            auto p = e.GetParams(); p.radialCenter[0] = c.x; p.radialCenter[1] = c.y; e.SetParams(p);
        })
        .method("float GetStartRadius() const", [](const RadialBlurEffect &e) { return e.GetParams().startRadius; })
        .method("void SetStartRadius(float)", [](RadialBlurEffect &e, float v) { auto p = e.GetParams(); p.startRadius = v; e.SetParams(p); });

    RegisterComponentType<VignetteEffect>(engine, "VignetteEffect")
        .method("Vector2 GetCenter() const", [](const VignetteEffect &e) -> Vector2 {
            const auto &c = e.GetParams().center; return Vector2(c[0], c[1]);
        })
        .method("void SetCenter(const Vector2 &in)", [](VignetteEffect &e, const Vector2 &c) {
            auto p = e.GetParams(); p.center[0] = c.x; p.center[1] = c.y; e.SetParams(p);
        })
        .method("Vector4 GetColor() const", [](const VignetteEffect &e) -> Vector4 { return e.GetParams().color; })
        .method("void SetColor(const Vector4 &in)", [](VignetteEffect &e, const Vector4 &c) {
            auto p = e.GetParams(); p.color = c; e.SetParams(p);
        })
        .method("float GetIntensity() const", [](const VignetteEffect &e) { return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](VignetteEffect &e, float v) { auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("float GetInnerRadius() const", [](const VignetteEffect &e) { return e.GetParams().innerRadius; })
        .method("void SetInnerRadius(float)", [](VignetteEffect &e, float v) { auto p = e.GetParams(); p.innerRadius = v; e.SetParams(p); })
        .method("float GetSmoothness() const", [](const VignetteEffect &e) { return e.GetParams().smoothness; })
        .method("void SetSmoothness(float)", [](VignetteEffect &e, float v) { auto p = e.GetParams(); p.smoothness = v; e.SetParams(p); });
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

/// @brief 型IDに対応するコンポーネントを新規生成してオブジェクトへ追加し、出力ハンドルへ格納する
/// @param ref 出力先（T@ 変数へのポインタ）。渡した変数の型からどのコンポーネントを追加するか決まる
/// @param typeId 出力先のスクリプト型ID
/// @return 追加できた場合は true（型が未登録、または追加数上限などで失敗した場合は false）
bool AddComponentIntoHandle(EmptyObject &obj, void *ref, int typeId) {
    if (!ref || !(typeId & asTYPEID_OBJHANDLE)) return false;
    const int baseTypeId = typeId & ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST);
    auto it = gComponentTypeBindings.find(baseTypeId);
    if (it == gComponentTypeBindings.end()) {
        *static_cast<void **>(ref) = nullptr;
        return false;
    }

    auto newComponent = CreateObjectComponentByType(it->second.engineTypeName);
    IObjectComponent *added = newComponent ? obj.AddComponent(std::move(newComponent)) : nullptr;
    // コンポーネント型は参照カウント無し(asOBJ_NOCOUNT)のためAddRefは不要
    *static_cast<void **>(ref) = added;
    return added != nullptr;
}

/// @brief ?&in で渡されたコンポーネントハンドルをオブジェクトから削除する
/// @param ref 削除したいコンポーネントハンドルへのポインタ
/// @param typeId 渡されたハンドルのスクリプト型ID
/// @return 削除できた場合は true
bool RemoveComponentFromHandle(EmptyObject &obj, void *ref, int typeId) {
    if (!ref || !(typeId & asTYPEID_OBJHANDLE)) return false;
    const int baseTypeId = typeId & ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST);
    if (gComponentTypeBindings.find(baseTypeId) == gComponentTypeBindings.end()) return false;

    void *componentPtr = *static_cast<void **>(ref);
    if (!componentPtr) return false;
    return obj.RemoveComponent(static_cast<IObjectComponent *>(componentPtr));
}

/// @brief EmptyObjectのポインタ配列から array<Object@>@ を構築する（Scene::GetObjects用）
CScriptArray *MakeObjectArray(const std::vector<EmptyObject *> &objects) {
    asIScriptContext *context = asGetActiveContext();
    asIScriptEngine *engine = context ? context->GetEngine() : nullptr;
    if (!engine) return nullptr;

    asITypeInfo *arrayType = engine->GetTypeInfoByDecl("array<Object@>");
    if (!arrayType) return nullptr;

    CScriptArray *array = CScriptArray::Create(arrayType, static_cast<asUINT>(objects.size()));
    if (!array) return nullptr;
    for (asUINT i = 0; i < objects.size(); ++i) {
        void *handle = objects[i];
        array->SetValue(i, &handle);
    }
    return array;
}

//==================================================
// シーン変数（?&in/?&out と MyAny の相互変換）
//==================================================

/// @brief シーン変数の読み書きで対応する型のタイプID（RegisterObjectTypesで一度だけ設定される）
/// @details ラムダをネイティブ呼び出し規約で登録するには非キャプチャである必要があるため、
///          キャプチャ変数の代わりにこのグローバル変数を経由して型IDを参照する
struct SceneVariableTypeIds {
    int stringTypeId = 0;
    int vector2TypeId = 0;
    int vector3TypeId = 0;
    int vector4TypeId = 0;
    int quaternionTypeId = 0;
};
SceneVariableTypeIds gSceneVariableTypeIds;

/// @brief ?&in で渡された値を型に応じてシーン変数へ書き込む（上書き）
/// @return 対応している型であれば true、対応外の型の場合は false
bool SetSceneVariableFromGeneric(SceneContext &scene, const std::string &key, void *ref, int typeId) {
    if (!ref) return false;
    if (typeId == asTYPEID_BOOL) { scene.AddSceneVariable<bool>(key, *static_cast<bool *>(ref)); return true; }
    if (typeId == asTYPEID_INT32) { scene.AddSceneVariable<int32_t>(key, *static_cast<int32_t *>(ref)); return true; }
    if (typeId == asTYPEID_UINT32) { scene.AddSceneVariable<uint32_t>(key, *static_cast<uint32_t *>(ref)); return true; }
    if (typeId == asTYPEID_FLOAT) { scene.AddSceneVariable<float>(key, *static_cast<float *>(ref)); return true; }
    if (typeId == asTYPEID_DOUBLE) { scene.AddSceneVariable<double>(key, *static_cast<double *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.stringTypeId) { scene.AddSceneVariable<std::string>(key, *static_cast<std::string *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.vector2TypeId) { scene.AddSceneVariable<Vector2>(key, *static_cast<Vector2 *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.vector3TypeId) { scene.AddSceneVariable<Vector3>(key, *static_cast<Vector3 *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.vector4TypeId) { scene.AddSceneVariable<Vector4>(key, *static_cast<Vector4 *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.quaternionTypeId) { scene.AddSceneVariable<Quaternion>(key, *static_cast<Quaternion *>(ref)); return true; }
    return false;
}

/// @brief ?&in で渡された値を型に応じてグローバルシーン変数へ書き込む（上書き）
/// @details AddGlobalSceneVariable は既存キーがあっても上書きしない仕様のため、先に削除してから追加する
/// @return 対応している型であれば true、対応外の型の場合は false
bool SetGlobalSceneVariableFromGeneric(SceneContext &scene, const std::string &key, void *ref, int typeId) {
    if (!ref) return false;
    scene.RemoveGlobalSceneVariable(key);
    if (typeId == asTYPEID_BOOL) { scene.AddGlobalSceneVariable<bool>(key, *static_cast<bool *>(ref)); return true; }
    if (typeId == asTYPEID_INT32) { scene.AddGlobalSceneVariable<int32_t>(key, *static_cast<int32_t *>(ref)); return true; }
    if (typeId == asTYPEID_UINT32) { scene.AddGlobalSceneVariable<uint32_t>(key, *static_cast<uint32_t *>(ref)); return true; }
    if (typeId == asTYPEID_FLOAT) { scene.AddGlobalSceneVariable<float>(key, *static_cast<float *>(ref)); return true; }
    if (typeId == asTYPEID_DOUBLE) { scene.AddGlobalSceneVariable<double>(key, *static_cast<double *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.stringTypeId) { scene.AddGlobalSceneVariable<std::string>(key, *static_cast<std::string *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.vector2TypeId) { scene.AddGlobalSceneVariable<Vector2>(key, *static_cast<Vector2 *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.vector3TypeId) { scene.AddGlobalSceneVariable<Vector3>(key, *static_cast<Vector3 *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.vector4TypeId) { scene.AddGlobalSceneVariable<Vector4>(key, *static_cast<Vector4 *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.quaternionTypeId) { scene.AddGlobalSceneVariable<Quaternion>(key, *static_cast<Quaternion *>(ref)); return true; }
    return false;
}

/// @brief MyAny（シーン変数の実体）の値を型に応じて ?&out の変数へ書き戻す
/// @return 変数が存在し、かつ渡された型と一致していれば true
bool WriteSceneVariableToGeneric(MyAny *value, void *ref, int typeId) {
    if (!value || !ref || value->IsEmpty()) return false;
    if (typeId == asTYPEID_BOOL && value->IsType<bool>()) { *static_cast<bool *>(ref) = value->AnyCast<bool>(); return true; }
    if (typeId == asTYPEID_INT32 && value->IsType<int32_t>()) { *static_cast<int32_t *>(ref) = value->AnyCast<int32_t>(); return true; }
    if (typeId == asTYPEID_UINT32 && value->IsType<uint32_t>()) { *static_cast<uint32_t *>(ref) = value->AnyCast<uint32_t>(); return true; }
    if (typeId == asTYPEID_FLOAT && value->IsType<float>()) { *static_cast<float *>(ref) = value->AnyCast<float>(); return true; }
    if (typeId == asTYPEID_DOUBLE && value->IsType<double>()) { *static_cast<double *>(ref) = value->AnyCast<double>(); return true; }
    if (typeId == gSceneVariableTypeIds.stringTypeId && value->IsType<std::string>()) { *static_cast<std::string *>(ref) = value->AnyCast<std::string>(); return true; }
    if (typeId == gSceneVariableTypeIds.vector2TypeId && value->IsType<Vector2>()) { *static_cast<Vector2 *>(ref) = value->AnyCast<Vector2>(); return true; }
    if (typeId == gSceneVariableTypeIds.vector3TypeId && value->IsType<Vector3>()) { *static_cast<Vector3 *>(ref) = value->AnyCast<Vector3>(); return true; }
    if (typeId == gSceneVariableTypeIds.vector4TypeId && value->IsType<Vector4>()) { *static_cast<Vector4 *>(ref) = value->AnyCast<Vector4>(); return true; }
    if (typeId == gSceneVariableTypeIds.quaternionTypeId && value->IsType<Quaternion>()) { *static_cast<Quaternion *>(ref) = value->AnyCast<Quaternion>(); return true; }
    return false;
}

//==================================================
// オブジェクト・シーン・衝突情報
//==================================================

/// @brief Object/Scene/HitInfo とそれらのメソッドを登録する
/// @details Object::GetTransform() が Transform@ を返すため、RegisterTransformType の後に呼ぶこと。
///          MeshRenderer 等ここより後に登録されるコンポーネントは Object@/Scene@ を
///          パラメータ/戻り値として自由に参照できる（型は既に登録済みのため）。
void RegisterObjectTypes(asIScriptEngine *engine) {
    // シーン変数の ?&in/?&out 変換で使うタイプIDをキャッシュする（string/Vector2等はここまでに登録済み）
    gSceneVariableTypeIds.stringTypeId = engine->GetTypeIdByDecl("string");
    gSceneVariableTypeIds.vector2TypeId = engine->GetTypeIdByDecl("Vector2");
    gSceneVariableTypeIds.vector3TypeId = engine->GetTypeIdByDecl("Vector3");
    gSceneVariableTypeIds.vector4TypeId = engine->GetTypeIdByDecl("Vector4");
    gSceneVariableTypeIds.quaternionTypeId = engine->GetTypeIdByDecl("Quaternion");

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
        })
        .method("bool AddComponent(?&out)", [](EmptyObject &obj, void *ref, int typeId) -> bool {
            return AddComponentIntoHandle(obj, ref, typeId);
        })
        .method("bool RemoveComponent(?&in)", [](EmptyObject &obj, void *ref, int typeId) -> bool {
            return RemoveComponentFromHandle(obj, ref, typeId);
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
        .method("array<Object@>@ GetObjects(const string &in) const", [](const SceneContext &scene, const std::string &name) -> CScriptArray * {
            return MakeObjectArray(scene.GetSceneObjects(name));
        })
        .method("void SetNextSceneName(const string &in)", &SceneContext::SetNextSceneName)
        .method("bool ChangeToNextScene()", &SceneContext::ChangeToNextScene)
        .method("bool HasNextSceneName() const", &SceneContext::HasNextSceneName)
        .method("void ClearNextSceneName()", &SceneContext::ClearNextSceneName)
        // オブジェクトの生成・複製・削除
        .method("Object@ CreateObject(const string &in name = \"\")", [](SceneContext &scene, const std::string &name) -> EmptyObject * {
            return scene.CreateEmptyObject(name);
        })
        .method("Object@ CloneObject(Object@ source, const string &in name = \"\")", [](SceneContext &scene, EmptyObject *source, const std::string &name) -> EmptyObject * {
            return scene.CloneObject(source, name);
        })
        .method("bool DeleteObject(Object@ obj)", &SceneContext::DeleteObject)
        // シーン変数（このシーンが読み込まれている間だけ有効。スクリプト間で値を受け渡すのに使う）
        .method("bool SetVariable(const string &in, ?&in)", [](SceneContext &scene, const std::string &key, void *ref, int typeId) -> bool {
            return SetSceneVariableFromGeneric(scene, key, ref, typeId);
        })
        .method("bool GetVariable(const string &in, ?&out)", [](SceneContext &scene, const std::string &key, void *ref, int typeId) -> bool {
            return WriteSceneVariableToGeneric(scene.GetSceneVariable(key), ref, typeId);
        })
        .method("bool HasVariable(const string &in)", [](SceneContext &scene, const std::string &key) -> bool {
            return scene.GetSceneVariable(key) != nullptr;
        })
        .method("bool RemoveVariable(const string &in)", &SceneContext::RemoveSceneVariable)
        // グローバルシーン変数（シーンを跨いでも値が残る。SceneManagerが保持する）
        .method("bool SetGlobalVariable(const string &in, ?&in)", [](SceneContext &scene, const std::string &key, void *ref, int typeId) -> bool {
            return SetGlobalSceneVariableFromGeneric(scene, key, ref, typeId);
        })
        .method("bool GetGlobalVariable(const string &in, ?&out)", [](SceneContext &scene, const std::string &key, void *ref, int typeId) -> bool {
            return WriteSceneVariableToGeneric(scene.GetGlobalSceneVariable(key), ref, typeId);
        })
        .method("bool HasGlobalVariable(const string &in)", [](SceneContext &scene, const std::string &key) -> bool {
            return scene.GetGlobalSceneVariable(key) != nullptr;
        })
        .method("bool RemoveGlobalVariable(const string &in)", &SceneContext::RemoveGlobalSceneVariable);

    // スクリプト側でコンポーネントの動作を定義するためのインターフェース
    // （ScriptComponentはこのインターフェースを実装したクラスを探して実行する）
    engine->RegisterInterface("ScriptComponentBehavior");
}

//==================================================
// イージング関数（Utilities/MathUtils/Easings.h）
//==================================================

/// @brief EaseType 列挙型と Easing:: 名前空間のユーティリティ関数を登録する
void RegisterEasingBindings(asIScriptEngine *engine) {
    engine->RegisterEnum("EaseType");
    static const std::pair<const char *, EaseType> kEaseTypeValues[] = {
        { "Linear", EaseType::Linear },
        { "EaseInQuad", EaseType::EaseInQuad }, { "EaseOutQuad", EaseType::EaseOutQuad },
        { "EaseInOutQuad", EaseType::EaseInOutQuad }, { "EaseOutInQuad", EaseType::EaseOutInQuad },
        { "EaseInCubic", EaseType::EaseInCubic }, { "EaseOutCubic", EaseType::EaseOutCubic },
        { "EaseInOutCubic", EaseType::EaseInOutCubic }, { "EaseOutInCubic", EaseType::EaseOutInCubic },
        { "EaseInQuart", EaseType::EaseInQuart }, { "EaseOutQuart", EaseType::EaseOutQuart },
        { "EaseInOutQuart", EaseType::EaseInOutQuart }, { "EaseOutInQuart", EaseType::EaseOutInQuart },
        { "EaseInQuint", EaseType::EaseInQuint }, { "EaseOutQuint", EaseType::EaseOutQuint },
        { "EaseInOutQuint", EaseType::EaseInOutQuint }, { "EaseOutInQuint", EaseType::EaseOutInQuint },
        { "EaseInSine", EaseType::EaseInSine }, { "EaseOutSine", EaseType::EaseOutSine },
        { "EaseInOutSine", EaseType::EaseInOutSine }, { "EaseOutInSine", EaseType::EaseOutInSine },
        { "EaseInExpo", EaseType::EaseInExpo }, { "EaseOutExpo", EaseType::EaseOutExpo },
        { "EaseInOutExpo", EaseType::EaseInOutExpo }, { "EaseOutInExpo", EaseType::EaseOutInExpo },
        { "EaseInCirc", EaseType::EaseInCirc }, { "EaseOutCirc", EaseType::EaseOutCirc },
        { "EaseInOutCirc", EaseType::EaseInOutCirc }, { "EaseOutInCirc", EaseType::EaseOutInCirc },
        { "EaseInBack", EaseType::EaseInBack }, { "EaseOutBack", EaseType::EaseOutBack },
        { "EaseInOutBack", EaseType::EaseInOutBack }, { "EaseOutInBack", EaseType::EaseOutInBack },
        { "EaseInElastic", EaseType::EaseInElastic }, { "EaseOutElastic", EaseType::EaseOutElastic },
        { "EaseInOutElastic", EaseType::EaseInOutElastic }, { "EaseOutInElastic", EaseType::EaseOutInElastic },
        { "EaseInBounce", EaseType::EaseInBounce }, { "EaseOutBounce", EaseType::EaseOutBounce },
        { "EaseInOutBounce", EaseType::EaseInOutBounce }, { "EaseOutInBounce", EaseType::EaseOutInBounce },
    };
    for (const auto &entry : kEaseTypeValues) {
        engine->RegisterEnumValue("EaseType", entry.first, static_cast<int>(entry.second));
    }

    // Easing:: 名前空間へユーティリティ関数をまとめて登録する
    asbind20::namespace_ easingNamespace(engine, "Easing");
    asbind20::global(engine)
        .function("float Normalize01(const float &in, const float &in, const float &in)",
            static_cast<float (*)(const float &, const float &, const float &)>(&Normalize01))
        .function("Vector2 Normalize01(const Vector2 &in, const Vector2 &in, const Vector2 &in)",
            static_cast<Vector2 (*)(const Vector2 &, const Vector2 &, const Vector2 &)>(&Normalize01))
        .function("Vector3 Normalize01(const Vector3 &in, const Vector3 &in, const Vector3 &in)",
            static_cast<Vector3 (*)(const Vector3 &, const Vector3 &, const Vector3 &)>(&Normalize01))
        .function("Vector4 Normalize01(const Vector4 &in, const Vector4 &in, const Vector4 &in)",
            static_cast<Vector4 (*)(const Vector4 &, const Vector4 &, const Vector4 &)>(&Normalize01))
        .function("float Lerp(const float &in, const float &in, const float &in)",
            static_cast<float (*)(const float &, const float &, const float &)>(&Lerp))
        .function("float Apply(float, EaseType)", &Apply)
        .function("float Eased(const float &in, const float &in, float, EaseType)", &Eased<float>)
        .function("Vector2 Eased(const Vector2 &in, const Vector2 &in, float, EaseType)", &Eased<Vector2>)
        .function("Vector3 Eased(const Vector3 &in, const Vector3 &in, float, EaseType)", &Eased<Vector3>)
        .function("Vector4 Eased(const Vector4 &in, const Vector4 &in, float, EaseType)", &Eased<Vector4>)
        .function("float EasedGAB(const float &in, const float &in, float, EaseType, EaseType)", &EasedGAB<float>)
        .function("Vector2 EasedGAB(const Vector2 &in, const Vector2 &in, float, EaseType, EaseType)", &EasedGAB<Vector2>)
        .function("Vector3 EasedGAB(const Vector3 &in, const Vector3 &in, float, EaseType, EaseType)", &EasedGAB<Vector3>)
        .function("Vector4 EasedGAB(const Vector4 &in, const Vector4 &in, float, EaseType, EaseType)", &EasedGAB<Vector4>);
}

//==================================================
// 乱数（Utilities/RandomValue.h）
//==================================================

/// @brief Random:: 名前空間へ乱数ユーティリティ関数を登録する
void RegisterRandomBindings(asIScriptEngine *engine) {
    asbind20::namespace_ randomNamespace(engine, "Random");
    asbind20::global(engine)
        .function("int Int(int, int)", &GetRandomInt)
        .function("int64 Int64(int64, int64)", &GetRandomInt64)
        .function("float Float(float, float)", &GetRandomFloat)
        .function("double Double(double, double)", &GetRandomDouble)
        .function("bool Bool(float trueProbability = 0.5f)", &GetRandomBool);
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
        })
        .function("bool AddComponent(?&out)", [](void *ref, int typeId) -> bool {
            auto *owner = gCurrentObjectContext ? const_cast<EmptyObject *>(gCurrentObjectContext->GetOwner()) : nullptr;
            return owner ? AddComponentIntoHandle(*owner, ref, typeId) : false;
        })
        .function("bool RemoveComponent(?&in)", [](void *ref, int typeId) -> bool {
            auto *owner = gCurrentObjectContext ? const_cast<EmptyObject *>(gCurrentObjectContext->GetOwner()) : nullptr;
            return owner ? RemoveComponentFromHandle(*owner, ref, typeId) : false;
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
    // Transform は Object::GetTransform() が参照するため、Object/Scene より先に登録する
    RegisterTransformType(engine);
    // Object/Scene はここで一度に登録する。以降に登録するコンポーネント（MeshRenderer等）は
    // Object@/Scene@ をパラメータ/戻り値として自由に参照できる（型・メソッドとも登録済みのため）
    RegisterObjectTypes(engine);
    RegisterComponentTypes(engine);
    RegisterEasingBindings(engine);
    RegisterRandomBindings(engine);
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
