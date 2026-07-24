#include "Scene/Components/Script/ScriptBindings.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string_view>
#include <unordered_map>

#include <angelscript.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptdictionary/scriptdictionary.h>
#include <asbind20/asbind.hpp>

#include "Assets/AudioManager.h"
#include "Assets/ModelManager.h"
#include "ComponentSerialize/ComponentRegistry.h"
#include "Core/Window.h"
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
#if defined(USE_IMGUI)
#include "Scene/Components/Script/EditorToolManager.h"
#endif
#include "Utilities/FileIO/JSON.h"
#include "Utilities/MathUtils.h"
#include "Utilities/MyAny.h"
#include "Utilities/RandomValue.h"
#include "Utilities/StageGraphGenerator.h"
#include "Utilities/StageGridBuilder.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/ValueType.h"
#include "Utilities/WaveFunctionCollapse.h"

// オブジェクトコンポーネント（全種類をスクリプトへ登録する）
#include "Objects/Components/Animator.h"
#include "Objects/Components/KeyFrameAnimator.h"
#include "Objects/Components/InputCommandApplier.h"
#include "Objects/Components/SceneVariableApplier.h"
#include "Objects/Components/Shake.h"
#include "Objects/Components/AudioListener.h"
#include "Objects/Components/Comment.h"
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
#include "Objects/Components/PostProcessing/AmbientOcclusionEffect.h"
#include "Objects/Components/PostProcessing/BloomEffect.h"
#include "Objects/Components/PostProcessing/BoxFilterEffect.h"
#include "Objects/Components/PostProcessing/ChromaticAberrationEffect.h"
#include "Objects/Components/PostProcessing/ColorAdjustEffect.h"
#include "Objects/Components/PostProcessing/DepthOfFieldEffect.h"
#include "Objects/Components/PostProcessing/DissolveEffect.h"
#include "Objects/Components/PostProcessing/DitherEffect.h"
#include "Objects/Components/PostProcessing/DotMatrixEffect.h"
#include "Objects/Components/PostProcessing/FXAAEffect.h"
#include "Objects/Components/PostProcessing/GaussianFilterEffect.h"
#include "Objects/Components/PostProcessing/GrayscaleEffect.h"
#include "Objects/Components/PostProcessing/MotionBlurEffect.h"
#include "Objects/Components/PostProcessing/OutlineEffect.h"
#include "Objects/Components/PostProcessing/RadialBlurEffect.h"
#include "Objects/Components/PostProcessing/VignetteEffect.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/CameraController.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/IWindowObjectComponent.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/ShadowMapObject.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/Components/TargetLookAt.h"
#include "Objects/Components/ParticleSystem2D.h"
#include "Objects/Components/ParticleSystem3D.h"
#include "Objects/Components/PreTransform.h"
#include "Objects/Components/Rotation.h"
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
// タグ型
//==================================================

/// @brief Tag型を値型として登録する
/// @details 文字列からハッシュ値を計算して保持する軽量な比較用タグ。
///          オブジェクト/コンポーネントのGetTag()との比較や、スクリプト内での分類判定に使う
void RegisterTagType(asIScriptEngine *engine) {
    asbind20::value_class<Tag>(engine, "Tag", asOBJ_APP_CLASS_ALLINTS | asOBJ_APP_CLASS_MORE_CONSTRUCTORS)
        .behaviours_by_traits()
        .constructor<const std::string &>("const string &in name")
        .opEquals()
        .method("uint64 GetHash() const", &Tag::GetHash)
        .method("bool IsEmpty() const", &Tag::IsEmpty);
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
        .method("const string &GetComponentType() const", static_cast<const std::string &(T::*)() const>(&T::GetComponentType))
        .method("void SetTag(const string &in)", static_cast<void (T::*)(const std::string &)>(&T::SetTag))
        .method("Tag GetTag() const", [](const T &component) -> Tag { return component.GetTag(); })
        .method("const string &GetTagName() const", [](const T &component) -> const std::string & { return component.GetTagName(); });

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

/// @brief Collider（ICollider基底）型を参照型として登録する
/// @details HitInfoのselfCollider/otherColliderで「どのコライダー同士が衝突したか」を
///          受け渡すための共通型。各コライダー型はopImplCastでこの型へ暗黙変換でき、
///          cast<BoxCollider>(hit.otherCollider) のように具体型へダウンキャストもできる
void RegisterColliderBaseType(asIScriptEngine *engine) {
    asbind20::ref_class<ICollider>(engine, "Collider", asOBJ_NOCOUNT)
        .method("bool IsActive() const", static_cast<bool (ICollider::*)() const>(&ICollider::IsActive))
        .method("void SetActive(bool)", static_cast<void (ICollider::*)(bool)>(&ICollider::SetActive))
        .method("const string &GetComponentType() const", static_cast<const std::string &(ICollider::*)() const>(&ICollider::GetComponentType))
        .method("void SetTag(const string &in)", static_cast<void (ICollider::*)(const std::string &)>(&ICollider::SetTag))
        .method("Tag GetTag() const", [](const ICollider &collider) -> Tag { return collider.GetTag(); })
        .method("const string &GetTagName() const", [](const ICollider &collider) -> const std::string & { return collider.GetTagName(); })
        .method("bool IsTrigger() const", static_cast<bool (ICollider::*)() const noexcept>(&ICollider::IsTrigger))
        .method("void SetTrigger(bool)", static_cast<void (ICollider::*)(bool) noexcept>(&ICollider::SetTrigger))
        .method("bool IsContinuousDetection() const", static_cast<bool (ICollider::*)() const noexcept>(&ICollider::IsContinuousDetection))
        .method("void SetContinuousDetection(bool)", static_cast<void (ICollider::*)(bool) noexcept>(&ICollider::SetContinuousDetection))
        .method("bool Is2D() const", static_cast<bool (ICollider::*)() const noexcept>(&ICollider::Is2D));
}

/// @brief Collider@ から具体的なコライダー型へのダウンキャスト（cast<T>用）
template <typename T>
T *ColliderDownCast(ICollider *collider) {
    return dynamic_cast<T *>(collider);
}

/// @brief コライダー型を登録する（ICollider共通のメソッドを追加で登録する）
template <typename T>
auto RegisterColliderType(asIScriptEngine *engine, const char *name) {
    auto binder = RegisterComponentType<T>(engine, name);
    binder
        .method("bool IsTrigger() const", static_cast<bool (T::*)() const noexcept>(&T::IsTrigger))
        .method("void SetTrigger(bool)", static_cast<void (T::*)(bool) noexcept>(&T::SetTrigger))
        .method("bool IsContinuousDetection() const", static_cast<bool (T::*)() const noexcept>(&T::IsContinuousDetection))
        .method("void SetContinuousDetection(bool)", static_cast<void (T::*)(bool) noexcept>(&T::SetContinuousDetection))
        .method("bool Is2D() const", static_cast<bool (T::*)() const noexcept>(&T::Is2D))
        // 基底のCollider型への暗黙変換（HitInfoのselfCollider/otherColliderとの比較用）
        .method("Collider@ opImplCast()", [](T &collider) -> ICollider * { return &collider; });
    // cast<具体型>(Collider@) によるダウンキャスト
    engine->RegisterObjectMethod("Collider", (std::string(name) + "@ opCast()").c_str(),
        asFUNCTION((ColliderDownCast<T>)), asCALL_CDECL_OBJLAST);
    return binder;
}

/// @brief WindowObject（IWindowObjectComponent基底）型を参照型として登録する
/// @details WindowMessageInfoのsourceComponentで「どのウィンドウコンポーネントからの通知か」を
///          受け渡すための共通型。NormalWindowObject/OverlayWindowObjectはopImplCastでこの型へ
///          暗黙変換でき、cast<NormalWindowObject>(info.sourceComponent) のように具体型へダウンキャストもできる
void RegisterWindowObjectBaseType(asIScriptEngine *engine) {
    asbind20::ref_class<IWindowObjectComponent>(engine, "WindowObject", asOBJ_NOCOUNT)
        .method("bool IsActive() const", static_cast<bool (IWindowObjectComponent::*)() const>(&IWindowObjectComponent::IsActive))
        .method("void SetActive(bool)", static_cast<void (IWindowObjectComponent::*)(bool)>(&IWindowObjectComponent::SetActive))
        .method("const string &GetComponentType() const", static_cast<const std::string &(IWindowObjectComponent::*)() const>(&IWindowObjectComponent::GetComponentType))
        .method("void SetTag(const string &in)", static_cast<void (IWindowObjectComponent::*)(const std::string &)>(&IWindowObjectComponent::SetTag))
        .method("Tag GetTag() const", [](const IWindowObjectComponent &component) -> Tag { return component.GetTag(); })
        .method("const string &GetTagName() const", [](const IWindowObjectComponent &component) -> const std::string & { return component.GetTagName(); })
        .method("void SetTitle(const string &in)", &IWindowObjectComponent::SetTitle)
        .method("const string &GetTitle() const", &IWindowObjectComponent::GetTitle)
        .method("void SetSize(uint, uint)", &IWindowObjectComponent::SetSize)
        .method("bool SetMessageIntercepted(uint msg, bool enabled)", &IWindowObjectComponent::SetMessageIntercepted)
        .method("bool IsMessageIntercepted(uint msg) const", &IWindowObjectComponent::IsMessageIntercepted)
        .method("void CloseWindow()", &IWindowObjectComponent::CloseWindow)
        .method("int GetClientWidth() const", [](const IWindowObjectComponent &component) -> int {
            Window *window = component.GetWindow();
            return (window && Window::IsExist(window)) ? window->GetClientWidth() : 0;
        })
        .method("int GetClientHeight() const", [](const IWindowObjectComponent &component) -> int {
            Window *window = component.GetWindow();
            return (window && Window::IsExist(window)) ? window->GetClientHeight() : 0;
        })
        .method("bool IsWindowFocused() const", [](const IWindowObjectComponent &component) -> bool {
            Window *window = component.GetWindow();
            return (window && Window::IsExist(window)) ? window->IsFocused() : false;
        })
        .method("bool IsWindowMinimized() const", [](const IWindowObjectComponent &component) -> bool {
            Window *window = component.GetWindow();
            return (window && Window::IsExist(window)) ? window->IsMinimized() : false;
        });
}

/// @brief WindowObject@ から具体的なウィンドウコンポーネント型へのダウンキャスト（cast<T>用）
template <typename T>
T *WindowObjectDownCast(IWindowObjectComponent *component) {
    return dynamic_cast<T *>(component);
}

/// @brief よく使うウィンドウメッセージ定数（WM_*）をスクリプトへ登録する
void RegisterWindowMessageConstants(asIScriptEngine *engine) {
    static const std::pair<const char *, asUINT> kConstants[] = {
        { "WM_ACTIVATE", WM_ACTIVATE },
        { "WM_CLOSE", WM_CLOSE },
        { "WM_DESTROY", WM_DESTROY },
        { "WM_MOVE", WM_MOVE },
        { "WM_SIZE", WM_SIZE },
        { "WM_SIZING", WM_SIZING },
        { "WM_ENTERSIZEMOVE", WM_ENTERSIZEMOVE },
        { "WM_EXITSIZEMOVE", WM_EXITSIZEMOVE },
        { "WM_SETFOCUS", WM_SETFOCUS },
        { "WM_KILLFOCUS", WM_KILLFOCUS },
        { "WM_KEYDOWN", WM_KEYDOWN },
        { "WM_KEYUP", WM_KEYUP },
        { "WM_SYSKEYDOWN", WM_SYSKEYDOWN },
        { "WM_SYSKEYUP", WM_SYSKEYUP },
        { "WM_CHAR", WM_CHAR },
        { "WM_MOUSEMOVE", WM_MOUSEMOVE },
        { "WM_LBUTTONDOWN", WM_LBUTTONDOWN },
        { "WM_LBUTTONUP", WM_LBUTTONUP },
        { "WM_LBUTTONDBLCLK", WM_LBUTTONDBLCLK },
        { "WM_RBUTTONDOWN", WM_RBUTTONDOWN },
        { "WM_RBUTTONUP", WM_RBUTTONUP },
        { "WM_RBUTTONDBLCLK", WM_RBUTTONDBLCLK },
        { "WM_MBUTTONDOWN", WM_MBUTTONDOWN },
        { "WM_MBUTTONUP", WM_MBUTTONUP },
        { "WM_MOUSEWHEEL", WM_MOUSEWHEEL },
        { "WM_MOUSEHWHEEL", WM_MOUSEHWHEEL },
        { "WM_DROPFILES", WM_DROPFILES },
        { "WM_PAINT", WM_PAINT },
    };
    // RegisterGlobalPropertyへは値のアドレスを渡すため、静的領域に値を保持する
    static asUINT sValues[std::size(kConstants)];
    for (size_t i = 0; i < std::size(kConstants); ++i) {
        sValues[i] = kConstants[i].second;
        const std::string decl = std::string("const uint ") + kConstants[i].first;
        engine->RegisterGlobalProperty(decl.c_str(), &sValues[i]);
    }
}

/// @brief Light::Type をスクリプト用の LightType 列挙型として登録する
void RegisterLightTypeEnum(asIScriptEngine *engine) {
    engine->RegisterEnum("LightType");
    engine->RegisterEnumValue("LightType", "Directional", static_cast<int>(Light::Type::Directional));
    engine->RegisterEnumValue("LightType", "Point", static_cast<int>(Light::Type::Point));
    engine->RegisterEnumValue("LightType", "Spot", static_cast<int>(Light::Type::Spot));
    engine->RegisterEnumValue("LightType", "Rect", static_cast<int>(Light::Type::Rect));
    engine->RegisterEnumValue("LightType", "Sphere", static_cast<int>(Light::Type::Sphere));
    engine->RegisterEnumValue("LightType", "Disc", static_cast<int>(Light::Type::Disc));
    engine->RegisterEnumValue("LightType", "Tube", static_cast<int>(Light::Type::Tube));
    engine->RegisterEnumValue("LightType", "Box", static_cast<int>(Light::Type::Box));
}

/// @brief TextRenderer::HorizontalAlign/VerticalAlign をスクリプト用の列挙型として登録する
void RegisterTextRendererEnums(asIScriptEngine *engine) {
    engine->RegisterEnum("TextHorizontalAlign");
    engine->RegisterEnumValue("TextHorizontalAlign", "Left", static_cast<int>(TextRenderer::HorizontalAlign::Left));
    engine->RegisterEnumValue("TextHorizontalAlign", "Center", static_cast<int>(TextRenderer::HorizontalAlign::Center));
    engine->RegisterEnumValue("TextHorizontalAlign", "Right", static_cast<int>(TextRenderer::HorizontalAlign::Right));

    engine->RegisterEnum("TextVerticalAlign");
    engine->RegisterEnumValue("TextVerticalAlign", "Top", static_cast<int>(TextRenderer::VerticalAlign::Top));
    engine->RegisterEnumValue("TextVerticalAlign", "Middle", static_cast<int>(TextRenderer::VerticalAlign::Middle));
    engine->RegisterEnumValue("TextVerticalAlign", "Bottom", static_cast<int>(TextRenderer::VerticalAlign::Bottom));
}

/// @brief Transformコンポーネントを登録する
/// @details Object::GetTransform() が Transform@ を返すため、Object/Scene（RegisterObjectTypes）より
///          先に登録しておく必要がある。gComponentTypeBindings のクリアもここで行う（最初に呼ばれるため）
void RegisterTransformType(asIScriptEngine *engine) {
    gComponentTypeBindings.clear();
    RegisterLightTypeEnum(engine);
    RegisterTextRendererEnums(engine);

    RegisterComponentType<Transform>(engine, "Transform")
        .method("void SetTranslate(const Vector3 &in)", &Transform::SetTranslate)
        .method("const Vector3 &GetTranslate() const", &Transform::GetTranslate)
        .method("void SetRotate(const Vector3 &in)", &Transform::SetRotate)
        .method("const Vector3 &GetRotate() const", &Transform::GetRotate)
        .method("void SetRotateQuaternion(const Quaternion &in)", &Transform::SetRotateQuaternion)
        .method("const Quaternion &GetRotateQuaternion() const", &Transform::GetRotateQuaternion)
        .method("void SetScale(const Vector3 &in)", &Transform::SetScale)
        .method("const Vector3 &GetScale() const", &Transform::GetScale)
        .method("const Matrix4x4 &GetWorldMatrix()", &Transform::GetWorldMatrix)
        .method("Vector3 GetWorldPosition()", &Transform::GetWorldPosition)
        .method("Vector3 GetWorldRotate() const", &Transform::GetWorldRotate)
        .method("Quaternion GetWorldRotateQuaternion() const", &Transform::GetWorldRotateQuaternion)
        .method("Vector3 GetWorldScale()", &Transform::GetWorldScale);
}

void RegisterComponentTypes(asIScriptEngine *engine) {
    RegisterComponentType<Velocity>(engine, "Velocity")
        .method("void SetVelocity(const Vector3 &in)", &Velocity::SetVelocity)
        .method("const Vector3 &GetVelocity() const", &Velocity::GetVelocity)
        .method("void SetAcceleration(const Vector3 &in)", &Velocity::SetAcceleration)
        .method("const Vector3 &GetAcceleration() const", &Velocity::GetAcceleration)
        .method("void AddVelocity(const Vector3 &in)", &Velocity::AddVelocity);

    RegisterComponentType<Rotation>(engine, "Rotation")
        .method("void SetAngularVelocity(const Vector3 &in)", &Rotation::SetAngularVelocity)
        .method("const Vector3 &GetAngularVelocity() const", &Rotation::GetAngularVelocity)
        .method("void SetAngularAcceleration(const Vector3 &in)", &Rotation::SetAngularAcceleration)
        .method("const Vector3 &GetAngularAcceleration() const", &Rotation::GetAngularAcceleration)
        .method("void AddAngularVelocity(const Vector3 &in)", &Rotation::AddAngularVelocity);

    RegisterComponentType<PreTransform>(engine, "PreTransform")
        .method("const Vector3 &GetPreviousTranslate() const", &PreTransform::GetPreviousTranslate)
        .method("const Vector3 &GetPreviousRotate() const", &PreTransform::GetPreviousRotate)
        .method("const Quaternion &GetPreviousRotateQuaternion() const", &PreTransform::GetPreviousRotateQuaternion)
        .method("const Vector3 &GetPreviousScale() const", &PreTransform::GetPreviousScale)
        .method("const Matrix4x4 &GetPreviousWorldMatrix() const", &PreTransform::GetPreviousWorldMatrix)
        .method("const Vector3 &GetPreviousWorldPosition() const", &PreTransform::GetPreviousWorldPosition)
        .method("const Vector3 &GetPreviousWorldRotate() const", &PreTransform::GetPreviousWorldRotate)
        .method("const Quaternion &GetPreviousWorldRotateQuaternion() const", &PreTransform::GetPreviousWorldRotateQuaternion)
        .method("const Vector3 &GetPreviousWorldScale() const", &PreTransform::GetPreviousWorldScale);

    // TargetLookAtの回転モード（ParticleSystemのビルボード設定でも使うため先に登録する）
    engine->RegisterEnum("TargetLookAtMode");
    engine->RegisterEnumValue("TargetLookAtMode", "SyncRotation", static_cast<int>(TargetLookAt::RotationMode::SyncRotation));
    engine->RegisterEnumValue("TargetLookAtMode", "LookAt", static_cast<int>(TargetLookAt::RotationMode::LookAt));

    RegisterComponentType<ParticleSystem2D>(engine, "ParticleSystem2D")
        .method("void Play()", [](ParticleSystem2D &self) { self.Play(); })
        .method("void Stop()", [](ParticleSystem2D &self) { self.Stop(); })
        .method("bool IsPlaying() const", [](const ParticleSystem2D &self) -> bool { return self.IsPlaying(); })
        .method("void Clear()", [](ParticleSystem2D &self) { self.Clear(); })
        .method("void SetEmissionRate(float)", [](ParticleSystem2D &self, float rate) { self.SetEmissionRate(rate); })
        .method("float GetEmissionRate() const", [](const ParticleSystem2D &self) -> float { return self.GetEmissionRate(); })
        .method("void SetMaxParticles(int)", [](ParticleSystem2D &self, int count) { self.SetMaxParticles(count); })
        .method("int GetMaxParticles() const", [](const ParticleSystem2D &self) -> int { return self.GetMaxParticles(); })
        .method("void SetBillboard(bool)", [](ParticleSystem2D &self, bool enabled) { self.SetBillboard(enabled); })
        .method("bool IsBillboard() const", [](const ParticleSystem2D &self) -> bool { return self.IsBillboard(); })
        .method("void SetBillboardTarget(Object@)", [](ParticleSystem2D &self, EmptyObject *obj) { self.SetBillboardTarget(obj); })
        .method("Object@ GetBillboardTarget() const", [](const ParticleSystem2D &self) -> EmptyObject * { return self.GetBillboardTarget(); })
        .method("void SetBillboardRotationMode(TargetLookAtMode)", [](ParticleSystem2D &self, TargetLookAt::RotationMode mode) { self.SetBillboardRotationMode(mode); })
        .method("TargetLookAtMode GetBillboardRotationMode() const", [](const ParticleSystem2D &self) -> TargetLookAt::RotationMode { return self.GetBillboardRotationMode(); });

    RegisterComponentType<ParticleSystem3D>(engine, "ParticleSystem3D")
        .method("void Play()", [](ParticleSystem3D &self) { self.Play(); })
        .method("void Stop()", [](ParticleSystem3D &self) { self.Stop(); })
        .method("bool IsPlaying() const", [](const ParticleSystem3D &self) -> bool { return self.IsPlaying(); })
        .method("void Clear()", [](ParticleSystem3D &self) { self.Clear(); })
        .method("void SetEmissionRate(float)", [](ParticleSystem3D &self, float rate) { self.SetEmissionRate(rate); })
        .method("float GetEmissionRate() const", [](const ParticleSystem3D &self) -> float { return self.GetEmissionRate(); })
        .method("void SetMaxParticles(int)", [](ParticleSystem3D &self, int count) { self.SetMaxParticles(count); })
        .method("int GetMaxParticles() const", [](const ParticleSystem3D &self) -> int { return self.GetMaxParticles(); })
        .method("void SetBillboard(bool)", [](ParticleSystem3D &self, bool enabled) { self.SetBillboard(enabled); })
        .method("bool IsBillboard() const", [](const ParticleSystem3D &self) -> bool { return self.IsBillboard(); })
        .method("void SetBillboardTarget(Object@)", [](ParticleSystem3D &self, EmptyObject *obj) { self.SetBillboardTarget(obj); })
        .method("Object@ GetBillboardTarget() const", [](const ParticleSystem3D &self) -> EmptyObject * { return self.GetBillboardTarget(); })
        .method("void SetBillboardRotationMode(TargetLookAtMode)", [](ParticleSystem3D &self, TargetLookAt::RotationMode mode) { self.SetBillboardRotationMode(mode); })
        .method("TargetLookAtMode GetBillboardRotationMode() const", [](const ParticleSystem3D &self) -> TargetLookAt::RotationMode { return self.GetBillboardRotationMode(); });

    RegisterComponentType<TargetLookAt>(engine, "TargetLookAt")
        .method("void SetTargetObject(Object@)", [](TargetLookAt &c, EmptyObject *obj) { c.SetTargetObject(obj); })
        .method("Object@ GetTargetObject() const", &TargetLookAt::GetTargetObject)
        .method("void SetRotationOffset(const Vector3 &in)", &TargetLookAt::SetRotationOffset)
        .method("const Vector3 &GetRotationOffset() const", &TargetLookAt::GetRotationOffset)
        .method("void SetRotationMode(TargetLookAtMode)", &TargetLookAt::SetRotationMode)
        .method("TargetLookAtMode GetRotationMode() const", &TargetLookAt::GetRotationMode);

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
        .method("bool Reload()", &ScriptComponent::Reload)
        // 他オブジェクトのScriptComponentを取得した上で、その[SerializeField]変数を名前で直接読み書きする
        // （シーン変数を介さないスクリプト間のデータ受け渡し用。対応型はSerializeFieldと同じプリミティブ/数学型/Object@のみ）
        .method("bool GetVariable(const string &in, ?&out) const", [](const ScriptComponent &self, const std::string &name, void *ref, int typeId) -> bool {
            return self.GetVariable(name, ref, typeId);
        })
        .method("bool SetVariable(const string &in, ?&in)", [](ScriptComponent &self, const std::string &name, void *ref, int typeId) -> bool {
            return self.SetVariable(name, ref, typeId);
        });

    RegisterComponentType<MeshFilter>(engine, "MeshFilter")
        .method("void SetMeshHandle(uint)", [](MeshFilter &c, uint32_t handle) { c.SetMeshHandle(handle); })
        .method("uint GetMeshHandle() const", [](const MeshFilter &c) -> uint32_t { return c.GetMeshHandle(); })
        .method("bool HasMesh() const", &MeshFilter::HasMesh);

    RegisterComponentType<Animator>(engine, "Animator")
        .method("void SetAnimationName(const string &in)", &Animator::SetAnimationName)
        .method("const string &GetAnimationName() const", &Animator::GetAnimationName)
        .method("void SetPlayOnStart(bool)", &Animator::SetPlayOnStart)
        .method("bool GetPlayOnStart() const", &Animator::GetPlayOnStart);

    RegisterComponentType<KeyFrameAnimator>(engine, "KeyFrameAnimator")
        .method("bool Play(const string &in name)", &KeyFrameAnimator::Play)
        .method("bool Stop(const string &in name)", &KeyFrameAnimator::Stop)
        .method("void PlayAll()", &KeyFrameAnimator::PlayAll)
        .method("void StopAll()", &KeyFrameAnimator::StopAll)
        .method("bool IsPlaying(const string &in name) const", &KeyFrameAnimator::IsPlaying)
        .method("bool SetPlaybackSpeed(const string &in name, float speed)", &KeyFrameAnimator::SetPlaybackSpeed)
        .method("float GetPlaybackSpeed(const string &in name) const", &KeyFrameAnimator::GetPlaybackSpeed)
        .method("bool TryGetValue(const string &in name, float &out value) const", &KeyFrameAnimator::TryGetValue)
        .method("uint GetAnimationCount() const", [](const KeyFrameAnimator &animator) -> std::uint32_t {
            return static_cast<std::uint32_t>(animator.GetAnimationCount());
        });

    RegisterComponentType<InputCommandApplier>(engine, "InputCommandApplier")
        .method("void SetCommandName(const string &in)", &InputCommandApplier::SetCommandName)
        .method("const string &GetCommandName() const", &InputCommandApplier::GetCommandName)
        .method("void SetThreshold(float)", &InputCommandApplier::SetThreshold)
        .method("float GetThreshold() const", &InputCommandApplier::GetThreshold)
        .method("void SetFixedValue(float)", &InputCommandApplier::SetFixedValue)
        .method("float GetFixedValue() const", &InputCommandApplier::GetFixedValue)
        .method("void SetValueScale(float)", &InputCommandApplier::SetValueScale)
        .method("float GetValueScale() const", &InputCommandApplier::GetValueScale)
        .method("void SetValueOffset(float)", &InputCommandApplier::SetValueOffset)
        .method("float GetValueOffset() const", &InputCommandApplier::GetValueOffset)
        .method("bool WasApplied() const", &InputCommandApplier::WasApplied)
        .method("float GetLastValue() const", &InputCommandApplier::GetLastValue);

    RegisterComponentType<SceneVariableApplier>(engine, "SceneVariableApplier")
        .method("void SetVariableName(const string &in)", &SceneVariableApplier::SetVariableName)
        .method("const string &GetVariableName() const", &SceneVariableApplier::GetVariableName)
        .method("bool WasApplied() const", &SceneVariableApplier::WasApplied);

    RegisterComponentType<Shake>(engine, "Shake")
        .method("void Play(float = 0.0f)", &Shake::Play)
        .method("void Stop()", &Shake::Stop)
        .method("bool IsPlaying() const", &Shake::IsPlaying)
        // ProcessTiming: 0=Immediate, 1=DeferredEnd
        .method("void SetProcessTiming(int)", &Shake::SetProcessTimingInt)
        .method("int GetProcessTiming() const", &Shake::GetProcessTimingInt)
        // ApplyTarget: 0=ToTransform, 1=RenderOnly
        .method("void SetApplyTarget(int)", &Shake::SetApplyTargetInt)
        .method("int GetApplyTarget() const", &Shake::GetApplyTargetInt)
        .method("void SetPositionEnable(bool, bool, bool)", &Shake::SetPositionEnable)
        .method("void SetPositionAmplitude(const Vector3 &in)", &Shake::SetPositionAmplitude)
        .method("const Vector3 &GetPositionAmplitude() const", &Shake::GetPositionAmplitude)
        .method("void SetPositionSpeed(const Vector3 &in)", &Shake::SetPositionSpeed)
        .method("const Vector3 &GetPositionSpeed() const", &Shake::GetPositionSpeed)
        .method("void SetPositionEaseType(int)", &Shake::SetPositionEaseTypeInt)
        .method("int GetPositionEaseType() const", &Shake::GetPositionEaseTypeInt)
        .method("void SetRotationEnable(bool, bool, bool)", &Shake::SetRotationEnable)
        .method("void SetRotationAmplitude(const Vector3 &in)", &Shake::SetRotationAmplitude)
        .method("const Vector3 &GetRotationAmplitude() const", &Shake::GetRotationAmplitude)
        .method("void SetRotationSpeed(const Vector3 &in)", &Shake::SetRotationSpeed)
        .method("const Vector3 &GetRotationSpeed() const", &Shake::GetRotationSpeed)
        .method("void SetRotationEaseType(int)", &Shake::SetRotationEaseTypeInt)
        .method("int GetRotationEaseType() const", &Shake::GetRotationEaseTypeInt);

    RegisterComponentType<TextRenderer>(engine, "TextRenderer")
        .method("void SetText(const string &in)", &TextRenderer::SetText)
        .method("const string &GetText() const", &TextRenderer::GetText)
        .method("void SetFontName(const string &in)", &TextRenderer::SetFontName)
        .method("const string &GetFontName() const", &TextRenderer::GetFontName)
        .method("void SetFontSize(float)", &TextRenderer::SetFontSize)
        .method("float GetFontSize() const", &TextRenderer::GetFontSize)
        .method("void SetColor(const Vector4 &in)", &TextRenderer::SetColor)
        .method("const Vector4 &GetColor() const", &TextRenderer::GetColor)
        .method("void SetHorizontalAlign(TextHorizontalAlign)", [](TextRenderer &c, int align) {
            c.SetHorizontalAlign(static_cast<TextRenderer::HorizontalAlign>(align));
        })
        .method("TextHorizontalAlign GetHorizontalAlign() const", [](const TextRenderer &c) -> int {
            return static_cast<int>(c.GetHorizontalAlign());
        })
        .method("void SetVerticalAlign(TextVerticalAlign)", [](TextRenderer &c, int align) {
            c.SetVerticalAlign(static_cast<TextRenderer::VerticalAlign>(align));
        })
        .method("TextVerticalAlign GetVerticalAlign() const", [](const TextRenderer &c) -> int {
            return static_cast<int>(c.GetVerticalAlign());
        })
        .method("void SetDefaultCharacterAnchor(const Vector2 &in)", &TextRenderer::SetDefaultCharacterAnchor)
        .method("const Vector2 &GetDefaultCharacterAnchor() const", &TextRenderer::GetDefaultCharacterAnchor)
        .method("void SetDefaultCharacterPivot(const Vector2 &in)", &TextRenderer::SetDefaultCharacterPivot)
        .method("const Vector2 &GetDefaultCharacterPivot() const", &TextRenderer::GetDefaultCharacterPivot)
        .method("void SetTargetObject(Object@)", [](TextRenderer &c, EmptyObject *obj) { c.SetTargetObject(obj); })
        .method("void SetPipelineName(const string &in)", &TextRenderer::SetPipelineName)
        .method("const string &GetPipelineName() const", &TextRenderer::GetPipelineName)
        .method("uint64 GetCharacterCount() const", [](const TextRenderer &c) -> uint64_t { return static_cast<uint64_t>(c.GetCharacterCount()); })
        .method("void SetCharacterOffset(uint64, const Vector2 &in)", [](TextRenderer &c, uint64_t index, const Vector2 &offset) {
            c.SetCharacterOffset(static_cast<size_t>(index), offset);
        })
        .method("Vector2 GetCharacterOffset(uint64) const", [](const TextRenderer &c, uint64_t index) -> Vector2 {
            return c.GetCharacterOffset(static_cast<size_t>(index));
        })
        .method("void SetCharacterRotation(uint64, float)", [](TextRenderer &c, uint64_t index, float rotation) {
            c.SetCharacterRotation(static_cast<size_t>(index), rotation);
        })
        .method("float GetCharacterRotation(uint64) const", [](const TextRenderer &c, uint64_t index) -> float {
            return c.GetCharacterRotation(static_cast<size_t>(index));
        })
        .method("void SetCharacterScale(uint64, const Vector2 &in)", [](TextRenderer &c, uint64_t index, const Vector2 &scale) {
            c.SetCharacterScale(static_cast<size_t>(index), scale);
        })
        .method("Vector2 GetCharacterScale(uint64) const", [](const TextRenderer &c, uint64_t index) -> Vector2 {
            return c.GetCharacterScale(static_cast<size_t>(index));
        });

    RegisterComponentType<Comment>(engine, "Comment")
        .method("void SetComment(const string &in)", &Comment::SetComment)
        .method("const string &GetComment() const", &Comment::GetComment);

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
        .method("void SetTargetObject(Object@)", [](MeshRenderer &c, EmptyObject *obj) { c.SetTargetObject(obj); })
        .method("void SetInstanceColor(const Vector4 &in)", &MeshRenderer::SetInstanceColor)
        .method("const Vector4 &GetInstanceColor() const", &MeshRenderer::GetInstanceColor)
        // instanceColorBlendModeは 0=Override, 1=Multiply, 2=Add, 3=Subtract
        .method("void SetInstanceColorBlendMode(int)", [](MeshRenderer &c, int mode) { c.SetInstanceColorBlendMode(static_cast<MeshRenderer::ColorBlendMode>(mode)); })
        .method("int GetInstanceColorBlendMode() const", [](const MeshRenderer &c) { return static_cast<int>(c.GetInstanceColorBlendMode()); });

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
        .method("float GetOuterAngle() const", &Light::GetOuterAngle)
        .method("void SetSourceRadius(float)", &Light::SetSourceRadius)
        .method("float GetSourceRadius() const", &Light::GetSourceRadius)
        .method("void SetSourceWidth(float)", &Light::SetSourceWidth)
        .method("float GetSourceWidth() const", &Light::GetSourceWidth)
        .method("void SetSourceHeight(float)", &Light::SetSourceHeight)
        .method("float GetSourceHeight() const", &Light::GetSourceHeight)
        .method("void SetSourceLength(float)", &Light::SetSourceLength)
        .method("float GetSourceLength() const", &Light::GetSourceLength)
        .method("void SetSourceDepth(float)", &Light::SetSourceDepth)
        .method("float GetSourceDepth() const", &Light::GetSourceDepth)
        .method("void SetCastShadows(bool)", &Light::SetCastShadows)
        .method("bool IsCastShadows() const", &Light::IsCastShadows)
        .method("void SetShadowDistance(float)", &Light::SetShadowDistance)
        .method("float GetShadowDistance() const", &Light::GetShadowDistance)
        .method("void SetShadowMapResolution(uint)", &Light::SetShadowMapResolution)
        .method("uint GetShadowMapResolution() const", &Light::GetShadowMapResolution)
        .method("void SetShadowBias(float)", &Light::SetShadowBias)
        .method("float GetShadowBias() const", &Light::GetShadowBias)
        .method("void SetShadowSoftness(float)", &Light::SetShadowSoftness)
        .method("float GetShadowSoftness() const", &Light::GetShadowSoftness)
        .method("float GetEffectiveShadowSoftness() const", &Light::GetEffectiveShadowSoftness);

    RegisterComponentType<LightRenderer>(engine, "LightRenderer")
        .method("void SetPipelineName(const string &in)", &LightRenderer::SetPipelineName)
        .method("const string &GetPipelineName() const", &LightRenderer::GetPipelineName)
        .method("Light@ GetLight() const", &LightRenderer::GetLight)
        .method("LightType GetLightType() const", &LightRenderer::GetLightType)
        .method("Vector3 GetWorldPosition() const", &LightRenderer::GetWorldPosition)
        .method("Vector3 GetWorldDirection() const", &LightRenderer::GetWorldDirection)
        .method("Vector3 GetWorldRight() const", &LightRenderer::GetWorldRight)
        .method("Vector3 GetWorldUp() const", &LightRenderer::GetWorldUp);

    RegisterComponentType<NormalWindowObject>(engine, "NormalWindowObject")
        .method("void SetTitle(const string &in)", static_cast<void (NormalWindowObject::*)(const std::string &)>(&NormalWindowObject::SetTitle))
        .method("const string &GetTitle() const", static_cast<const std::string &(NormalWindowObject::*)() const noexcept>(&NormalWindowObject::GetTitle))
        .method("void SetSize(uint, uint)", static_cast<void (NormalWindowObject::*)(std::uint32_t, std::uint32_t)>(&NormalWindowObject::SetSize))
        .method("bool SetMessageIntercepted(uint msg, bool enabled)", static_cast<bool (NormalWindowObject::*)(std::uint32_t, bool)>(&NormalWindowObject::SetMessageIntercepted))
        .method("bool IsMessageIntercepted(uint msg) const", static_cast<bool (NormalWindowObject::*)(std::uint32_t) const>(&NormalWindowObject::IsMessageIntercepted))
        .method("void CloseWindow()", static_cast<void (NormalWindowObject::*)()>(&NormalWindowObject::CloseWindow))
        // 基底のWindowObject型への暗黙変換（WindowMessageInfoのsourceComponentとの比較用）
        .method("WindowObject@ opImplCast()", [](NormalWindowObject &component) -> IWindowObjectComponent * { return &component; });
    engine->RegisterObjectMethod("WindowObject", "NormalWindowObject@ opCast()",
        asFUNCTION((WindowObjectDownCast<NormalWindowObject>)), asCALL_CDECL_OBJLAST);

    RegisterComponentType<OverlayWindowObject>(engine, "OverlayWindowObject")
        .method("void SetTitle(const string &in)", static_cast<void (OverlayWindowObject::*)(const std::string &)>(&OverlayWindowObject::SetTitle))
        .method("const string &GetTitle() const", static_cast<const std::string &(OverlayWindowObject::*)() const noexcept>(&OverlayWindowObject::GetTitle))
        .method("void SetSize(uint, uint)", static_cast<void (OverlayWindowObject::*)(std::uint32_t, std::uint32_t)>(&OverlayWindowObject::SetSize))
        .method("bool SetMessageIntercepted(uint msg, bool enabled)", static_cast<bool (OverlayWindowObject::*)(std::uint32_t, bool)>(&OverlayWindowObject::SetMessageIntercepted))
        .method("bool IsMessageIntercepted(uint msg) const", static_cast<bool (OverlayWindowObject::*)(std::uint32_t) const>(&OverlayWindowObject::IsMessageIntercepted))
        .method("void CloseWindow()", static_cast<void (OverlayWindowObject::*)()>(&OverlayWindowObject::CloseWindow))
        .method("WindowObject@ opImplCast()", [](OverlayWindowObject &component) -> IWindowObjectComponent * { return &component; });
    engine->RegisterObjectMethod("WindowObject", "OverlayWindowObject@ opCast()",
        asFUNCTION((WindowObjectDownCast<OverlayWindowObject>)), asCALL_CDECL_OBJLAST);

    RegisterComponentType<ScreenBufferObject>(engine, "ScreenBufferObject")
        .method("void SetName(const string &in)", &ScreenBufferObject::SetName)
        .method("const string &GetName() const", &ScreenBufferObject::GetName)
        .method("void SetSize(uint, uint)", &ScreenBufferObject::SetSize)
        .method("void SetSaveDirectory(const string &in)", &ScreenBufferObject::SetSaveDirectory)
        .method("const string &GetSaveDirectory() const", &ScreenBufferObject::GetSaveDirectory)
        .method("void SetSaveFileNamePrefix(const string &in)", &ScreenBufferObject::SetSaveFileNamePrefix)
        .method("const string &GetSaveFileNamePrefix() const", &ScreenBufferObject::GetSaveFileNamePrefix)
        .method("void SetSaveFormat(const string &in)", &ScreenBufferObject::SetSaveFormat)
        .method("const string &GetSaveFormat() const", &ScreenBufferObject::GetSaveFormat)
        .method("bool RequestSave(const string &in filePath = \"\")", &ScreenBufferObject::RequestSave);

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

    RegisterComponentType<AmbientOcclusionEffect>(engine, "AmbientOcclusionEffect")
        .method("float GetRadius() const", [](const AmbientOcclusionEffect &e) { return e.GetParams().radius; })
        .method("void SetRadius(float)", [](AmbientOcclusionEffect &e, float v) { auto p = e.GetParams(); p.radius = v; e.SetParams(p); })
        .method("float GetIntensity() const", [](const AmbientOcclusionEffect &e) { return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](AmbientOcclusionEffect &e, float v) { auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("float GetPower() const", [](const AmbientOcclusionEffect &e) { return e.GetParams().power; })
        .method("void SetPower(float)", [](AmbientOcclusionEffect &e, float v) { auto p = e.GetParams(); p.power = v; e.SetParams(p); })
        .method("float GetBias() const", [](const AmbientOcclusionEffect &e) { return e.GetParams().bias; })
        .method("void SetBias(float)", [](AmbientOcclusionEffect &e, float v) { auto p = e.GetParams(); p.bias = v; e.SetParams(p); })
        .method("uint GetSampleCount() const", [](const AmbientOcclusionEffect &e) -> uint32_t { return e.GetParams().sampleCount; })
        .method("void SetSampleCount(uint)", [](AmbientOcclusionEffect &e, uint32_t v) { auto p = e.GetParams(); p.sampleCount = v; e.SetParams(p); })
        .method("int GetBlurRadius() const", [](const AmbientOcclusionEffect &e) { return e.GetParams().blurRadius; })
        .method("void SetBlurRadius(int)", [](AmbientOcclusionEffect &e, int v) { auto p = e.GetParams(); p.blurRadius = v; e.SetParams(p); })
        .method("float GetDepthThreshold() const", [](const AmbientOcclusionEffect &e) { return e.GetParams().depthThreshold; })
        .method("void SetDepthThreshold(float)", [](AmbientOcclusionEffect &e, float v) { auto p = e.GetParams(); p.depthThreshold = v; e.SetParams(p); });

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

    RegisterComponentType<DepthOfFieldEffect>(engine, "DepthOfFieldEffect")
        .method("float GetFocusDistance() const", [](const DepthOfFieldEffect &e) { return e.GetParams().focusDistance; })
        .method("void SetFocusDistance(float)", [](DepthOfFieldEffect &e, float v) { auto p = e.GetParams(); p.focusDistance = v; e.SetParams(p); })
        .method("float GetFocusRange() const", [](const DepthOfFieldEffect &e) { return e.GetParams().focusRange; })
        .method("void SetFocusRange(float)", [](DepthOfFieldEffect &e, float v) { auto p = e.GetParams(); p.focusRange = v; e.SetParams(p); })
        .method("float GetNearBlurDistance() const", [](const DepthOfFieldEffect &e) { return e.GetParams().nearBlurDistance; })
        .method("void SetNearBlurDistance(float)", [](DepthOfFieldEffect &e, float v) { auto p = e.GetParams(); p.nearBlurDistance = v; e.SetParams(p); })
        .method("float GetFarBlurDistance() const", [](const DepthOfFieldEffect &e) { return e.GetParams().farBlurDistance; })
        .method("void SetFarBlurDistance(float)", [](DepthOfFieldEffect &e, float v) { auto p = e.GetParams(); p.farBlurDistance = v; e.SetParams(p); })
        .method("float GetMaxBlurRadiusPixels() const", [](const DepthOfFieldEffect &e) { return e.GetParams().maxBlurRadiusPixels; })
        .method("void SetMaxBlurRadiusPixels(float)", [](DepthOfFieldEffect &e, float v) { auto p = e.GetParams(); p.maxBlurRadiusPixels = v; e.SetParams(p); })
        .method("uint GetSampleCount() const", [](const DepthOfFieldEffect &e) -> uint32_t { return e.GetParams().sampleCount; })
        .method("void SetSampleCount(uint)", [](DepthOfFieldEffect &e, uint32_t v) { auto p = e.GetParams(); p.sampleCount = v; e.SetParams(p); })
        .method("int GetDilateRadius() const", [](const DepthOfFieldEffect &e) { return e.GetParams().dilateRadius; })
        .method("void SetDilateRadius(int)", [](DepthOfFieldEffect &e, int v) { auto p = e.GetParams(); p.dilateRadius = v; e.SetParams(p); });

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

    RegisterComponentType<MotionBlurEffect>(engine, "MotionBlurEffect")
        .method("float GetIntensity() const", [](const MotionBlurEffect &e) { return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](MotionBlurEffect &e, float v) { auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("float GetVelocityScale() const", [](const MotionBlurEffect &e) { return e.GetParams().velocityScale; })
        .method("void SetVelocityScale(float)", [](MotionBlurEffect &e, float v) { auto p = e.GetParams(); p.velocityScale = v; e.SetParams(p); })
        .method("float GetMaxBlurPixels() const", [](const MotionBlurEffect &e) { return e.GetParams().maxBlurPixels; })
        .method("void SetMaxBlurPixels(float)", [](MotionBlurEffect &e, float v) { auto p = e.GetParams(); p.maxBlurPixels = v; e.SetParams(p); })
        .method("uint GetSamples() const", [](const MotionBlurEffect &e) -> uint32_t { return e.GetParams().samples; })
        .method("void SetSamples(uint)", [](MotionBlurEffect &e, uint32_t v) { auto p = e.GetParams(); p.samples = v; e.SetParams(p); });

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
        });

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
        .method("void SetTag(const string &in)", &EmptyObject::SetTag)
        .method("Tag GetTag() const", [](const EmptyObject &obj) -> Tag { return obj.GetTag(); })
        .method("const string &GetTagName() const", &EmptyObject::GetTagName)
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
        .property("Object@ otherObject", &ScriptHitInfo::otherObject)
        .property("Collider@ selfCollider", &ScriptHitInfo::selfCollider)
        .property("Collider@ otherCollider", &ScriptHitInfo::otherCollider);

    // ウィンドウメッセージ通知（OnWindowMessage）へ渡す情報
    asbind20::value_class<ScriptWindowMessageInfo>(engine, "WindowMessageInfo")
        .behaviours_by_traits()
        .property("WindowObject@ sourceComponent", &ScriptWindowMessageInfo::sourceComponent)
        .property("uint message", &ScriptWindowMessageInfo::message)
        .property("uint64 wParam", &ScriptWindowMessageInfo::wparam)
        .property("int64 lParam", &ScriptWindowMessageInfo::lparam);

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
        .method("bool RemoveGlobalVariable(const string &in)", &SceneContext::RemoveGlobalSceneVariable)
        // ゲームループの終了要求（エディター実行時は再生停止として扱われる）
        .method("void RequestExitGameLoop()", &SceneContext::RequestExitGameLoop);

    // スクリプト側でコンポーネントの動作を定義するためのインターフェース
    // （ScriptComponentはこのインターフェースを実装したクラスを探して実行する）
    engine->RegisterInterface("ScriptComponentBehavior");

    // エディターツール用スクリプトのインターフェース
    // （EditorToolManagerがEditorToolsフォルダの.asからこのインターフェースを実装したクラスを探して実行する）
    engine->RegisterInterface("EditorTool");
}

//==================================================
// JSON（スクリプトからのjsonファイル保存・読み込み）
//==================================================

/// @brief スクリプト用のJSON値ラッパー（参照カウント式の参照型）
/// @details 内部にJSON（nlohmann::json）を1つ保持する。GetJson/Atは部分木のコピーを持つ
///          新しいインスタンスを返すため、子のJsonへの変更は親へ反映されない
///          （変更後にSetJsonで書き戻すこと）。
class ScriptJsonValue final {
public:
    ScriptJsonValue() = default;
    explicit ScriptJsonValue(JSON value) : data(std::move(value)) {}

    void AddRef() { refCount_.fetch_add(1, std::memory_order_relaxed); }
    void Release() {
        if (refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }

    /// @brief 保持しているJSON値（未設定時はnull。オブジェクトのキー設定/配列へのPushで型が確定する）
    JSON data;

private:
    ~ScriptJsonValue() = default;
    std::atomic<int> refCount_{1};
};

/// @brief std::stringの配列から array<string>@ を構築する（Json::GetKeys用）
CScriptArray *MakeStringArray(const std::vector<std::string> &values) {
    asIScriptContext *context = asGetActiveContext();
    asIScriptEngine *engine = context ? context->GetEngine() : nullptr;
    if (!engine) return nullptr;

    asITypeInfo *arrayType = engine->GetTypeInfoByDecl("array<string>");
    if (!arrayType) return nullptr;

    CScriptArray *array = CScriptArray::Create(arrayType, static_cast<asUINT>(values.size()));
    if (!array) return nullptr;
    for (asUINT i = 0; i < values.size(); ++i) {
        array->SetValue(i, const_cast<std::string *>(&values[i]));
    }
    return array;
}

/// @brief 汎用変換（?&in/?&out）の再帰深度上限（自己参照による無限再帰を防ぐ）
constexpr int kMaxJsonConversionDepth = 16;

bool GenericToJson(asIScriptEngine *engine, const void *ref, int typeId, JSON &out, int depth);
bool JsonToGeneric(asIScriptEngine *engine, const JSON &value, void *ref, int typeId, int depth);

/// @brief ?&in で渡された値をJSONへ変換する
/// @details 対応型: プリミティブ / string / Vector2-4 / Quaternion / Json / array<対応型> / dictionary。
///          array・dictionary・Jsonはハンドルでも実体でも受け付ける
bool GenericToJson(asIScriptEngine *engine, const void *ref, int typeId, JSON &out, int depth) {
    if (!engine || !ref || depth > kMaxJsonConversionDepth) return false;

    // ハンドルで渡された場合は実体へデリファレンスする（nullハンドルはJSONのnullにする）
    if (typeId & asTYPEID_OBJHANDLE) {
        ref = *static_cast<void *const *>(ref);
        if (!ref) {
            out = nullptr;
            return true;
        }
        typeId &= ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST);
    }

    switch (typeId) {
    case asTYPEID_BOOL:   out = *static_cast<const bool *>(ref); return true;
    case asTYPEID_INT8:   out = static_cast<std::int64_t>(*static_cast<const std::int8_t *>(ref)); return true;
    case asTYPEID_INT16:  out = static_cast<std::int64_t>(*static_cast<const std::int16_t *>(ref)); return true;
    case asTYPEID_INT32:  out = static_cast<std::int64_t>(*static_cast<const std::int32_t *>(ref)); return true;
    case asTYPEID_INT64:  out = *static_cast<const std::int64_t *>(ref); return true;
    case asTYPEID_UINT8:  out = static_cast<std::uint64_t>(*static_cast<const std::uint8_t *>(ref)); return true;
    case asTYPEID_UINT16: out = static_cast<std::uint64_t>(*static_cast<const std::uint16_t *>(ref)); return true;
    case asTYPEID_UINT32: out = static_cast<std::uint64_t>(*static_cast<const std::uint32_t *>(ref)); return true;
    case asTYPEID_UINT64: out = *static_cast<const std::uint64_t *>(ref); return true;
    case asTYPEID_FLOAT:  out = *static_cast<const float *>(ref); return true;
    case asTYPEID_DOUBLE: out = *static_cast<const double *>(ref); return true;
    default: break;
    }

    if (typeId == gSceneVariableTypeIds.stringTypeId) { out = *static_cast<const std::string *>(ref); return true; }
    if (typeId == gSceneVariableTypeIds.vector2TypeId) { out = ToJSON(*static_cast<const Vector2 *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.vector3TypeId) { out = ToJSON(*static_cast<const Vector3 *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.vector4TypeId) { out = ToJSON(*static_cast<const Vector4 *>(ref)); return true; }
    if (typeId == gSceneVariableTypeIds.quaternionTypeId) { out = ToJSON(*static_cast<const Quaternion *>(ref)); return true; }

    asITypeInfo *typeInfo = engine->GetTypeInfoById(typeId);
    if (!typeInfo || !typeInfo->GetName()) return false;
    const std::string_view typeName = typeInfo->GetName();

    if (typeName == "Json") {
        out = static_cast<const ScriptJsonValue *>(ref)->data;
        return true;
    }

    if (typeName == "array") {
        // 要素型が対応外の場合は配列全体を失敗にする
        const auto *array = static_cast<const CScriptArray *>(ref);
        const int subTypeId = array->GetElementTypeId();
        JSON jsonArray = JSON::array();
        for (asUINT i = 0; i < array->GetSize(); ++i) {
            JSON element;
            if (!GenericToJson(engine, array->At(i), subTypeId, element, depth + 1)) return false;
            jsonArray.push_back(std::move(element));
        }
        out = std::move(jsonArray);
        return true;
    }

    if (typeName == "dictionary") {
        // 辞書は動的型付けのため、変換できない型の値だけスキップしてオブジェクトを作る
        const auto *dictionary = static_cast<const CScriptDictionary *>(ref);
        JSON jsonObject = JSON::object();
        for (auto it = dictionary->begin(); it != dictionary->end(); ++it) {
            const int valueTypeId = it.GetTypeId();
            JSON element;
            if (valueTypeId == asTYPEID_INT64) {
                asINT64 v = 0;
                if (!it.GetValue(v)) continue;
                element = static_cast<std::int64_t>(v);
            } else if (valueTypeId == asTYPEID_DOUBLE) {
                double v = 0.0;
                if (!it.GetValue(v)) continue;
                element = v;
            } else if (valueTypeId == asTYPEID_BOOL) {
                bool v = false;
                if (!it.GetValue(&v, asTYPEID_BOOL)) continue;
                element = v;
            } else {
                // オブジェクト型（string/数学型/array/dictionary/Json）。
                // GetAddressOfValue は非ハンドルなら実体、ハンドルならハンドルのアドレスを返す
                if (!GenericToJson(engine, it.GetAddressOfValue(), valueTypeId, element, depth + 1)) continue;
            }
            jsonObject[it.GetKey()] = std::move(element);
        }
        out = std::move(jsonObject);
        return true;
    }

    return false;
}

/// @brief JSON配列から復元先のarray要素型を推定する（dictionaryの値として復元する場合用）
/// @return 推定できた場合はarrayの型情報、混在・空配列などで推定できない場合はnullptr
asITypeInfo *InferArrayTypeForJson(asIScriptEngine *engine, const JSON &jsonArray) {
    if (!jsonArray.is_array() || jsonArray.empty()) return nullptr;

    auto allOf = [&jsonArray](auto predicate) {
        for (const auto &element : jsonArray) {
            if (!predicate(element)) return false;
        }
        return true;
    };

    const char *decl = nullptr;
    if (allOf([](const JSON &v) { return v.is_boolean(); })) decl = "array<bool>";
    else if (allOf([](const JSON &v) { return v.is_number_integer() || v.is_number_unsigned(); })) decl = "array<int64>";
    else if (allOf([](const JSON &v) { return v.is_number(); })) decl = "array<double>";
    else if (allOf([](const JSON &v) { return v.is_string(); })) decl = "array<string>";
    else if (allOf([](const JSON &v) { return v.is_object(); })) decl = "array<dictionary@>";
    if (!decl) return nullptr;
    return engine->GetTypeInfoByDecl(decl);
}

/// @brief JSONの値を ?&out の変数へ書き戻す
/// @details 対応型はGenericToJsonと同じ。array<T>はサイズを合わせてから各要素を書き込み、
///          dictionaryは内容をクリアしてからJSONオブジェクトの値を型に応じて格納する
bool JsonToGeneric(asIScriptEngine *engine, const JSON &value, void *ref, int typeId, int depth) {
    if (!engine || !ref || depth > kMaxJsonConversionDepth) return false;

    // ハンドル変数が渡された場合は実体へ書き込む（nullハンドルはarray/dictionary/Jsonなら生成する）
    if (typeId & asTYPEID_OBJHANDLE) {
        void **handleRef = static_cast<void **>(ref);
        const int bareTypeId = typeId & ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST);
        if (*handleRef == nullptr) {
            asITypeInfo *typeInfo = engine->GetTypeInfoById(bareTypeId);
            if (!typeInfo || !typeInfo->GetName()) return false;
            const std::string_view typeName = typeInfo->GetName();
            void *created = nullptr;
            if (typeName == "array") created = CScriptArray::Create(typeInfo);
            else if (typeName == "dictionary") created = CScriptDictionary::Create(engine);
            else if (typeName == "Json") created = new ScriptJsonValue();
            if (!created) return false;
            *handleRef = created;
        }
        ref = *handleRef;
        typeId = bareTypeId;
    }

    switch (typeId) {
    case asTYPEID_BOOL:
        if (!value.is_boolean()) return false;
        *static_cast<bool *>(ref) = value.get<bool>();
        return true;
    case asTYPEID_INT8: case asTYPEID_INT16: case asTYPEID_INT32: case asTYPEID_INT64:
    case asTYPEID_UINT8: case asTYPEID_UINT16: case asTYPEID_UINT32: case asTYPEID_UINT64:
    case asTYPEID_FLOAT: case asTYPEID_DOUBLE: {
        if (!value.is_number()) return false;
        const double asDouble = value.get<double>();
        const std::int64_t asInt = value.is_number_float() ? static_cast<std::int64_t>(asDouble) : value.get<std::int64_t>();
        switch (typeId) {
        case asTYPEID_INT8:   *static_cast<std::int8_t *>(ref) = static_cast<std::int8_t>(asInt); break;
        case asTYPEID_INT16:  *static_cast<std::int16_t *>(ref) = static_cast<std::int16_t>(asInt); break;
        case asTYPEID_INT32:  *static_cast<std::int32_t *>(ref) = static_cast<std::int32_t>(asInt); break;
        case asTYPEID_INT64:  *static_cast<std::int64_t *>(ref) = asInt; break;
        case asTYPEID_UINT8:  *static_cast<std::uint8_t *>(ref) = static_cast<std::uint8_t>(asInt); break;
        case asTYPEID_UINT16: *static_cast<std::uint16_t *>(ref) = static_cast<std::uint16_t>(asInt); break;
        case asTYPEID_UINT32: *static_cast<std::uint32_t *>(ref) = static_cast<std::uint32_t>(asInt); break;
        case asTYPEID_UINT64: *static_cast<std::uint64_t *>(ref) = static_cast<std::uint64_t>(asInt); break;
        case asTYPEID_FLOAT:  *static_cast<float *>(ref) = static_cast<float>(asDouble); break;
        case asTYPEID_DOUBLE: *static_cast<double *>(ref) = asDouble; break;
        default: break;
        }
        return true;
    }
    default: break;
    }

    try {
        if (typeId == gSceneVariableTypeIds.stringTypeId) {
            if (!value.is_string()) return false;
            *static_cast<std::string *>(ref) = value.get<std::string>();
            return true;
        }
        if (typeId == gSceneVariableTypeIds.vector2TypeId) { *static_cast<Vector2 *>(ref) = FromJSON<Vector2>(value); return true; }
        if (typeId == gSceneVariableTypeIds.vector3TypeId) { *static_cast<Vector3 *>(ref) = FromJSON<Vector3>(value); return true; }
        if (typeId == gSceneVariableTypeIds.vector4TypeId) { *static_cast<Vector4 *>(ref) = FromJSON<Vector4>(value); return true; }
        if (typeId == gSceneVariableTypeIds.quaternionTypeId) { *static_cast<Quaternion *>(ref) = FromJSON<Quaternion>(value); return true; }
    } catch (const std::exception &) {
        return false;
    }

    asITypeInfo *typeInfo = engine->GetTypeInfoById(typeId);
    if (!typeInfo || !typeInfo->GetName()) return false;
    const std::string_view typeName = typeInfo->GetName();

    if (typeName == "Json") {
        static_cast<ScriptJsonValue *>(ref)->data = value;
        return true;
    }

    if (typeName == "array") {
        if (!value.is_array()) return false;
        auto *array = static_cast<CScriptArray *>(ref);
        const int subTypeId = array->GetElementTypeId();
        array->Resize(static_cast<asUINT>(value.size()));
        for (asUINT i = 0; i < array->GetSize(); ++i) {
            if (!JsonToGeneric(engine, value[i], array->At(i), subTypeId, depth + 1)) return false;
        }
        return true;
    }

    if (typeName == "dictionary") {
        if (!value.is_object()) return false;
        auto *dictionary = static_cast<CScriptDictionary *>(ref);
        dictionary->DeleteAll();
        const int dictionaryTypeId = typeInfo->GetTypeId();
        for (auto it = value.begin(); it != value.end(); ++it) {
            const JSON &element = it.value();
            if (element.is_boolean()) {
                bool v = element.get<bool>();
                dictionary->Set(it.key(), &v, asTYPEID_BOOL);
            } else if (element.is_number_integer() || element.is_number_unsigned()) {
                const asINT64 v = element.get<std::int64_t>();
                dictionary->Set(it.key(), v);
            } else if (element.is_number_float()) {
                const double v = element.get<double>();
                dictionary->Set(it.key(), v);
            } else if (element.is_string()) {
                std::string v = element.get<std::string>();
                dictionary->Set(it.key(), &v, gSceneVariableTypeIds.stringTypeId);
            } else if (element.is_object()) {
                // ネストしたオブジェクトは辞書として復元する
                // （数学型として保存されたものも辞書になる点に注意。Setはハンドルをaddrefするため生成分を手放す）
                CScriptDictionary *child = CScriptDictionary::Create(engine);
                if (child) {
                    if (JsonToGeneric(engine, element, child, dictionaryTypeId, depth + 1)) {
                        void *handle = child;
                        dictionary->Set(it.key(), &handle, dictionaryTypeId | asTYPEID_OBJHANDLE);
                    }
                    child->Release();
                }
            } else if (element.is_array()) {
                // 要素型を推定できた配列のみ復元する（混在型・空配列はスキップ）
                asITypeInfo *arrayType = InferArrayTypeForJson(engine, element);
                if (arrayType) {
                    CScriptArray *child = CScriptArray::Create(arrayType);
                    if (child) {
                        if (JsonToGeneric(engine, element, child, arrayType->GetTypeId(), depth + 1)) {
                            void *handle = child;
                            dictionary->Set(it.key(), &handle, arrayType->GetTypeId() | asTYPEID_OBJHANDLE);
                        }
                        child->Release();
                    }
                }
            }
            // nullはスキップ（dictionaryにnullの表現が無いため）
        }
        return true;
    }

    return false;
}

/// @brief Json型（参照型）とファイル入出力のグローバル関数を登録する
void RegisterJsonBindings(asIScriptEngine *engine) {
    asbind20::ref_class<ScriptJsonValue>(engine, "Json")
        .default_factory()
        .addref(&ScriptJsonValue::AddRef)
        .release(&ScriptJsonValue::Release)
        // 型判定
        .method("bool IsNull() const", [](const ScriptJsonValue &j) -> bool { return j.data.is_null(); })
        .method("bool IsObject() const", [](const ScriptJsonValue &j) -> bool { return j.data.is_object(); })
        .method("bool IsArray() const", [](const ScriptJsonValue &j) -> bool { return j.data.is_array(); })
        .method("bool IsString() const", [](const ScriptJsonValue &j) -> bool { return j.data.is_string(); })
        .method("bool IsNumber() const", [](const ScriptJsonValue &j) -> bool { return j.data.is_number(); })
        .method("bool IsBool() const", [](const ScriptJsonValue &j) -> bool { return j.data.is_boolean(); })
        // オブジェクト操作
        .method("bool Has(const string &in key) const", [](const ScriptJsonValue &j, const std::string &key) -> bool {
            return j.data.is_object() && j.data.contains(key);
        })
        .method("bool Remove(const string &in key)", [](ScriptJsonValue &j, const std::string &key) -> bool {
            if (!j.data.is_object()) return false;
            return j.data.erase(key) > 0;
        })
        .method("void Clear()", [](ScriptJsonValue &j) { j.data = JSON(); })
        .method("array<string>@ GetKeys() const", [](const ScriptJsonValue &j) -> CScriptArray * {
            std::vector<std::string> keys;
            if (j.data.is_object()) {
                for (auto it = j.data.begin(); it != j.data.end(); ++it) keys.push_back(it.key());
            }
            return MakeStringArray(keys);
        })
        // 値の設定（オブジェクトのキーへ設定。null状態から呼ぶとオブジェクトになる）
        .method("void SetBool(const string &in key, bool value)", [](ScriptJsonValue &j, const std::string &key, bool v) {
            try { j.data[key] = v; } catch (const std::exception &) {}
        })
        .method("void SetInt(const string &in key, int64 value)", [](ScriptJsonValue &j, const std::string &key, std::int64_t v) {
            try { j.data[key] = v; } catch (const std::exception &) {}
        })
        .method("void SetFloat(const string &in key, double value)", [](ScriptJsonValue &j, const std::string &key, double v) {
            try { j.data[key] = v; } catch (const std::exception &) {}
        })
        .method("void SetString(const string &in key, const string &in value)", [](ScriptJsonValue &j, const std::string &key, const std::string &v) {
            try { j.data[key] = v; } catch (const std::exception &) {}
        })
        .method("void SetVector2(const string &in key, const Vector2 &in value)", [](ScriptJsonValue &j, const std::string &key, const Vector2 &v) {
            try { j.data[key] = ToJSON(v); } catch (const std::exception &) {}
        })
        .method("void SetVector3(const string &in key, const Vector3 &in value)", [](ScriptJsonValue &j, const std::string &key, const Vector3 &v) {
            try { j.data[key] = ToJSON(v); } catch (const std::exception &) {}
        })
        .method("void SetVector4(const string &in key, const Vector4 &in value)", [](ScriptJsonValue &j, const std::string &key, const Vector4 &v) {
            try { j.data[key] = ToJSON(v); } catch (const std::exception &) {}
        })
        .method("void SetQuaternion(const string &in key, const Quaternion &in value)", [](ScriptJsonValue &j, const std::string &key, const Quaternion &v) {
            try { j.data[key] = ToJSON(v); } catch (const std::exception &) {}
        })
        .method("void SetJson(const string &in key, const Json &in value)", [](ScriptJsonValue &j, const std::string &key, const ScriptJsonValue &v) {
            try { j.data[key] = v.data; } catch (const std::exception &) {}
        })
        .method("void SetNull(const string &in key)", [](ScriptJsonValue &j, const std::string &key) {
            try { j.data[key] = nullptr; } catch (const std::exception &) {}
        })
        // 汎用のSet/Get（array<T>・dictionaryを含む全対応型を型に応じて変換する）
        .method("bool Set(const string &in key, const ?&in value)", [](ScriptJsonValue &j, const std::string &key, void *ref, int typeId) -> bool {
            asIScriptContext *context = asGetActiveContext();
            if (!context) return false;
            JSON converted;
            if (!GenericToJson(context->GetEngine(), ref, typeId, converted, 0)) return false;
            try { j.data[key] = std::move(converted); } catch (const std::exception &) { return false; }
            return true;
        })
        .method("bool Get(const string &in key, ?&out value) const", [](const ScriptJsonValue &j, const std::string &key, void *ref, int typeId) -> bool {
            if (!j.data.is_object() || !j.data.contains(key)) return false;
            asIScriptContext *context = asGetActiveContext();
            if (!context) return false;
            try { return JsonToGeneric(context->GetEngine(), j.data[key], ref, typeId, 0); } catch (const std::exception &) { return false; }
        })
        // 値の取得（キーが無い/型が合わない場合はデフォルト値を返す）
        .method("bool GetBool(const string &in key, bool defaultValue = false) const", [](const ScriptJsonValue &j, const std::string &key, bool def) -> bool {
            if (j.data.is_object() && j.data.contains(key) && j.data[key].is_boolean()) return j.data[key].get<bool>();
            return def;
        })
        .method("int64 GetInt(const string &in key, int64 defaultValue = 0) const", [](const ScriptJsonValue &j, const std::string &key, std::int64_t def) -> std::int64_t {
            if (j.data.is_object() && j.data.contains(key) && j.data[key].is_number()) return j.data[key].get<std::int64_t>();
            return def;
        })
        .method("double GetFloat(const string &in key, double defaultValue = 0) const", [](const ScriptJsonValue &j, const std::string &key, double def) -> double {
            if (j.data.is_object() && j.data.contains(key) && j.data[key].is_number()) return j.data[key].get<double>();
            return def;
        })
        .method("string GetString(const string &in key, const string &in defaultValue = \"\") const", [](const ScriptJsonValue &j, const std::string &key, const std::string &def) -> std::string {
            if (j.data.is_object() && j.data.contains(key) && j.data[key].is_string()) return j.data[key].get<std::string>();
            return def;
        })
        .method("Vector2 GetVector2(const string &in key, const Vector2 &in defaultValue = Vector2()) const", [](const ScriptJsonValue &j, const std::string &key, const Vector2 &def) -> Vector2 {
            try { if (j.data.is_object() && j.data.contains(key)) return FromJSON<Vector2>(j.data[key]); } catch (const std::exception &) {}
            return def;
        })
        .method("Vector3 GetVector3(const string &in key, const Vector3 &in defaultValue = Vector3()) const", [](const ScriptJsonValue &j, const std::string &key, const Vector3 &def) -> Vector3 {
            try { if (j.data.is_object() && j.data.contains(key)) return FromJSON<Vector3>(j.data[key]); } catch (const std::exception &) {}
            return def;
        })
        .method("Vector4 GetVector4(const string &in key, const Vector4 &in defaultValue = Vector4()) const", [](const ScriptJsonValue &j, const std::string &key, const Vector4 &def) -> Vector4 {
            try { if (j.data.is_object() && j.data.contains(key)) return FromJSON<Vector4>(j.data[key]); } catch (const std::exception &) {}
            return def;
        })
        .method("Quaternion GetQuaternion(const string &in key, const Quaternion &in defaultValue = Quaternion()) const", [](const ScriptJsonValue &j, const std::string &key, const Quaternion &def) -> Quaternion {
            try { if (j.data.is_object() && j.data.contains(key)) return FromJSON<Quaternion>(j.data[key]); } catch (const std::exception &) {}
            return def;
        })
        .method("Json@ GetJson(const string &in key) const", [](const ScriptJsonValue &j, const std::string &key) -> ScriptJsonValue * {
            if (!j.data.is_object() || !j.data.contains(key)) return nullptr;
            return new ScriptJsonValue(j.data[key]);
        })
        // 配列操作（null状態からPushすると配列になる）
        .method("uint Size() const", [](const ScriptJsonValue &j) -> asUINT { return static_cast<asUINT>(j.data.size()); })
        .method("Json@ At(uint index) const", [](const ScriptJsonValue &j, asUINT index) -> ScriptJsonValue * {
            if (!j.data.is_array() || index >= j.data.size()) return nullptr;
            return new ScriptJsonValue(j.data[index]);
        })
        .method("void PushBool(bool value)", [](ScriptJsonValue &j, bool v) {
            try { j.data.push_back(v); } catch (const std::exception &) {}
        })
        .method("void PushInt(int64 value)", [](ScriptJsonValue &j, std::int64_t v) {
            try { j.data.push_back(v); } catch (const std::exception &) {}
        })
        .method("void PushFloat(double value)", [](ScriptJsonValue &j, double v) {
            try { j.data.push_back(v); } catch (const std::exception &) {}
        })
        .method("void PushString(const string &in value)", [](ScriptJsonValue &j, const std::string &v) {
            try { j.data.push_back(v); } catch (const std::exception &) {}
        })
        .method("void PushJson(const Json &in value)", [](ScriptJsonValue &j, const ScriptJsonValue &v) {
            try { j.data.push_back(v.data); } catch (const std::exception &) {}
        })
        // 汎用のPush（array<T>・dictionaryを含む全対応型を型に応じて変換して配列へ追加する）
        .method("bool Push(const ?&in value)", [](ScriptJsonValue &j, void *ref, int typeId) -> bool {
            asIScriptContext *context = asGetActiveContext();
            if (!context) return false;
            JSON converted;
            if (!GenericToJson(context->GetEngine(), ref, typeId, converted, 0)) return false;
            try { j.data.push_back(std::move(converted)); } catch (const std::exception &) { return false; }
            return true;
        })
        // 直接値の取得（this自身が数値・文字列等の場合。Atで取り出した配列要素向け）
        .method("bool AsBool(bool defaultValue = false) const", [](const ScriptJsonValue &j, bool def) -> bool {
            return j.data.is_boolean() ? j.data.get<bool>() : def;
        })
        .method("int64 AsInt(int64 defaultValue = 0) const", [](const ScriptJsonValue &j, std::int64_t def) -> std::int64_t {
            return j.data.is_number() ? j.data.get<std::int64_t>() : def;
        })
        .method("double AsFloat(double defaultValue = 0) const", [](const ScriptJsonValue &j, double def) -> double {
            return j.data.is_number() ? j.data.get<double>() : def;
        })
        .method("string AsString(const string &in defaultValue = \"\") const", [](const ScriptJsonValue &j, const std::string &def) -> std::string {
            return j.data.is_string() ? j.data.get<std::string>() : def;
        })
        // 文字列化・パース
        .method("string ToString(int indent = -1) const", [](const ScriptJsonValue &j, int indent) -> std::string {
            return j.data.dump(indent, ' ', false, JSON::error_handler_t::replace);
        })
        .method("bool Parse(const string &in text)", [](ScriptJsonValue &j, const std::string &text) -> bool {
            j.data = JSON::parse(text, nullptr, false);
            if (j.data.is_discarded()) {
                j.data = JSON();
                return false;
            }
            return true;
        });

    asbind20::global(engine)
        .function("Json@ LoadJsonFile(const string &in path)", [](const std::string &path) -> ScriptJsonValue * {
            JSON data = LoadJSON(path);
            if (data.is_discarded()) return nullptr;
            return new ScriptJsonValue(std::move(data));
        })
        .function("bool SaveJsonFile(const string &in path, const Json &in data, int indent = 4)", [](const std::string &path, const ScriptJsonValue &data, int indent) -> bool {
            return SaveJSON(data.data, path, indent);
        });
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
// 数学ユーティリティ（Utilities/MathUtils.h）
//==================================================

/// @brief 角度変換・円周率・Clampをグローバル関数として登録する
void RegisterMathUtilBindings(asIScriptEngine *engine) {
    asbind20::global(engine)
        .function("float GetPI()", &GetPI<float>)
        .function("float ToDegrees(float)", &ToDegrees<float>)
        .function("Vector2 ToDegrees(const Vector2 &in)", &ToDegrees<Vector2>)
        .function("Vector3 ToDegrees(const Vector3 &in)", &ToDegrees<Vector3>)
        .function("Vector4 ToDegrees(const Vector4 &in)", &ToDegrees<Vector4>)
        .function("float ToRadians(float)", &ToRadians<float>)
        .function("Vector2 ToRadians(const Vector2 &in)", &ToRadians<Vector2>)
        .function("Vector3 ToRadians(const Vector3 &in)", &ToRadians<Vector3>)
        .function("Vector4 ToRadians(const Vector4 &in)", &ToRadians<Vector4>)
        .function("float Clamp(float, float, float)",
            static_cast<float (*)(float, float, float)>(&Clamp))
        .function("Vector2 Clamp(const Vector2 &in, const Vector2 &in, const Vector2 &in)",
            static_cast<Vector2 (*)(const Vector2 &, const Vector2 &, const Vector2 &)>(&Clamp))
        .function("Vector3 Clamp(const Vector3 &in, const Vector3 &in, const Vector3 &in)",
            static_cast<Vector3 (*)(const Vector3 &, const Vector3 &, const Vector3 &)>(&Clamp))
        .function("Vector4 Clamp(const Vector4 &in, const Vector4 &in, const Vector4 &in)",
            static_cast<Vector4 (*)(const Vector4 &, const Vector4 &, const Vector4 &)>(&Clamp));
}

//==================================================
// 数学関数（<cmath>のラッパー）
//==================================================

/// @brief abs/sqrt/pow/三角関数など、標準的な数学関数をグローバル関数として登録する
void RegisterMathFunctionBindings(asIScriptEngine *engine) {
    asbind20::global(engine)
        .function("float Abs(float)", [](float value) -> float { return std::abs(value); })
        .function("int Abs(int)", [](int value) -> int { return std::abs(value); })
        .function("float Sqrt(float)", [](float value) -> float { return std::sqrt(value); })
        .function("float Pow(float, float)", [](float base, float exponent) -> float { return std::pow(base, exponent); })
        .function("float Floor(float)", [](float value) -> float { return std::floor(value); })
        .function("float Ceil(float)", [](float value) -> float { return std::ceil(value); })
        .function("float Round(float)", [](float value) -> float { return std::round(value); })
        .function("float Sign(float)", [](float value) -> float { return static_cast<float>((value > 0.0f) - (value < 0.0f)); })
        .function("float Min(float, float)", [](float a, float b) -> float { return std::min(a, b); })
        .function("float Max(float, float)", [](float a, float b) -> float { return std::max(a, b); })
        .function("int Min(int, int)", [](int a, int b) -> int { return std::min(a, b); })
        .function("int Max(int, int)", [](int a, int b) -> int { return std::max(a, b); })
        .function("float Sin(float)", [](float radians) -> float { return std::sin(radians); })
        .function("float Cos(float)", [](float radians) -> float { return std::cos(radians); })
        .function("float Tan(float)", [](float radians) -> float { return std::tan(radians); })
        .function("float Asin(float)", [](float value) -> float { return std::asin(value); })
        .function("float Acos(float)", [](float value) -> float { return std::acos(value); })
        .function("float Atan(float)", [](float value) -> float { return std::atan(value); })
        .function("float Atan2(float, float)", [](float y, float x) -> float { return std::atan2(y, x); })
        .function("float Exp(float)", [](float value) -> float { return std::exp(value); })
        .function("float Ln(float)", [](float value) -> float { return std::log(value); })
        .function("float Log10(float)", [](float value) -> float { return std::log10(value); });
}

//==================================================
// 波動関数崩壊アルゴリズム（Utilities/WaveFunctionCollapse.h）
//==================================================

/// @brief スクリプト用のWaveFunctionCollapseラッパー（参照カウント式の参照型）
/// @details 内部にWaveFunctionCollapseを1つ保持するだけの薄いラッパー。C++側のクラス自体には
///          参照カウントを持たせず、スクリプトから生成・保持できるようにするためだけにここで包む
class ScriptWaveFunctionCollapse final {
public:
    ScriptWaveFunctionCollapse() = default;

    void AddRef() { refCount_.fetch_add(1, std::memory_order_relaxed); }
    void Release() {
        if (refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }

    WaveFunctionCollapse data;

private:
    ~ScriptWaveFunctionCollapse() = default;
    std::atomic<int> refCount_{1};
};

/// @brief WFCDirection列挙とWaveFunctionCollapse型（参照型）を登録する
/// @details Direction引数はAngelScriptの列挙（4バイト int）とC++側の
///          enum class Direction（1バイト uint8_t）でサイズが異なるため、
///          ネイティブ呼び出し規約での直接受け渡しはできない。そのためラムダ側は
///          uintで受け取り、内部でDirectionへキャストしてから本体を呼び出す
void RegisterWaveFunctionCollapseBindings(asIScriptEngine *engine) {
    engine->RegisterEnum("WFCDirection");
    engine->RegisterEnumValue("WFCDirection", "Up", static_cast<int>(WaveFunctionCollapse::Direction::Up));
    engine->RegisterEnumValue("WFCDirection", "Down", static_cast<int>(WaveFunctionCollapse::Direction::Down));
    engine->RegisterEnumValue("WFCDirection", "Left", static_cast<int>(WaveFunctionCollapse::Direction::Left));
    engine->RegisterEnumValue("WFCDirection", "Right", static_cast<int>(WaveFunctionCollapse::Direction::Right));
    engine->RegisterEnumValue("WFCDirection", "Front", static_cast<int>(WaveFunctionCollapse::Direction::Front));
    engine->RegisterEnumValue("WFCDirection", "Back", static_cast<int>(WaveFunctionCollapse::Direction::Back));

    asbind20::ref_class<ScriptWaveFunctionCollapse>(engine, "WaveFunctionCollapse")
        .default_factory()
        .addref(&ScriptWaveFunctionCollapse::AddRef)
        .release(&ScriptWaveFunctionCollapse::Release)
        .method("void SetSeed(uint seed)", [](ScriptWaveFunctionCollapse &self, std::uint32_t seed) {
            self.data.SetSeed(seed);
        })
        .method("uint GetSeed() const", [](const ScriptWaveFunctionCollapse &self) -> std::uint32_t {
            return self.data.GetSeed();
        })
        .method("void SetGridSize(uint width, uint height, uint depth)",
            [](ScriptWaveFunctionCollapse &self, std::uint32_t width, std::uint32_t height, std::uint32_t depth) {
                self.data.SetGridSize(width, height, depth);
            })
        .method("uint GetGridWidth() const", [](const ScriptWaveFunctionCollapse &self) -> std::uint32_t {
            return self.data.GetGridWidth();
        })
        .method("uint GetGridHeight() const", [](const ScriptWaveFunctionCollapse &self) -> std::uint32_t {
            return self.data.GetGridHeight();
        })
        .method("uint GetGridDepth() const", [](const ScriptWaveFunctionCollapse &self) -> std::uint32_t {
            return self.data.GetGridDepth();
        })
        .method("bool RegisterTile(const string &in name)", [](ScriptWaveFunctionCollapse &self, const std::string &name) -> bool {
            return self.data.RegisterTile(name);
        })
        .method("bool RemoveTile(const string &in tileName)", [](ScriptWaveFunctionCollapse &self, const std::string &tileName) -> bool {
            return self.data.RemoveTile(tileName);
        })
        .method("bool AddTileConnection(const string &in tileName, WFCDirection direction, const string &in connectedTileName)",
            [](ScriptWaveFunctionCollapse &self, const std::string &tileName, std::uint32_t direction, const std::string &connectedTileName) -> bool {
                return self.data.AddTileConnection(tileName, static_cast<WaveFunctionCollapse::Direction>(direction), connectedTileName);
            })
        .method("bool FixTile(uint x, uint y, uint z, const string &in tileName)",
            [](ScriptWaveFunctionCollapse &self, std::uint32_t x, std::uint32_t y, std::uint32_t z, const std::string &tileName) -> bool {
                return self.data.FixTile(x, y, z, tileName);
            })
        .method("bool TryGetFixedTile(uint x, uint y, uint z, string &out tileName) const",
            [](const ScriptWaveFunctionCollapse &self, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::string &tileName) -> bool {
                auto fixed = self.data.GetFixedTile(x, y, z);
                if (!fixed) return false;
                tileName = *fixed;
                return true;
            })
        .method("bool SetStartPosition(uint x, uint y, uint z)",
            [](ScriptWaveFunctionCollapse &self, std::uint32_t x, std::uint32_t y, std::uint32_t z) -> bool {
                return self.data.SetStartPosition(x, y, z);
            })
        .method("bool TryGetStartPosition(uint &out x, uint &out y, uint &out z) const",
            [](const ScriptWaveFunctionCollapse &self, std::uint32_t &x, std::uint32_t &y, std::uint32_t &z) -> bool {
                auto position = self.data.GetStartPosition();
                if (!position) return false;
                x = (*position)[0];
                y = (*position)[1];
                z = (*position)[2];
                return true;
            })
        .method("bool Solve()", [](ScriptWaveFunctionCollapse &self) -> bool { return self.data.Solve(); })
        .method("bool TryGetResolvedTile(uint x, uint y, uint z, string &out tileName) const",
            [](const ScriptWaveFunctionCollapse &self, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::string &tileName) -> bool {
                auto resolved = self.data.GetResolvedTile(x, y, z);
                if (!resolved) return false;
                tileName = *resolved;
                return true;
            })
        .method("Json@ SaveToJson() const", [](const ScriptWaveFunctionCollapse &self) -> ScriptJsonValue * {
            return new ScriptJsonValue(self.data.SaveToJson());
        })
        .method("bool LoadFromJson(const Json &in json)", [](ScriptWaveFunctionCollapse &self, const ScriptJsonValue &json) -> bool {
            return self.data.LoadFromJson(json.data);
        });
}

//==================================================
// ステージ生成（Utilities/StageGraphGenerator.h, Utilities/StageGridBuilder.h）
//==================================================

/// @brief スクリプト用のStageGraphGeneratorラッパー（参照カウント式の参照型）
class ScriptStageGraphGenerator final {
public:
    ScriptStageGraphGenerator() = default;

    void AddRef() { refCount_.fetch_add(1, std::memory_order_relaxed); }
    void Release() {
        if (refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }

    StageGraphGenerator data;

private:
    ~ScriptStageGraphGenerator() = default;
    std::atomic<int> refCount_{1};
};

/// @brief スクリプト用のStageGridBuilderラッパー（参照カウント式の参照型）
class ScriptStageGridBuilder final {
public:
    ScriptStageGridBuilder() = default;

    void AddRef() { refCount_.fetch_add(1, std::memory_order_relaxed); }
    void Release() {
        if (refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }

    StageGridBuilder data;

private:
    ~ScriptStageGridBuilder() = default;
    std::atomic<int> refCount_{1};
};

/// @brief RoomType列挙、StageGraphGenerator型、StageGridBuilder型を登録する
/// @details WaveFunctionCollapseと同じ理由（列挙のサイズ差）で、RoomType引数を取るメソッドは
///          ラムダ側でuintとして受け取り、内部でRoomTypeへキャストする
void RegisterStageGenerationBindings(asIScriptEngine *engine) {
    engine->RegisterEnum("RoomType");
    engine->RegisterEnumValue("RoomType", "Start", static_cast<int>(RoomType::Start));
    engine->RegisterEnumValue("RoomType", "Goal", static_cast<int>(RoomType::Goal));
    engine->RegisterEnumValue("RoomType", "Normal", static_cast<int>(RoomType::Normal));
    engine->RegisterEnumValue("RoomType", "Branch", static_cast<int>(RoomType::Branch));
    engine->RegisterEnumValue("RoomType", "Building", static_cast<int>(RoomType::Building));
    engine->RegisterEnumValue("RoomType", "GimmickDepth", static_cast<int>(RoomType::GimmickDepth));
    engine->RegisterEnumValue("RoomType", "Treasure", static_cast<int>(RoomType::Treasure));

    asbind20::ref_class<ScriptStageGraphGenerator>(engine, "StageGraphGenerator")
        .default_factory()
        .addref(&ScriptStageGraphGenerator::AddRef)
        .release(&ScriptStageGraphGenerator::Release)
        .method("void SetSeed(uint seed)", [](ScriptStageGraphGenerator &self, std::uint32_t seed) {
            self.data.SetSeed(seed);
        })
        .method("uint GetSeed() const", [](const ScriptStageGraphGenerator &self) -> std::uint32_t {
            return self.data.GetSeed();
        })
        .method("void SetGridSize(uint width, uint height, uint depth)",
            [](ScriptStageGraphGenerator &self, std::uint32_t width, std::uint32_t height, std::uint32_t depth) {
                self.data.SetGridSize(width, height, depth);
            })
        .method("uint GetGridWidth() const", [](const ScriptStageGraphGenerator &self) -> std::uint32_t { return self.data.GetGridWidth(); })
        .method("uint GetGridHeight() const", [](const ScriptStageGraphGenerator &self) -> std::uint32_t { return self.data.GetGridHeight(); })
        .method("uint GetGridDepth() const", [](const ScriptStageGraphGenerator &self) -> std::uint32_t { return self.data.GetGridDepth(); })
        .method("void SetBranchProbability(float probability)", [](ScriptStageGraphGenerator &self, float probability) {
            self.data.SetBranchProbability(probability);
        })
        .method("float GetBranchProbability() const", [](const ScriptStageGraphGenerator &self) -> float {
            return self.data.GetBranchProbability();
        })
        .method("bool AddSideRoomType(RoomType type, float weight)",
            [](ScriptStageGraphGenerator &self, std::uint32_t type, float weight) -> bool {
                return self.data.AddSideRoomType(static_cast<RoomType>(type), weight);
            })
        .method("void ClearSideRoomTypes()", [](ScriptStageGraphGenerator &self) { self.data.ClearSideRoomTypes(); })
        .method("void Generate()", [](ScriptStageGraphGenerator &self) { self.data.Generate(); })
        .method("uint GetRoomCount() const", [](const ScriptStageGraphGenerator &self) -> std::uint32_t {
            return static_cast<std::uint32_t>(self.data.GetRoomCount());
        })
        .method("uint GetRoomID(uint index) const", [](const ScriptStageGraphGenerator &self, std::uint32_t index) -> std::uint32_t {
            const RoomNode *room = self.data.GetRoomByIndex(index);
            return room ? room->id : 0;
        })
        .method("RoomType GetRoomType(uint index) const", [](const ScriptStageGraphGenerator &self, std::uint32_t index) -> std::uint32_t {
            const RoomNode *room = self.data.GetRoomByIndex(index);
            return static_cast<std::uint32_t>(room ? room->type : RoomType::Branch);
        })
        .method("uint GetRoomX(uint index) const", [](const ScriptStageGraphGenerator &self, std::uint32_t index) -> std::uint32_t {
            const RoomNode *room = self.data.GetRoomByIndex(index);
            return room ? room->x : 0;
        })
        .method("uint GetRoomY(uint index) const", [](const ScriptStageGraphGenerator &self, std::uint32_t index) -> std::uint32_t {
            const RoomNode *room = self.data.GetRoomByIndex(index);
            return room ? room->y : 0;
        })
        .method("uint GetRoomZ(uint index) const", [](const ScriptStageGraphGenerator &self, std::uint32_t index) -> std::uint32_t {
            const RoomNode *room = self.data.GetRoomByIndex(index);
            return room ? room->z : 0;
        })
        .method("uint GetRoomConnectionCount(uint index) const", [](const ScriptStageGraphGenerator &self, std::uint32_t index) -> std::uint32_t {
            const RoomNode *room = self.data.GetRoomByIndex(index);
            return room ? static_cast<std::uint32_t>(room->connectedRoomIDs.size()) : 0;
        })
        .method("uint GetRoomConnectedRoomID(uint index, uint connectionIndex) const",
            [](const ScriptStageGraphGenerator &self, std::uint32_t index, std::uint32_t connectionIndex) -> std::uint32_t {
                const RoomNode *room = self.data.GetRoomByIndex(index);
                if (!room || connectionIndex >= room->connectedRoomIDs.size()) return 0;
                return room->connectedRoomIDs[connectionIndex];
            })
        .method("bool TryGetStartRoomID(uint &out roomID) const", [](const ScriptStageGraphGenerator &self, std::uint32_t &roomID) -> bool {
            auto id = self.data.GetStartRoomID();
            if (!id) return false;
            roomID = *id;
            return true;
        })
        .method("bool TryGetGoalRoomID(uint &out roomID) const", [](const ScriptStageGraphGenerator &self, std::uint32_t &roomID) -> bool {
            auto id = self.data.GetGoalRoomID();
            if (!id) return false;
            roomID = *id;
            return true;
        });

    asbind20::ref_class<ScriptStageGridBuilder>(engine, "StageGridBuilder")
        .default_factory()
        .addref(&ScriptStageGridBuilder::AddRef)
        .release(&ScriptStageGridBuilder::Release)
        .method("void SetRoomSize(uint sizeX, uint sizeY, uint sizeZ)",
            [](ScriptStageGridBuilder &self, std::uint32_t sizeX, std::uint32_t sizeY, std::uint32_t sizeZ) {
                self.data.SetRoomSize(sizeX, sizeY, sizeZ);
            })
        .method("void SetRoomSpacing(uint spacing)", [](ScriptStageGridBuilder &self, std::uint32_t spacing) {
            self.data.SetRoomSpacing(spacing);
        })
        .method("void SetCorridorWidth(uint width)", [](ScriptStageGridBuilder &self, std::uint32_t width) {
            self.data.SetCorridorWidth(width);
        })
        .method("void SetTileWorldSize(float size)", [](ScriptStageGridBuilder &self, float size) {
            self.data.SetTileWorldSize(size);
        })
        .method("void SetRoomTileName(RoomType type, const string &in tileName)",
            [](ScriptStageGridBuilder &self, std::uint32_t type, const std::string &tileName) {
                self.data.SetRoomTileName(static_cast<RoomType>(type), tileName);
            })
        .method("void SetDefaultRoomTileName(const string &in tileName)", [](ScriptStageGridBuilder &self, const std::string &tileName) {
            self.data.SetDefaultRoomTileName(tileName);
        })
        .method("void SetCorridorTileName(const string &in tileName)", [](ScriptStageGridBuilder &self, const std::string &tileName) {
            self.data.SetCorridorTileName(tileName);
        })
        .method("bool Build(const StageGraphGenerator &in graph, WaveFunctionCollapse@ wfc)",
            [](const ScriptStageGridBuilder &self, const ScriptStageGraphGenerator &graph, ScriptWaveFunctionCollapse *wfc) -> bool {
                if (!wfc) return false;
                return self.data.Build(graph.data, wfc->data);
            })
        .method("bool TryGetRoomGridCenter(const StageGraphGenerator &in graph, uint roomID, uint &out x, uint &out y, uint &out z) const",
            [](const ScriptStageGridBuilder &self, const ScriptStageGraphGenerator &graph, std::uint32_t roomID,
                std::uint32_t &x, std::uint32_t &y, std::uint32_t &z) -> bool {
                return self.data.GetRoomGridCenter(graph.data, roomID, x, y, z);
            })
        .method("bool TryGetRoomWorldCenter(const StageGraphGenerator &in graph, uint roomID, Vector3 &out position) const",
            [](const ScriptStageGridBuilder &self, const ScriptStageGraphGenerator &graph, std::uint32_t roomID, Vector3 &position) -> bool {
                return self.data.GetRoomWorldCenter(graph.data, roomID, position);
            })
        .method("void GetRequiredGridSize(const StageGraphGenerator &in graph, uint &out width, uint &out height, uint &out depth) const",
            [](const ScriptStageGridBuilder &self, const ScriptStageGraphGenerator &graph,
                std::uint32_t &width, std::uint32_t &height, std::uint32_t &depth) {
                self.data.GetRequiredGridSize(graph.data, width, height, depth);
            });
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
        // ゲームループの終了要求（エディター実行時は再生停止として扱われる）
        .function("void RequestExitGameLoop()", []() { Scene::RequestExitGameLoop(); })
        // エディターツールのウィンドウ操作（[EditorWindow]で用意されたウィンドウが対象。
        // エディター無効ビルドでは何もしない）
        .function("void OpenEditorWindow(const string &in)", [](const std::string &name) {
#if defined(USE_IMGUI)
            EditorToolManager::GetInstance().OpenWindow(name);
#else
            (void)name;
#endif
        })
        .function("void CloseEditorWindow(const string &in)", [](const std::string &name) {
#if defined(USE_IMGUI)
            EditorToolManager::GetInstance().CloseWindow(name);
#else
            (void)name;
#endif
        })
        .function("bool IsEditorWindowOpen(const string &in)", [](const std::string &name) -> bool {
#if defined(USE_IMGUI)
            return EditorToolManager::GetInstance().IsWindowOpen(name);
#else
            (void)name;
            return false;
#endif
        })
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
        // モデル
        .function("uint GetModelHandleFromAssetPath(const string &in)", &ModelManager::GetModelHandleFromAssetPath)
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
        // objがまだシーン内に存在する有効なオブジェクトかどうかを判定する。
        // Object型はasOBJ_NOCOUNTで参照カウントを持たないため、[SerializeField]等で保持した
        // ハンドルの参照先が後から削除されるとダングリングポインタになり得る。この関数は
        // 渡されたポインタの中身には一切触れず、シーンが保持する生存オブジェクト集合との
        // アドレス一致だけで判定するため、削除済み（解放済みメモリ）を指していても安全に呼べる
        .function("bool IsValidObject(Object@)", [](EmptyObject *obj) -> bool {
            return gCurrentSceneContext && gCurrentSceneContext->GetSceneObject(obj) != nullptr;
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

        // コンストラクタ/ファクトリ（値型のコンストラクタはビヘイビア、参照型のファクトリは別途列挙する）
        const asUINT behaviourCount = typeInfo->GetBehaviourCount();
        for (asUINT b = 0; b < behaviourCount; ++b) {
            asEBehaviours behaviour = asBEHAVE_CONSTRUCT;
            const asIScriptFunction *function = typeInfo->GetBehaviourByIndex(b, &behaviour);
            if (!function) continue;
            if (behaviour != asBEHAVE_CONSTRUCT && behaviour != asBEHAVE_FACTORY) continue;
            const std::string decl = MakeConstructorDeclaration(typeInfo->GetName(), function, (flags & asOBJ_TEMPLATE) != 0);
            if (!decl.empty()) out += "\t" + decl + ";\n";
        }
        const asUINT factoryCount = typeInfo->GetFactoryCount();
        for (asUINT f = 0; f < factoryCount; ++f) {
            const asIScriptFunction *function = typeInfo->GetFactoryByIndex(f);
            if (!function) continue;
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
    // Tag はコンポーネント共通メソッド（GetTag等）が参照するため、コンポーネント登録より先に登録する
    RegisterTagType(engine);
    // Transform は Object::GetTransform() が参照するため、Object/Scene より先に登録する
    RegisterTransformType(engine);
    // Collider（ICollider基底）は HitInfo の selfCollider/otherCollider が参照するため、
    // Object/Scene（HitInfoを含む）より先に登録する
    RegisterColliderBaseType(engine);
    // WindowObject（IWindowObjectComponent基底）は WindowMessageInfo の sourceComponent が参照するため、
    // Object/Scene（WindowMessageInfoを含む）より先に登録する
    RegisterWindowObjectBaseType(engine);
    RegisterWindowMessageConstants(engine);
    // Object/Scene はここで一度に登録する。以降に登録するコンポーネント（MeshRenderer等）は
    // Object@/Scene@ をパラメータ/戻り値として自由に参照できる（型・メソッドとも登録済みのため）
    RegisterObjectTypes(engine);
    RegisterComponentTypes(engine);
    RegisterJsonBindings(engine);
    RegisterEasingBindings(engine);
    RegisterMathUtilBindings(engine);
    RegisterMathFunctionBindings(engine);
    // WaveFunctionCollapseはJson型（SaveToJson/LoadFromJson）を参照するため、Json登録より後に呼ぶ
    RegisterWaveFunctionCollapseBindings(engine);
    // StageGridBuilderはWaveFunctionCollapse型・Vector3型を参照するため、それらの登録より後に呼ぶ
    RegisterStageGenerationBindings(engine);
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
