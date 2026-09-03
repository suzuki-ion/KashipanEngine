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
#include "Core/PlayerSettings.h"
#include "Core/ProjectPaths.h"
#include "Core/Window.h"
#include "Debug/Logger.h"
#include "Debug/ScriptDebugDraw.h"
#include "Input/Input.h"
#include "Input/InputCommand.h"
#include "Input/Mouse.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Objects/EmptyObject.h"
#include "Objects/ObjectContext.h"
#include "Scene/Components/Script/ScriptComponentHandle.h"
#include "Scene/Components/Script/ScriptObjectHandle.h"
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
#include "Utilities/Translation.h"
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
#include "Objects/Components/Collider/MeshButton.h"
#include "Objects/Components/Collider/MeshCollider.h"
#include "Objects/Components/Collider/Ray2DCollider.h"
#include "Objects/Components/Collider/RayCollider.h"
#include "Objects/Components/Collider/RigidBody2D.h"
#include "Objects/Components/Collider/RigidBody3D.h"
#include "Objects/Components/Collider/SphereCollider.h"
#include "Objects/Components/Compute/ComputeShaderProcessing.h"
#include "Objects/Components/GifSource.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/PrefabInstanceComponent.h"
#include "Objects/Components/TextureSource.h"
#include "Objects/Components/VideoSource.h"
#include "Objects/Components/PostProcessing/SSAOEffect.h"
#include "Objects/Components/PostProcessing/GTAOEffect.h"
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
#include "Objects/Components/PostProcessing/ScreenWideDitherBlendEffect.h"
#include "Objects/Components/PostProcessing/TemporalBlendEffect.h"
#include "Objects/Components/PostProcessing/VignetteEffect.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/CameraController.h"
#include "Objects/Components/Render/CameraController2D.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/IWindowObjectComponent.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/ScreenAnchor.h"
#include "Objects/Components/Render/ScreenBufferViewport.h"
#include "Objects/Components/Render/ShadowMapObject.h"
#include "Objects/Components/UI/UIButton.h"
#include "Objects/Components/Render/SkinnedMeshRenderer.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/Render/TilemapRenderer.h"
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
/// @details コンポーネントはScriptComponentHandle<T>（UUID+追加順ID保持の参照カウント式ハンドル）として
///          スクリプトへ渡すため、生ポインタとハンドル(void*)の相互変換を型ごとの関数ポインタで扱う
struct ComponentTypeBinding {
    /// @brief ComponentRegistryへ登録されている型名（CreateObjectComponentByTypeへ渡す）
    std::string engineTypeName;
    IObjectComponent *(*getOne)(EmptyObject &) = nullptr;
    std::vector<IObjectComponent *> (*getAll)(EmptyObject &) = nullptr;
    /// @brief 生ポインタからこの型のScriptComponentHandleを生成する（refcount=1、呼び出し側が所有）
    void *(*wrapAsHandle)(IObjectComponent *) = nullptr;
    /// @brief wrapAsHandleで得たハンドルの所有権を1つ手放す（他で保持されていなければ破棄される）
    void (*releaseHandle)(void *) = nullptr;
    /// @brief ハンドル(void*)を実体へ解決する（削除済み・null時はnullptr）。RemoveComponent用
    IObjectComponent *(*resolveHandle)(void *) = nullptr;
};
std::unordered_map<int, ComponentTypeBinding> gComponentTypeBindings;

/// @brief Object@引数（ScriptObjectHandle）を実体へ解決する
/// @details handleがnull（スクリプトが明示的にnullを渡した）ならそのままnullptrを返し、
///          handleはあるが参照先が既に削除されている場合はAngelScriptの例外を投げてnullptrを返す
EmptyObject *ResolveObjectArg(ScriptObjectHandle *handle) {
    if (!handle) return nullptr;
    EmptyObject *obj = handle->Resolve();
    if (!obj) ThrowDestroyedObjectException();
    return obj;
}

//==================================================
// 数学型
//==================================================

/// @brief array<Vector2>@（制御点列）からVector2::CatmullRomPosition用のstd::vectorへ変換して呼び出す
Vector2 Vector2CatmullRomPosition(CScriptArray *points, float t, bool isLoop) {
    if (!points || points->GetSize() == 0) return Vector2::Zero();
    std::vector<Vector2> buffer(points->GetSize());
    for (asUINT i = 0; i < points->GetSize(); ++i) {
        buffer[i] = *static_cast<const Vector2 *>(points->At(i));
    }
    return Vector2::CatmullRomPosition(buffer, t, isLoop);
}

/// @brief array<Vector3>@（制御点列）からVector3::CatmullRomPosition用のstd::vectorへ変換して呼び出す
Vector3 Vector3CatmullRomPosition(CScriptArray *points, float t, bool isLoop) {
    if (!points || points->GetSize() == 0) return Vector3::Zero();
    std::vector<Vector3> buffer(points->GetSize());
    for (asUINT i = 0; i < points->GetSize(); ++i) {
        buffer[i] = *static_cast<const Vector3 *>(points->At(i));
    }
    return Vector3::CatmullRomPosition(buffer, t, isLoop);
}

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
        .function("Vector2 Bezier(const Vector2 &in, const Vector2 &in, const Vector2 &in, float)", &Vector2::Bezier)
        .function("Vector3 Bezier(const Vector3 &in, const Vector3 &in, const Vector3 &in, float)", &Vector3::Bezier)
        .function("Vector2 CatmullRom(const Vector2 &in, const Vector2 &in, const Vector2 &in, const Vector2 &in, float)", &Vector2::CatmullRomInterpolation)
        .function("Vector3 CatmullRom(const Vector3 &in, const Vector3 &in, const Vector3 &in, const Vector3 &in, float)", &Vector3::CatmullRomInterpolation)
        .function("Vector2 CatmullRomSpline(array<Vector2>@ points, float t, bool isLoop = false)", &Vector2CatmullRomPosition)
        .function("Vector3 CatmullRomSpline(array<Vector3>@ points, float t, bool isLoop = false)", &Vector3CatmullRomPosition)
        .function("Quaternion Slerp(const Quaternion &in, const Quaternion &in, float)", &Quaternion::Slerp)
        .function("Quaternion IdentityQuaternion()", &Quaternion::Identity)
        .function("Quaternion MakeRotateEuler(const Vector3 &in)", &Quaternion::MakeRotateEuler)
        .function("Quaternion MakeRotateAxisAngle(const Vector3 &in, float)", &Quaternion::MakeRotateAxisAngle)
        .function("Quaternion MakeFromRotationMatrix(const Matrix4x4 &in)", &Quaternion::MakeFromRotationMatrix)
        .function("Matrix3x3 IdentityMatrix3x3()", &Matrix3x3::Identity)
        .function("Matrix4x4 IdentityMatrix4x4()", &Matrix4x4::Identity);
}

//==================================================
// デバッグ描画
//==================================================

/// @brief スクリプトからのデバッグ描画関数をDebug名前空間へ登録する
/// @details 描画内容はScriptDebugDrawへ蓄積され、エディターのシーンビューにのみ表示される
///          （ゲーム画面には出ない）。SceneEditorView::UpdateEditorDebugDrawが毎フレーム取り出してクリアする
void RegisterDebugDrawBindings(asIScriptEngine *engine) {
    asbind20::namespace_ debugNamespace(engine, "Debug");
    asbind20::global(engine)
        .function("void DrawLine(const Vector3 &in start, const Vector3 &in end, const Vector4 &in color)", &ScriptDebugDraw::AddLine);
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
/// @details IObjectComponent共通のメソッドも合わせて登録する。戻り値のバインダで型固有のメソッドを追加できる。
///          スクリプトへは生ポインタではなくScriptComponentHandle<T>（UUID+追加順ID保持の参照カウント式
///          ハンドル）として公開し、各メソッドは呼び出しの都度Resolve()で生存確認してから転送する
///          （SafeCall/型固有のラムダの詳細はScriptComponentHandle.h参照）
template <typename T>
auto RegisterComponentType(asIScriptEngine *engine, const char *name) {
    auto binder = asbind20::ref_class<ScriptComponentHandle<T>>(engine, name);
    binder
        .addref(&ScriptComponentHandle<T>::AddRef)
        .release(&ScriptComponentHandle<T>::Release)
        .method("bool IsActive() const", SafeCall<static_cast<bool (T::*)() const>(&T::IsActive)>())
        .method("void SetActive(bool)", SafeCall<static_cast<void (T::*)(bool)>(&T::SetActive)>())
        .method("const string &GetComponentType() const", [](const ScriptComponentHandle<T> &self) -> const std::string & {
            static const std::string kEmpty;
            T *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetComponentType();
        })
        .method("void SetTag(const string &in)", SafeCall<static_cast<void (T::*)(const std::string &)>(&T::SetTag)>())
        .method("Tag GetTag() const", [](const ScriptComponentHandle<T> &self) -> Tag {
            T *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return Tag(); }
            return obj->GetTag();
        })
        .method("const string &GetTagName() const", [](const ScriptComponentHandle<T> &self) -> const std::string & {
            static const std::string kEmpty;
            T *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetTagName();
        });

    const int typeId = engine->GetTypeIdByDecl(name);
    gComponentTypeBindings[typeId] = ComponentTypeBinding{
        name,
        +[](EmptyObject &obj) -> IObjectComponent * { return obj.GetComponent<T>(); },
        +[](EmptyObject &obj) -> std::vector<IObjectComponent *> {
            auto typed = obj.GetComponents<T>();
            return std::vector<IObjectComponent *>(typed.begin(), typed.end());
        },
        +[](IObjectComponent *component) -> void * {
            return ScriptComponentHandle<T>::Create(static_cast<T *>(component));
        },
        +[](void *handlePtr) { if (handlePtr) static_cast<ScriptComponentHandle<T> *>(handlePtr)->Release(); },
        +[](void *handlePtr) -> IObjectComponent * {
            auto *handle = static_cast<ScriptComponentHandle<T> *>(handlePtr);
            return handle ? handle->Resolve() : nullptr;
        },
    };
    return binder;
}

/// @brief ICollider::Shape をスクリプト用の ColliderShape 列挙型として登録する
void RegisterColliderShapeEnum(asIScriptEngine *engine) {
    engine->RegisterEnum("ColliderShape");
    engine->RegisterEnumValue("ColliderShape", "Box", static_cast<int>(ICollider::Shape::Box));
    engine->RegisterEnumValue("ColliderShape", "Sphere", static_cast<int>(ICollider::Shape::Sphere));
    engine->RegisterEnumValue("ColliderShape", "Capsule", static_cast<int>(ICollider::Shape::Capsule));
    engine->RegisterEnumValue("ColliderShape", "Ray", static_cast<int>(ICollider::Shape::Ray));
    engine->RegisterEnumValue("ColliderShape", "Mesh", static_cast<int>(ICollider::Shape::Mesh));
    engine->RegisterEnumValue("ColliderShape", "Ray2D", static_cast<int>(ICollider::Shape::Ray2D));
    engine->RegisterEnumValue("ColliderShape", "Box2D", static_cast<int>(ICollider::Shape::Box2D));
    engine->RegisterEnumValue("ColliderShape", "Circle2D", static_cast<int>(ICollider::Shape::Circle2D));
    engine->RegisterEnumValue("ColliderShape", "Capsule2D", static_cast<int>(ICollider::Shape::Capsule2D));
}

/// @brief Collider（ICollider基底）型を参照型として登録する
/// @details HitInfoのselfCollider/otherColliderで「どのコライダー同士が衝突したか」を
///          受け渡すための共通型。各コライダー型はopImplCastでこの型へ暗黙変換でき、
///          cast<BoxCollider>(hit.otherCollider) のように具体型へダウンキャストもできる。
///          他のコンポーネントと同様、生ポインタではなくScriptComponentHandle<ICollider>
///          （UUID+追加順ID保持の参照カウント式ハンドル）として公開する
void RegisterColliderBaseType(asIScriptEngine *engine) {
    RegisterColliderShapeEnum(engine);
    asbind20::ref_class<ScriptComponentHandle<ICollider>>(engine, "Collider")
        .addref(&ScriptComponentHandle<ICollider>::AddRef)
        .release(&ScriptComponentHandle<ICollider>::Release)
        .method("bool IsActive() const", SafeCall<static_cast<bool (ICollider::*)() const>(&ICollider::IsActive)>())
        .method("void SetActive(bool)", SafeCall<static_cast<void (ICollider::*)(bool)>(&ICollider::SetActive)>())
        .method("const string &GetComponentType() const", [](const ScriptComponentHandle<ICollider> &self) -> const std::string & {
            static const std::string kEmpty;
            ICollider *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetComponentType();
        })
        .method("void SetTag(const string &in)", SafeCall<static_cast<void (ICollider::*)(const std::string &)>(&ICollider::SetTag)>())
        .method("Tag GetTag() const", [](const ScriptComponentHandle<ICollider> &self) -> Tag {
            ICollider *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return Tag(); }
            return obj->GetTag();
        })
        .method("const string &GetTagName() const", [](const ScriptComponentHandle<ICollider> &self) -> const std::string & {
            static const std::string kEmpty;
            ICollider *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetTagName();
        })
        .method("bool IsTrigger() const", SafeCall<static_cast<bool (ICollider::*)() const noexcept>(&ICollider::IsTrigger)>())
        .method("void SetTrigger(bool)", SafeCall<static_cast<void (ICollider::*)(bool) noexcept>(&ICollider::SetTrigger)>())
        .method("bool IsContinuousDetection() const", SafeCall<static_cast<bool (ICollider::*)() const noexcept>(&ICollider::IsContinuousDetection)>())
        .method("void SetContinuousDetection(bool)", SafeCall<static_cast<void (ICollider::*)(bool) noexcept>(&ICollider::SetContinuousDetection)>())
        .method("bool Is2D() const", SafeCall<static_cast<bool (ICollider::*)() const noexcept>(&ICollider::Is2D)>())
        .method("ColliderShape GetShape() const", SafeCall<static_cast<ICollider::Shape (ICollider::*)() const noexcept>(&ICollider::GetShape)>())
        .method("Vector3 GetOwnerWorldPosition() const", SafeCall<static_cast<Vector3 (ICollider::*)() const>(&ICollider::GetOwnerWorldPosition)>())
        .method("bool IsSyncPositionEnabled(int) const", SafeCall<static_cast<bool (ICollider::*)(int) const noexcept>(&ICollider::IsSyncPositionEnabled)>())
        .method("bool IsSyncRotationEnabled() const", SafeCall<static_cast<bool (ICollider::*)() const noexcept>(&ICollider::IsSyncRotationEnabled)>())
        .method("bool IsSyncScaleEnabled(int) const", SafeCall<static_cast<bool (ICollider::*)(int) const noexcept>(&ICollider::IsSyncScaleEnabled)>())
        .method("Vector3 GetSyncedOwnerPosition() const", SafeCall<static_cast<Vector3 (ICollider::*)() const>(&ICollider::GetSyncedOwnerPosition)>())
        .method("Vector3 GetSyncedOwnerRotationEuler() const", SafeCall<static_cast<Vector3 (ICollider::*)() const>(&ICollider::GetSyncedOwnerRotationEuler)>())
        .method("Quaternion GetSyncedOwnerRotation() const", SafeCall<static_cast<Quaternion (ICollider::*)() const>(&ICollider::GetSyncedOwnerRotation)>())
        .method("Vector3 GetSyncedOwnerScale() const", SafeCall<static_cast<Vector3 (ICollider::*)() const>(&ICollider::GetSyncedOwnerScale)>())
        .method("Vector2 RotateOffsetBySyncedRotation2D(const Vector2 &in) const", SafeCall<static_cast<Vector2 (ICollider::*)(const Vector2 &) const>(&ICollider::RotateOffsetBySyncedRotation2D)>());
}

/// @brief Collider@ハンドルから具体的なコライダー型のハンドルへのダウンキャスト（cast<T>用）
/// @details 参照先が削除済みの場合は例外化してnullptrを返す。生存していれば実際の型を
///          dynamic_castで確認し、一致すれば同じComponentRefを持つ新しいハンドルを作る
template <typename T>
ScriptComponentHandle<T> *ColliderDownCast(ScriptComponentHandle<ICollider> *handle) {
    if (!handle) return nullptr;
    ICollider *collider = handle->Resolve();
    if (!collider) { ThrowDestroyedObjectException(); return nullptr; }
    return ScriptComponentHandle<T>::Create(dynamic_cast<T *>(collider));
}

/// @brief コライダー型を登録する（ICollider共通のメソッドを追加で登録する）
template <typename T>
auto RegisterColliderType(asIScriptEngine *engine, const char *name) {
    auto binder = RegisterComponentType<T>(engine, name);
    binder
        .method("bool IsTrigger() const", SafeCall<static_cast<bool (T::*)() const noexcept>(&T::IsTrigger)>())
        .method("void SetTrigger(bool)", SafeCall<static_cast<void (T::*)(bool) noexcept>(&T::SetTrigger)>())
        .method("bool IsContinuousDetection() const", SafeCall<static_cast<bool (T::*)() const noexcept>(&T::IsContinuousDetection)>())
        .method("void SetContinuousDetection(bool)", SafeCall<static_cast<void (T::*)(bool) noexcept>(&T::SetContinuousDetection)>())
        .method("bool Is2D() const", SafeCall<static_cast<bool (T::*)() const noexcept>(&T::Is2D)>())
        .method("ColliderShape GetShape() const", SafeCall<static_cast<ICollider::Shape (T::*)() const noexcept>(&T::GetShape)>())
        .method("Vector3 GetOwnerWorldPosition() const", SafeCall<static_cast<Vector3 (T::*)() const>(&T::GetOwnerWorldPosition)>())
        .method("bool IsSyncPositionEnabled(int) const", SafeCall<static_cast<bool (T::*)(int) const noexcept>(&T::IsSyncPositionEnabled)>())
        .method("bool IsSyncRotationEnabled() const", SafeCall<static_cast<bool (T::*)() const noexcept>(&T::IsSyncRotationEnabled)>())
        .method("bool IsSyncScaleEnabled(int) const", SafeCall<static_cast<bool (T::*)(int) const noexcept>(&T::IsSyncScaleEnabled)>())
        .method("Vector3 GetSyncedOwnerPosition() const", SafeCall<static_cast<Vector3 (T::*)() const>(&T::GetSyncedOwnerPosition)>())
        .method("Vector3 GetSyncedOwnerRotationEuler() const", SafeCall<static_cast<Vector3 (T::*)() const>(&T::GetSyncedOwnerRotationEuler)>())
        .method("Quaternion GetSyncedOwnerRotation() const", SafeCall<static_cast<Quaternion (T::*)() const>(&T::GetSyncedOwnerRotation)>())
        .method("Vector3 GetSyncedOwnerScale() const", SafeCall<static_cast<Vector3 (T::*)() const>(&T::GetSyncedOwnerScale)>())
        .method("Vector2 RotateOffsetBySyncedRotation2D(const Vector2 &in) const", SafeCall<static_cast<Vector2 (T::*)(const Vector2 &) const>(&T::RotateOffsetBySyncedRotation2D)>())
        // 基底のCollider型への暗黙変換（HitInfoのselfCollider/otherColliderとの比較用）。
        // 参照先が削除済みでも「同じComponentRefを持つCollider@ハンドル」は作れるため、
        // ここではResolve失敗を例外化しない（Colliderとしての生死判定はCollider側のメソッドで行われる）
        .method("Collider@ opImplCast()", [](ScriptComponentHandle<T> &self) -> ScriptComponentHandle<ICollider> * {
            T *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<ICollider>::Create(static_cast<ICollider *>(obj));
        });
    // cast<具体型>(Collider@) によるダウンキャスト
    engine->RegisterObjectMethod("Collider", (std::string(name) + "@ opCast()").c_str(),
        asFUNCTION((ColliderDownCast<T>)), asCALL_CDECL_OBJLAST);
    return binder;
}

/// @brief ICollider*のポインタ配列から array<Collider@>@ を構築する（RigidBody2D/3D::GetOwnerColliders用）
CScriptArray *MakeColliderArray(const std::vector<ICollider *> &colliders) {
    asIScriptContext *context = asGetActiveContext();
    asIScriptEngine *engine = context ? context->GetEngine() : nullptr;
    if (!engine) return nullptr;

    asITypeInfo *arrayType = engine->GetTypeInfoByDecl("array<Collider@>");
    if (!arrayType) return nullptr;

    CScriptArray *array = CScriptArray::Create(arrayType, static_cast<asUINT>(colliders.size()));
    if (!array) return nullptr;
    for (asUINT i = 0; i < colliders.size(); ++i) {
        // ScriptComponentHandle<ICollider>::Createはrefcount=1で生成される。SetValueは配列側で
        // 独自にAddRefするため、格納後にこちら側の分をReleaseして所有権を配列だけに残す
        ScriptComponentHandle<ICollider> *handle = ScriptComponentHandle<ICollider>::Create(colliders[i]);
        void *handlePtr = handle;
        array->SetValue(i, &handlePtr);
        if (handle) handle->Release();
    }
    return array;
}

/// @brief std::stringの配列から array<string>@ を構築する（MakeStringArrayの前方宣言。定義はJson登録セクションにある）
CScriptArray *MakeStringArray(const std::vector<std::string> &values);

/// @brief array<string>@ からstd::vector<std::string>へ変換する（CameraRenderer::SetBindVariableNames用）
std::vector<std::string> StringArrayToVector(CScriptArray *array) {
    std::vector<std::string> result;
    if (!array) return result;
    result.reserve(array->GetSize());
    for (asUINT i = 0; i < array->GetSize(); ++i) {
        result.push_back(*static_cast<const std::string *>(array->At(i)));
    }
    return result;
}

/// @brief 指定ウィンドウコンポーネントのクライアント座標系でのマウス座標を取得する（未生成/取得不可時は{0,0}）
/// @details WindowObjectメソッド版とグローバル関数版の両方から共有される実装本体
Vector2 GetWindowMousePosition(const IWindowObjectComponent *component) {
    if (!component) return Vector2::Zero();
    Window *window = component->GetWindow();
    if (!window || !Window::IsExist(window)) return Vector2::Zero();
    Input *input = gCurrentSceneContext ? gCurrentSceneContext->GetInput() : nullptr;
    if (!input) return Vector2::Zero();
    const POINT p = input->GetMouse().GetPos(window);
    return Vector2(static_cast<float>(p.x), static_cast<float>(p.y));
}

/// @brief マウスカーソルが指定ウィンドウコンポーネントのクライアント領域内にあるかどうかを取得する
bool IsWindowMouseInside(const IWindowObjectComponent *component) {
    if (!component) return false;
    Window *window = component->GetWindow();
    if (!window || !Window::IsExist(window)) return false;
    const Vector2 pos = GetWindowMousePosition(component);
    return pos.x >= 0.0f && pos.y >= 0.0f &&
        pos.x < static_cast<float>(window->GetClientWidth()) &&
        pos.y < static_cast<float>(window->GetClientHeight());
}

/// @brief WindowObject（IWindowObjectComponent基底）型を参照型として登録する
/// @details WindowMessageInfoのsourceComponentで「どのウィンドウコンポーネントからの通知か」を
///          受け渡すための共通型。NormalWindowObject/OverlayWindowObjectはopImplCastでこの型へ
///          暗黙変換でき、cast<NormalWindowObject>(info.sourceComponent) のように具体型へダウンキャストもできる。
///          他のコンポーネントと同様、生ポインタではなくScriptComponentHandle<IWindowObjectComponent>
///          （UUID+追加順ID保持の参照カウント式ハンドル）として公開する
void RegisterWindowObjectBaseType(asIScriptEngine *engine) {
    asbind20::ref_class<ScriptComponentHandle<IWindowObjectComponent>>(engine, "WindowObject")
        .addref(&ScriptComponentHandle<IWindowObjectComponent>::AddRef)
        .release(&ScriptComponentHandle<IWindowObjectComponent>::Release)
        .method("bool IsActive() const", SafeCall<static_cast<bool (IWindowObjectComponent::*)() const>(&IWindowObjectComponent::IsActive)>())
        .method("void SetActive(bool)", SafeCall<static_cast<void (IWindowObjectComponent::*)(bool)>(&IWindowObjectComponent::SetActive)>())
        .method("const string &GetComponentType() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> const std::string & {
            static const std::string kEmpty;
            IWindowObjectComponent *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetComponentType();
        })
        .method("void SetTag(const string &in)", SafeCall<static_cast<void (IWindowObjectComponent::*)(const std::string &)>(&IWindowObjectComponent::SetTag)>())
        .method("Tag GetTag() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> Tag {
            IWindowObjectComponent *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return Tag(); }
            return obj->GetTag();
        })
        .method("const string &GetTagName() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> const std::string & {
            static const std::string kEmpty;
            IWindowObjectComponent *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetTagName();
        })
        .method("void SetTitle(const string &in)", SafeCall<&IWindowObjectComponent::SetTitle>())
        .method("const string &GetTitle() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> const std::string & {
            static const std::string kEmpty;
            IWindowObjectComponent *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetTitle();
        })
        .method("void SetSize(uint, uint)", SafeCall<&IWindowObjectComponent::SetSize>())
        .method("void SetSyncWithTransform(bool)", SafeCall<&IWindowObjectComponent::SetSyncWithTransform>())
        .method("bool IsSyncWithTransformEnabled() const", SafeCall<&IWindowObjectComponent::IsSyncWithTransformEnabled>())
        .method("bool SetMessageIntercepted(uint msg, bool enabled)", SafeCall<&IWindowObjectComponent::SetMessageIntercepted>())
        .method("bool IsMessageIntercepted(uint msg) const", SafeCall<&IWindowObjectComponent::IsMessageIntercepted>())
        .method("void CloseWindow()", SafeCall<&IWindowObjectComponent::CloseWindow>())
        .method("int GetClientWidth() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> int {
            IWindowObjectComponent *component = self.Resolve();
            if (!component) { ThrowDestroyedObjectException(); return 0; }
            Window *window = component->GetWindow();
            return (window && Window::IsExist(window)) ? window->GetClientWidth() : 0;
        })
        .method("int GetClientHeight() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> int {
            IWindowObjectComponent *component = self.Resolve();
            if (!component) { ThrowDestroyedObjectException(); return 0; }
            Window *window = component->GetWindow();
            return (window && Window::IsExist(window)) ? window->GetClientHeight() : 0;
        })
        .method("bool IsWindowFocused() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> bool {
            IWindowObjectComponent *component = self.Resolve();
            if (!component) { ThrowDestroyedObjectException(); return false; }
            Window *window = component->GetWindow();
            return (window && Window::IsExist(window)) ? window->IsFocused() : false;
        })
        .method("bool IsWindowMinimized() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> bool {
            IWindowObjectComponent *component = self.Resolve();
            if (!component) { ThrowDestroyedObjectException(); return false; }
            Window *window = component->GetWindow();
            return (window && Window::IsExist(window)) ? window->IsMinimized() : false;
        })
        .method("Vector2 GetMousePosition() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> Vector2 {
            IWindowObjectComponent *component = self.Resolve();
            if (!component) { ThrowDestroyedObjectException(); return Vector2::Zero(); }
            return GetWindowMousePosition(component);
        })
        .method("bool IsMouseInside() const", [](const ScriptComponentHandle<IWindowObjectComponent> &self) -> bool {
            IWindowObjectComponent *component = self.Resolve();
            if (!component) { ThrowDestroyedObjectException(); return false; }
            return IsWindowMouseInside(component);
        });
}

/// @brief WindowObject@ハンドルから具体的なウィンドウコンポーネント型のハンドルへのダウンキャスト（cast<T>用）
template <typename T>
ScriptComponentHandle<T> *WindowObjectDownCast(ScriptComponentHandle<IWindowObjectComponent> *handle) {
    if (!handle) return nullptr;
    IWindowObjectComponent *component = handle->Resolve();
    if (!component) { ThrowDestroyedObjectException(); return nullptr; }
    return ScriptComponentHandle<T>::Create(dynamic_cast<T *>(component));
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

/// @brief SkinnedMeshRenderer::SkinQuality をスクリプト用の SkinQuality 列挙型として登録する
void RegisterSkinQualityEnum(asIScriptEngine *engine) {
    engine->RegisterEnum("SkinQuality");
    engine->RegisterEnumValue("SkinQuality", "Auto", static_cast<int>(SkinQuality::Auto));
    engine->RegisterEnumValue("SkinQuality", "Bone1", static_cast<int>(SkinQuality::Bone1));
    engine->RegisterEnumValue("SkinQuality", "Bone2", static_cast<int>(SkinQuality::Bone2));
    engine->RegisterEnumValue("SkinQuality", "Bone4", static_cast<int>(SkinQuality::Bone4));
}

/// @brief Transformコンポーネントを登録する
/// @details Object::GetTransform() が Transform@ を返すため、Object/Scene（RegisterObjectTypes）より
///          先に登録しておく必要がある。gComponentTypeBindings のクリアもここで行う（最初に呼ばれるため）
void RegisterTransformType(asIScriptEngine *engine) {
    gComponentTypeBindings.clear();
    RegisterLightTypeEnum(engine);
    RegisterTextRendererEnums(engine);
    RegisterSkinQualityEnum(engine);

    RegisterComponentType<Transform>(engine, "Transform")
        .method("void SetTranslate(const Vector3 &in)", SafeCall<&Transform::SetTranslate>())
        .method("const Vector3 &GetTranslate() const", SafeCall<&Transform::GetTranslate>())
        .method("void SetRotate(const Vector3 &in)", SafeCall<&Transform::SetRotate>())
        .method("const Vector3 &GetRotate() const", SafeCall<&Transform::GetRotate>())
        .method("void SetRotateQuaternion(const Quaternion &in)", SafeCall<&Transform::SetRotateQuaternion>())
        .method("const Quaternion &GetRotateQuaternion() const", SafeCall<&Transform::GetRotateQuaternion>())
        .method("void SetScale(const Vector3 &in)", SafeCall<&Transform::SetScale>())
        .method("const Vector3 &GetScale() const", SafeCall<&Transform::GetScale>())
        .method("const Matrix4x4 &GetWorldMatrix()", SafeCall<&Transform::GetWorldMatrix>())
        .method("Vector3 GetWorldPosition()", SafeCall<&Transform::GetWorldPosition>())
        .method("Vector3 GetWorldRotate() const", SafeCall<&Transform::GetWorldRotate>())
        .method("Quaternion GetWorldRotateQuaternion() const", SafeCall<&Transform::GetWorldRotateQuaternion>())
        .method("Vector3 GetWorldScale()", SafeCall<&Transform::GetWorldScale>());
}

void RegisterComponentTypes(asIScriptEngine *engine) {
    RegisterComponentType<Velocity>(engine, "Velocity")
        .method("void SetVelocity(const Vector3 &in)", SafeCall<&Velocity::SetVelocity>())
        .method("const Vector3 &GetVelocity() const", SafeCall<&Velocity::GetVelocity>())
        .method("void SetAcceleration(const Vector3 &in)", SafeCall<&Velocity::SetAcceleration>())
        .method("const Vector3 &GetAcceleration() const", SafeCall<&Velocity::GetAcceleration>())
        .method("void AddVelocity(const Vector3 &in)", SafeCall<&Velocity::AddVelocity>());

    RegisterComponentType<Rotation>(engine, "Rotation")
        .method("void SetAngularVelocity(const Vector3 &in)", SafeCall<&Rotation::SetAngularVelocity>())
        .method("const Vector3 &GetAngularVelocity() const", SafeCall<&Rotation::GetAngularVelocity>())
        .method("void SetAngularAcceleration(const Vector3 &in)", SafeCall<&Rotation::SetAngularAcceleration>())
        .method("const Vector3 &GetAngularAcceleration() const", SafeCall<&Rotation::GetAngularAcceleration>())
        .method("void AddAngularVelocity(const Vector3 &in)", SafeCall<&Rotation::AddAngularVelocity>());

    RegisterComponentType<PreTransform>(engine, "PreTransform")
        .method("const Vector3 &GetPreviousTranslate() const", SafeCall<&PreTransform::GetPreviousTranslate>())
        .method("const Vector3 &GetPreviousRotate() const", SafeCall<&PreTransform::GetPreviousRotate>())
        .method("const Quaternion &GetPreviousRotateQuaternion() const", SafeCall<&PreTransform::GetPreviousRotateQuaternion>())
        .method("const Vector3 &GetPreviousScale() const", SafeCall<&PreTransform::GetPreviousScale>())
        .method("const Matrix4x4 &GetPreviousWorldMatrix() const", SafeCall<&PreTransform::GetPreviousWorldMatrix>())
        .method("const Vector3 &GetPreviousWorldPosition() const", SafeCall<&PreTransform::GetPreviousWorldPosition>())
        .method("const Vector3 &GetPreviousWorldRotate() const", SafeCall<&PreTransform::GetPreviousWorldRotate>())
        .method("const Quaternion &GetPreviousWorldRotateQuaternion() const", SafeCall<&PreTransform::GetPreviousWorldRotateQuaternion>())
        .method("const Vector3 &GetPreviousWorldScale() const", SafeCall<&PreTransform::GetPreviousWorldScale>());

    // TargetLookAtの回転モード（ParticleSystemのビルボード設定でも使うため先に登録する）
    engine->RegisterEnum("TargetLookAtMode");
    engine->RegisterEnumValue("TargetLookAtMode", "SyncRotation", static_cast<int>(TargetLookAt::RotationMode::SyncRotation));
    engine->RegisterEnumValue("TargetLookAtMode", "LookAt", static_cast<int>(TargetLookAt::RotationMode::LookAt));

    RegisterComponentType<ParticleSystem2D>(engine, "ParticleSystem2D")
        .method("void Play()", [](ScriptComponentHandle<ParticleSystem2D> &selfHandle) {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem2D &self = *selfPtr; self.Play(); })
        .method("void Stop()", [](ScriptComponentHandle<ParticleSystem2D> &selfHandle) {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem2D &self = *selfPtr; self.Stop(); })
        .method("bool IsPlaying() const", [](const ScriptComponentHandle<ParticleSystem2D> &selfHandle) -> bool {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const ParticleSystem2D &self = *selfPtr; return self.IsPlaying(); })
        .method("void Clear()", [](ScriptComponentHandle<ParticleSystem2D> &selfHandle) {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem2D &self = *selfPtr; self.Clear(); })
        .method("void SetEmissionRate(float)", [](ScriptComponentHandle<ParticleSystem2D> &selfHandle, float rate) {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem2D &self = *selfPtr; self.SetEmissionRate(rate); })
        .method("float GetEmissionRate() const", [](const ScriptComponentHandle<ParticleSystem2D> &selfHandle) -> float {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const ParticleSystem2D &self = *selfPtr; return self.GetEmissionRate(); })
        .method("void SetMaxParticles(int)", [](ScriptComponentHandle<ParticleSystem2D> &selfHandle, int count) {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem2D &self = *selfPtr; self.SetMaxParticles(count); })
        .method("int GetMaxParticles() const", [](const ScriptComponentHandle<ParticleSystem2D> &selfHandle) -> int {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const ParticleSystem2D &self = *selfPtr; return self.GetMaxParticles(); })
        .method("void SetBillboard(bool)", [](ScriptComponentHandle<ParticleSystem2D> &selfHandle, bool enabled) {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem2D &self = *selfPtr; self.SetBillboard(enabled); })
        .method("bool IsBillboard() const", [](const ScriptComponentHandle<ParticleSystem2D> &selfHandle) -> bool {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const ParticleSystem2D &self = *selfPtr; return self.IsBillboard(); })
        .method("void SetBillboardTarget(Object@)", [](ScriptComponentHandle<ParticleSystem2D> &selfHandle, ScriptObjectHandle *obj) {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem2D &self = *selfPtr; self.SetBillboardTarget(ResolveObjectArg(obj)); })
        .method("Object@ GetBillboardTarget() const", [](const ScriptComponentHandle<ParticleSystem2D> &selfHandle) -> ScriptObjectHandle * {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const ParticleSystem2D &self = *selfPtr; return ScriptObjectHandle::Create(self.GetBillboardTarget()); })
        .method("void SetBillboardRotationMode(TargetLookAtMode)", [](ScriptComponentHandle<ParticleSystem2D> &selfHandle, TargetLookAt::RotationMode mode) {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem2D &self = *selfPtr; self.SetBillboardRotationMode(mode); })
        .method("TargetLookAtMode GetBillboardRotationMode() const", [](const ScriptComponentHandle<ParticleSystem2D> &selfHandle) -> TargetLookAt::RotationMode {
            ParticleSystem2D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<TargetLookAt::RotationMode>(); }
            const ParticleSystem2D &self = *selfPtr; return self.GetBillboardRotationMode(); });

    RegisterComponentType<ParticleSystem3D>(engine, "ParticleSystem3D")
        .method("void Play()", [](ScriptComponentHandle<ParticleSystem3D> &selfHandle) {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem3D &self = *selfPtr; self.Play(); })
        .method("void Stop()", [](ScriptComponentHandle<ParticleSystem3D> &selfHandle) {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem3D &self = *selfPtr; self.Stop(); })
        .method("bool IsPlaying() const", [](const ScriptComponentHandle<ParticleSystem3D> &selfHandle) -> bool {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const ParticleSystem3D &self = *selfPtr; return self.IsPlaying(); })
        .method("void Clear()", [](ScriptComponentHandle<ParticleSystem3D> &selfHandle) {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem3D &self = *selfPtr; self.Clear(); })
        .method("void SetEmissionRate(float)", [](ScriptComponentHandle<ParticleSystem3D> &selfHandle, float rate) {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem3D &self = *selfPtr; self.SetEmissionRate(rate); })
        .method("float GetEmissionRate() const", [](const ScriptComponentHandle<ParticleSystem3D> &selfHandle) -> float {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const ParticleSystem3D &self = *selfPtr; return self.GetEmissionRate(); })
        .method("void SetMaxParticles(int)", [](ScriptComponentHandle<ParticleSystem3D> &selfHandle, int count) {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem3D &self = *selfPtr; self.SetMaxParticles(count); })
        .method("int GetMaxParticles() const", [](const ScriptComponentHandle<ParticleSystem3D> &selfHandle) -> int {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const ParticleSystem3D &self = *selfPtr; return self.GetMaxParticles(); })
        .method("void SetBillboard(bool)", [](ScriptComponentHandle<ParticleSystem3D> &selfHandle, bool enabled) {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem3D &self = *selfPtr; self.SetBillboard(enabled); })
        .method("bool IsBillboard() const", [](const ScriptComponentHandle<ParticleSystem3D> &selfHandle) -> bool {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const ParticleSystem3D &self = *selfPtr; return self.IsBillboard(); })
        .method("void SetBillboardTarget(Object@)", [](ScriptComponentHandle<ParticleSystem3D> &selfHandle, ScriptObjectHandle *obj) {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem3D &self = *selfPtr; self.SetBillboardTarget(ResolveObjectArg(obj)); })
        .method("Object@ GetBillboardTarget() const", [](const ScriptComponentHandle<ParticleSystem3D> &selfHandle) -> ScriptObjectHandle * {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const ParticleSystem3D &self = *selfPtr; return ScriptObjectHandle::Create(self.GetBillboardTarget()); })
        .method("void SetBillboardRotationMode(TargetLookAtMode)", [](ScriptComponentHandle<ParticleSystem3D> &selfHandle, TargetLookAt::RotationMode mode) {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return; }
            ParticleSystem3D &self = *selfPtr; self.SetBillboardRotationMode(mode); })
        .method("TargetLookAtMode GetBillboardRotationMode() const", [](const ScriptComponentHandle<ParticleSystem3D> &selfHandle) -> TargetLookAt::RotationMode {
            ParticleSystem3D *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<TargetLookAt::RotationMode>(); }
            const ParticleSystem3D &self = *selfPtr; return self.GetBillboardRotationMode(); });

    RegisterComponentType<TargetLookAt>(engine, "TargetLookAt")
        .method("void SetTargetObject(Object@)", [](ScriptComponentHandle<TargetLookAt> &cHandle, ScriptObjectHandle *obj) {
            TargetLookAt *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TargetLookAt &c = *cPtr; c.SetTargetObject(ResolveObjectArg(obj)); })
        .method("Object@ GetTargetObject() const", [](const ScriptComponentHandle<TargetLookAt> &cHandle) -> ScriptObjectHandle * {
            TargetLookAt *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const TargetLookAt &c = *cPtr; return ScriptObjectHandle::Create(c.GetTargetObject()); })
        .method("void SetRotationOffset(const Vector3 &in)", SafeCall<&TargetLookAt::SetRotationOffset>())
        .method("const Vector3 &GetRotationOffset() const", SafeCall<&TargetLookAt::GetRotationOffset>())
        .method("void SetRotationMode(TargetLookAtMode)", SafeCall<&TargetLookAt::SetRotationMode>())
        .method("TargetLookAtMode GetRotationMode() const", SafeCall<&TargetLookAt::GetRotationMode>())
        .method("void SetFollowStrength(float)", SafeCall<&TargetLookAt::SetFollowStrength>())
        .method("float GetFollowStrength() const", SafeCall<&TargetLookAt::GetFollowStrength>());

    RegisterComponentType<AudioSource>(engine, "AudioSource")
        .method("uint Play()", SafeCall<&AudioSource::Play>())
        .method("void Stop()", SafeCall<&AudioSource::Stop>())
        .method("bool Pause()", SafeCall<&AudioSource::Pause>())
        .method("bool Resume()", SafeCall<&AudioSource::Resume>())
        .method("bool IsPlaying() const", SafeCall<&AudioSource::IsPlaying>())
        .method("bool IsPaused() const", SafeCall<&AudioSource::IsPaused>())
        .method("void SetSoundName(const string &in)", SafeCall<&AudioSource::SetSoundName>())
        .method("const string &GetSoundName() const", SafeCall<&AudioSource::GetSoundName>())
        .method("void SetVolume(float)", SafeCall<&AudioSource::SetVolume>())
        .method("float GetVolume() const", SafeCall<&AudioSource::GetVolume>())
        .method("void SetPitch(float)", SafeCall<&AudioSource::SetPitch>())
        .method("float GetPitch() const", SafeCall<&AudioSource::GetPitch>())
        .method("void SetLoop(bool)", SafeCall<&AudioSource::SetLoop>())
        .method("bool GetLoop() const", SafeCall<&AudioSource::GetLoop>())
        .method("void SetPlayOnAwake(bool)", SafeCall<&AudioSource::SetPlayOnAwake>())
        .method("bool GetPlayOnAwake() const", SafeCall<&AudioSource::GetPlayOnAwake>())
        .method("void AttachExternalPlayHandle(uint)", SafeCall<&AudioSource::AttachExternalPlayHandle>());

    RegisterComponentType<AudioListener>(engine, "AudioListener")
        .method("void SetUsed(bool)", SafeCall<&AudioListener::SetUsed>())
        .method("bool GetUsed() const", SafeCall<&AudioListener::GetUsed>())
        .method("Vector3 GetWorldPosition() const", SafeCall<&AudioListener::GetWorldPosition>());

    RegisterComponentType<Camera3D>(engine, "Camera3D")
        .method("void SetFovY(float)", SafeCall<&Camera3D::SetFovY>())
        .method("float GetFovY() const", SafeCall<&Camera3D::GetFovY>())
        .method("void SetNearClip(float)", SafeCall<&Camera3D::SetNearClip>())
        .method("float GetNearClip() const", SafeCall<&Camera3D::GetNearClip>())
        .method("void SetFarClip(float)", SafeCall<&Camera3D::SetFarClip>())
        .method("float GetFarClip() const", SafeCall<&Camera3D::GetFarClip>())
        .method("void SetAspectRatio(float)", SafeCall<&Camera3D::SetAspectRatio>())
        .method("float GetAspectRatio() const", SafeCall<&Camera3D::GetAspectRatio>())
        .method("void SetOrthographic(bool)", SafeCall<&Camera3D::SetOrthographic>())
        .method("bool IsOrthographic() const", SafeCall<&Camera3D::IsOrthographic>())
        .method("void SetOrthoSize(float)", SafeCall<&Camera3D::SetOrthoSize>())
        .method("float GetOrthoSize() const", SafeCall<&Camera3D::GetOrthoSize>())
        .method("void SetEnableJitter(bool)", SafeCall<&Camera3D::SetEnableJitter>())
        .method("bool IsJitterEnabled() const", SafeCall<&Camera3D::IsJitterEnabled>())
        .method("void SetAutoSyncAspectRatio(bool)", SafeCall<&Camera3D::SetAutoSyncAspectRatio>())
        .method("bool GetAutoSyncAspectRatio() const", SafeCall<&Camera3D::GetAutoSyncAspectRatio>());

    RegisterComponentType<SpriteRenderer>(engine, "SpriteRenderer")
        .method("void SetAnchor(const Vector2 &in)", SafeCall<&SpriteRenderer::SetAnchor>())
        .method("const Vector2 &GetAnchor() const", SafeCall<&SpriteRenderer::GetAnchor>())
        .method("void SetPivot(const Vector2 &in)", SafeCall<&SpriteRenderer::SetPivot>())
        .method("const Vector2 &GetPivot() const", SafeCall<&SpriteRenderer::GetPivot>())
        .method("void SetPipelineName(const string &in)", SafeCall<&SpriteRenderer::SetPipelineName>())
        .method("const string &GetPipelineName() const", SafeCall<&SpriteRenderer::GetPipelineName>())
        .method("void SetMaterialName(const string &in)", SafeCall<&SpriteRenderer::SetMaterialName>())
        .method("const string &GetMaterialName() const", SafeCall<&SpriteRenderer::GetMaterialName>())
        .method("void SetMaterialHandle(uint)", [](ScriptComponentHandle<SpriteRenderer> &cHandle, uint32_t handle) {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SpriteRenderer &c = *cPtr; c.SetMaterialHandle(handle); })
        .method("uint GetMaterialHandle() const", [](const ScriptComponentHandle<SpriteRenderer> &cHandle) -> uint32_t {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const SpriteRenderer &c = *cPtr; return c.GetMaterialHandle(); })
        .method("Object@ GetTargetObject() const", [](const ScriptComponentHandle<SpriteRenderer> &cHandle) -> ScriptObjectHandle * {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const SpriteRenderer &c = *cPtr; return ScriptObjectHandle::Create(c.GetTargetObject()); })
        .method("string GetTargetObjectID() const", [](const ScriptComponentHandle<SpriteRenderer> &cHandle) -> std::string {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const SpriteRenderer &c = *cPtr; return c.GetTargetObjectID().ToString(); })
        .method("void SetInstanceColor(const Vector4 &in)", SafeCall<&SpriteRenderer::SetInstanceColor>())
        .method("const Vector4 &GetInstanceColor() const", SafeCall<&SpriteRenderer::GetInstanceColor>())
        // instanceColorBlendModeは 0=Override, 1=Multiply, 2=Add, 3=Subtract
        .method("void SetInstanceColorBlendMode(int)", [](ScriptComponentHandle<SpriteRenderer> &cHandle, int mode) {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SpriteRenderer &c = *cPtr;
            c.SetInstanceColorBlendMode(static_cast<SpriteRenderer::ColorBlendMode>(mode));
        })
        .method("int GetInstanceColorBlendMode() const", [](const ScriptComponentHandle<SpriteRenderer> &cHandle) {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const SpriteRenderer &c = *cPtr;
            return static_cast<int>(c.GetInstanceColorBlendMode());
        })
        .method("void SetInstanceUvTranslate(const Vector2 &in)", SafeCall<&SpriteRenderer::SetInstanceUvTranslate>())
        .method("const Vector2 &GetInstanceUvTranslate() const", SafeCall<&SpriteRenderer::GetInstanceUvTranslate>())
        .method("void SetInstanceUvRotation(float)", SafeCall<&SpriteRenderer::SetInstanceUvRotation>())
        .method("float GetInstanceUvRotation() const", SafeCall<&SpriteRenderer::GetInstanceUvRotation>())
        .method("void SetInstanceUvScale(const Vector2 &in)", SafeCall<&SpriteRenderer::SetInstanceUvScale>())
        .method("const Vector2 &GetInstanceUvScale() const", SafeCall<&SpriteRenderer::GetInstanceUvScale>())
        .method("void SetInstanceUvPivot(const Vector2 &in)", SafeCall<&SpriteRenderer::SetInstanceUvPivot>())
        .method("const Vector2 &GetInstanceUvPivot() const", SafeCall<&SpriteRenderer::GetInstanceUvPivot>())
        // instanceUvCombineModeは 0=MaterialThenInstance, 1=InstanceThenMaterial, 2=InstanceOnly
        .method("void SetInstanceUvCombineMode(int)", [](ScriptComponentHandle<SpriteRenderer> &cHandle, int mode) {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SpriteRenderer &c = *cPtr;
            c.SetInstanceUvCombineMode(static_cast<SpriteRenderer::UVCombineMode>(mode));
        })
        .method("int GetInstanceUvCombineMode() const", [](const ScriptComponentHandle<SpriteRenderer> &cHandle) {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const SpriteRenderer &c = *cPtr;
            return static_cast<int>(c.GetInstanceUvCombineMode());
        })
        .method("void SetRenderPriority(int)", [](ScriptComponentHandle<SpriteRenderer> &cHandle, int32_t priority) {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SpriteRenderer &c = *cPtr; c.SetRenderPriority(priority); })
        .method("int GetRenderPriority() const", [](const ScriptComponentHandle<SpriteRenderer> &cHandle) -> int32_t {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int32_t>(); }
            const SpriteRenderer &c = *cPtr; return c.GetRenderPriority(); })
        .method("void SetAllowInstancing(bool)", SafeCall<&SpriteRenderer::SetAllowInstancing>())
        .method("bool GetAllowInstancing() const", SafeCall<&SpriteRenderer::GetAllowInstancing>())
        .method("uint GetMeshHandle() const", [](const ScriptComponentHandle<SpriteRenderer> &cHandle) -> uint32_t {
            SpriteRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const SpriteRenderer &c = *cPtr; return c.GetMeshHandle(); })
        .method("Matrix4x4 GetWorldMatrix() const", SafeCall<&SpriteRenderer::GetWorldMatrix>());

    RegisterComponentType<TilemapRenderer>(engine, "TilemapRenderer")
        .method("void SetTile(int, int, int)", SafeCall<&TilemapRenderer::SetTile>())
        .method("int GetTile(int, int) const", SafeCall<&TilemapRenderer::GetTile>())
        .method("void Resize(int, int)", SafeCall<&TilemapRenderer::Resize>())
        .method("void Clear()", SafeCall<&TilemapRenderer::Clear>())
        .method("int GetGridWidth() const", SafeCall<&TilemapRenderer::GetGridWidth>())
        .method("int GetGridHeight() const", SafeCall<&TilemapRenderer::GetGridHeight>())
        .method("void SetMaterialName(const string &in)", SafeCall<&TilemapRenderer::SetMaterialName>())
        .method("const string &GetMaterialName() const", SafeCall<&TilemapRenderer::GetMaterialName>())
        .method("void SetPipelineName(const string &in)", SafeCall<&TilemapRenderer::SetPipelineName>())
        .method("const string &GetPipelineName() const", SafeCall<&TilemapRenderer::GetPipelineName>())
        .method("void SetTileSize(const Vector2 &in)", SafeCall<&TilemapRenderer::SetTileSize>())
        .method("const Vector2 &GetTileSize() const", SafeCall<&TilemapRenderer::GetTileSize>())
        .method("void SetTilePixelSize(const Vector2 &in)", SafeCall<&TilemapRenderer::SetTilePixelSize>())
        .method("const Vector2 &GetTilePixelSize() const", SafeCall<&TilemapRenderer::GetTilePixelSize>())
        .method("int AddTileType(const Vector2 &in)", SafeCall<&TilemapRenderer::AddTileType>())
        .method("void RemoveTileType(int)", SafeCall<&TilemapRenderer::RemoveTileType>())
        .method("int GetTileTypeCount() const", SafeCall<&TilemapRenderer::GetTileTypeCount>())
        .method("void AddTileTypeConnection(int, int)", SafeCall<&TilemapRenderer::AddTileTypeConnection>())
        .method("void RemoveTileTypeConnection(int, int)", SafeCall<&TilemapRenderer::RemoveTileTypeConnection>())
        .method("int GetTileTypeConnectionCount(int) const", SafeCall<&TilemapRenderer::GetTileTypeConnectionCount>())
        .method("int GetTileTypeConnectionAt(int, int) const", SafeCall<&TilemapRenderer::GetTileTypeConnectionAt>())
        .method("void SetTileTypeOriginPx(int, const Vector2 &in)", SafeCall<&TilemapRenderer::SetTileTypeOriginPx>())
        .method("void SetTileTypeSolid(int, bool)", SafeCall<&TilemapRenderer::SetTileTypeSolid>())
        .method("bool GetTileTypeSolid(int) const", SafeCall<&TilemapRenderer::GetTileTypeSolid>())
        .method("void SetGenerateColliders(bool)", SafeCall<&TilemapRenderer::SetGenerateColliders>())
        .method("bool GetGenerateColliders() const", SafeCall<&TilemapRenderer::GetGenerateColliders>())
        // autotileModeは 0=FourDirection, 1=EightDirection
        .method("void SetAutotileMode(int)", [](ScriptComponentHandle<TilemapRenderer> &cHandle, int mode) {
            TilemapRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TilemapRenderer &c = *cPtr;
            c.SetAutotileMode(static_cast<TilemapRenderer::AutotileMode>(mode));
        })
        .method("int GetAutotileMode() const", [](const ScriptComponentHandle<TilemapRenderer> &cHandle) {
            TilemapRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const TilemapRenderer &c = *cPtr;
            return static_cast<int>(c.GetAutotileMode());
        });

    RegisterComponentType<ScriptComponent>(engine, "ScriptComponent")
        .method("void SetScriptPath(const string &in)", SafeCall<&ScriptComponent::SetScriptPath>())
        .method("const string &GetScriptPath() const", SafeCall<&ScriptComponent::GetScriptPath>())
        .method("bool Reload()", SafeCall<&ScriptComponent::Reload>())
        // 他オブジェクトのScriptComponentを取得した上で、その[SerializeField]変数を名前で直接読み書きする
        // （シーン変数を介さないスクリプト間のデータ受け渡し用。対応型はSerializeFieldと同じプリミティブ/数学型/Object@のみ）
        .method("bool GetVariable(const string &in, ?&out) const", [](ScriptComponentHandle<ScriptComponent> &selfHandle, const std::string &name, void *ref, int typeId) -> bool {
            ScriptComponent *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const ScriptComponent &self = *selfPtr;
            return self.GetVariable(name, ref, typeId);
        })
        .method("bool SetVariable(const string &in, ?&in)", [](ScriptComponentHandle<ScriptComponent> &selfHandle, const std::string &name, void *ref, int typeId) -> bool {
            ScriptComponent *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            ScriptComponent &self = *selfPtr;
            return self.SetVariable(name, ref, typeId);
        })
        // 他オブジェクトのScriptComponentを取得した上で、そのBehaviorインスタンスの関数を名前で直接呼び出す
        // （スクリプト間の関数呼び出し用。対象は"void 関数名()"または引数1個・戻り値無しの関数のみ）
        .method("bool CallMethod(const string &in)", [](ScriptComponentHandle<ScriptComponent> &selfHandle, const std::string &name) -> bool {
            ScriptComponent *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            return selfPtr->InvokeMethod(name);
        })
        .method("bool CallMethod(const string &in, ?&in)", [](ScriptComponentHandle<ScriptComponent> &selfHandle, const std::string &name, void *ref, int typeId) -> bool {
            ScriptComponent *selfPtr = selfHandle.Resolve();
            if (!selfPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            return selfPtr->InvokeMethod(name, ref, typeId);
        });

    RegisterComponentType<MeshFilter>(engine, "MeshFilter")
        .method("void SetMeshHandle(uint)", [](ScriptComponentHandle<MeshFilter> &cHandle, uint32_t handle) {
            MeshFilter *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshFilter &c = *cPtr; c.SetMeshHandle(handle); })
        .method("uint GetMeshHandle() const", [](const ScriptComponentHandle<MeshFilter> &cHandle) -> uint32_t {
            MeshFilter *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const MeshFilter &c = *cPtr; return c.GetMeshHandle(); })
        .method("bool HasMesh() const", SafeCall<&MeshFilter::HasMesh>());

    RegisterComponentType<Animator>(engine, "Animator")
        .method("void SetClipName(const string &in)", SafeCall<&Animator::SetClipName>())
        .method("const string &GetClipName() const", SafeCall<&Animator::GetClipName>())
        .method("void SetAnimationSourceAssetPath(const string &in)", SafeCall<&Animator::SetAnimationSourceAssetPath>())
        .method("const string &GetAnimationSourceAssetPath() const", SafeCall<&Animator::GetAnimationSourceAssetPath>())
        .method("void SetPlayOnStart(bool)", SafeCall<&Animator::SetPlayOnStart>())
        .method("bool GetPlayOnStart() const", SafeCall<&Animator::GetPlayOnStart>())
        .method("void SetLoop(bool)", SafeCall<&Animator::SetLoop>())
        .method("bool GetLoop() const", SafeCall<&Animator::GetLoop>())
        .method("void SetPlaybackSpeed(float)", SafeCall<&Animator::SetPlaybackSpeed>())
        .method("float GetPlaybackSpeed() const", SafeCall<&Animator::GetPlaybackSpeed>())
        .method("void Play()", SafeCall<&Animator::Play>())
        .method("void Stop()", SafeCall<&Animator::Stop>())
        .method("bool IsPlaying() const", SafeCall<&Animator::IsPlaying>());

    RegisterComponentType<KeyFrameAnimator>(engine, "KeyFrameAnimator")
        .method("bool Play(const string &in name)", SafeCall<&KeyFrameAnimator::Play>())
        .method("bool Stop(const string &in name)", SafeCall<&KeyFrameAnimator::Stop>())
        .method("void PlayAll()", SafeCall<&KeyFrameAnimator::PlayAll>())
        .method("void StopAll()", SafeCall<&KeyFrameAnimator::StopAll>())
        .method("bool IsPlaying(const string &in name) const", SafeCall<&KeyFrameAnimator::IsPlaying>())
        .method("bool SetPlaybackSpeed(const string &in name, float speed)", SafeCall<&KeyFrameAnimator::SetPlaybackSpeed>())
        .method("float GetPlaybackSpeed(const string &in name) const", SafeCall<&KeyFrameAnimator::GetPlaybackSpeed>())
        .method("bool TryGetValue(const string &in name, float &out value) const", SafeCall<&KeyFrameAnimator::TryGetValue>())
        .method("uint GetAnimationCount() const", [](const ScriptComponentHandle<KeyFrameAnimator> &animatorHandle) -> std::uint32_t {
            KeyFrameAnimator *animatorPtr = animatorHandle.Resolve();
            if (!animatorPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::uint32_t>(); }
            const KeyFrameAnimator &animator = *animatorPtr;
            return static_cast<std::uint32_t>(animator.GetAnimationCount());
        });

    RegisterComponentType<InputCommandApplier>(engine, "InputCommandApplier")
        .method("bool WasApplied(const string &in name) const", SafeCall<&InputCommandApplier::WasApplied>())
        .method("bool TryGetLastValue(const string &in name, float &out value) const", SafeCall<&InputCommandApplier::TryGetLastValue>())
        .method("uint GetCommandCount() const", [](const ScriptComponentHandle<InputCommandApplier> &applierHandle) -> std::uint32_t {
            InputCommandApplier *applierPtr = applierHandle.Resolve();
            if (!applierPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::uint32_t>(); }
            const InputCommandApplier &applier = *applierPtr;
            return static_cast<std::uint32_t>(applier.GetCommandCount());
        });

    RegisterComponentType<SceneVariableApplier>(engine, "SceneVariableApplier")
        .method("void SetVariableName(const string &in)", SafeCall<&SceneVariableApplier::SetVariableName>())
        .method("const string &GetVariableName() const", SafeCall<&SceneVariableApplier::GetVariableName>())
        .method("bool WasApplied() const", SafeCall<&SceneVariableApplier::WasApplied>());

    RegisterComponentType<Shake>(engine, "Shake")
        .method("void Play(float = 0.0f)", SafeCall<&Shake::Play>())
        .method("void Stop()", SafeCall<&Shake::Stop>())
        .method("bool IsPlaying() const", SafeCall<&Shake::IsPlaying>())
        // ProcessTiming: 0=Immediate, 1=DeferredEnd
        .method("void SetProcessTiming(int)", SafeCall<&Shake::SetProcessTimingInt>())
        .method("int GetProcessTiming() const", SafeCall<&Shake::GetProcessTimingInt>())
        // ApplyTarget: 0=ToTransform, 1=RenderOnly
        .method("void SetApplyTarget(int)", SafeCall<&Shake::SetApplyTargetInt>())
        .method("int GetApplyTarget() const", SafeCall<&Shake::GetApplyTargetInt>())
        .method("void SetPositionEnable(bool, bool, bool)", SafeCall<&Shake::SetPositionEnable>())
        .method("void SetPositionAmplitude(const Vector3 &in)", SafeCall<&Shake::SetPositionAmplitude>())
        .method("const Vector3 &GetPositionAmplitude() const", SafeCall<&Shake::GetPositionAmplitude>())
        .method("void SetPositionSpeed(const Vector3 &in)", SafeCall<&Shake::SetPositionSpeed>())
        .method("const Vector3 &GetPositionSpeed() const", SafeCall<&Shake::GetPositionSpeed>())
        .method("void SetPositionEaseType(int)", SafeCall<&Shake::SetPositionEaseTypeInt>())
        .method("int GetPositionEaseType() const", SafeCall<&Shake::GetPositionEaseTypeInt>())
        .method("void SetPositionAmplitudeMultiplier(float, float)", SafeCall<&Shake::SetPositionAmplitudeMultiplier>())
        .method("float GetPositionAmplitudeStartMultiplier() const", SafeCall<&Shake::GetPositionAmplitudeStartMultiplier>())
        .method("float GetPositionAmplitudeEndMultiplier() const", SafeCall<&Shake::GetPositionAmplitudeEndMultiplier>())
        .method("void SetPositionSpeedMultiplier(float, float)", SafeCall<&Shake::SetPositionSpeedMultiplier>())
        .method("float GetPositionSpeedStartMultiplier() const", SafeCall<&Shake::GetPositionSpeedStartMultiplier>())
        .method("float GetPositionSpeedEndMultiplier() const", SafeCall<&Shake::GetPositionSpeedEndMultiplier>())
        .method("void SetPositionEnvelopeEaseType(int)", SafeCall<&Shake::SetPositionEnvelopeEaseTypeInt>())
        .method("int GetPositionEnvelopeEaseType() const", SafeCall<&Shake::GetPositionEnvelopeEaseTypeInt>())
        .method("void SetRotationEnable(bool, bool, bool)", SafeCall<&Shake::SetRotationEnable>())
        .method("void SetRotationAmplitude(const Vector3 &in)", SafeCall<&Shake::SetRotationAmplitude>())
        .method("const Vector3 &GetRotationAmplitude() const", SafeCall<&Shake::GetRotationAmplitude>())
        .method("void SetRotationSpeed(const Vector3 &in)", SafeCall<&Shake::SetRotationSpeed>())
        .method("const Vector3 &GetRotationSpeed() const", SafeCall<&Shake::GetRotationSpeed>())
        .method("void SetRotationEaseType(int)", SafeCall<&Shake::SetRotationEaseTypeInt>())
        .method("int GetRotationEaseType() const", SafeCall<&Shake::GetRotationEaseTypeInt>())
        .method("void SetRotationAmplitudeMultiplier(float, float)", SafeCall<&Shake::SetRotationAmplitudeMultiplier>())
        .method("float GetRotationAmplitudeStartMultiplier() const", SafeCall<&Shake::GetRotationAmplitudeStartMultiplier>())
        .method("float GetRotationAmplitudeEndMultiplier() const", SafeCall<&Shake::GetRotationAmplitudeEndMultiplier>())
        .method("void SetRotationSpeedMultiplier(float, float)", SafeCall<&Shake::SetRotationSpeedMultiplier>())
        .method("float GetRotationSpeedStartMultiplier() const", SafeCall<&Shake::GetRotationSpeedStartMultiplier>())
        .method("float GetRotationSpeedEndMultiplier() const", SafeCall<&Shake::GetRotationSpeedEndMultiplier>())
        .method("void SetRotationEnvelopeEaseType(int)", SafeCall<&Shake::SetRotationEnvelopeEaseTypeInt>())
        .method("int GetRotationEnvelopeEaseType() const", SafeCall<&Shake::GetRotationEnvelopeEaseTypeInt>())
        .method("float GetPlayProgress() const", SafeCall<&Shake::GetPlayProgress>());

    RegisterComponentType<TextRenderer>(engine, "TextRenderer")
        .method("void SetText(const string &in)", SafeCall<&TextRenderer::SetText>())
        .method("const string &GetText() const", SafeCall<&TextRenderer::GetText>())
        .method("void SetFontName(const string &in)", SafeCall<&TextRenderer::SetFontName>())
        .method("const string &GetFontName() const", SafeCall<&TextRenderer::GetFontName>())
        .method("void SetFontSize(float)", SafeCall<&TextRenderer::SetFontSize>())
        .method("float GetFontSize() const", SafeCall<&TextRenderer::GetFontSize>())
        .method("void SetColor(const Vector4 &in)", SafeCall<&TextRenderer::SetColor>())
        .method("const Vector4 &GetColor() const", SafeCall<&TextRenderer::GetColor>())
        .method("void SetInstanceColor(const Vector4 &in)", SafeCall<&TextRenderer::SetInstanceColor>())
        .method("const Vector4 &GetInstanceColor() const", SafeCall<&TextRenderer::GetInstanceColor>())
        .method("void SetInstanceColorBlendMode(int)", [](ScriptComponentHandle<TextRenderer> &cHandle, int mode) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TextRenderer &c = *cPtr;
            c.SetInstanceColorBlendMode(static_cast<TextRenderer::ColorBlendMode>(mode));
        })
        .method("int GetInstanceColorBlendMode() const", [](const ScriptComponentHandle<TextRenderer> &cHandle) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const TextRenderer &c = *cPtr;
            return static_cast<int>(c.GetInstanceColorBlendMode());
        })
        .method("void SetMaterialName(const string &in)", SafeCall<&TextRenderer::SetMaterialName>())
        .method("const string &GetMaterialName() const", SafeCall<&TextRenderer::GetMaterialName>())
        .method("void SetOutlineWidth(float)", SafeCall<&TextRenderer::SetOutlineWidth>())
        .method("float GetOutlineWidth() const", SafeCall<&TextRenderer::GetOutlineWidth>())
        .method("void SetOutlineColor(const Vector4 &in)", SafeCall<&TextRenderer::SetOutlineColor>())
        .method("const Vector4 &GetOutlineColor() const", SafeCall<&TextRenderer::GetOutlineColor>())
        .method("void SetHorizontalAlign(TextHorizontalAlign)", [](ScriptComponentHandle<TextRenderer> &cHandle, int align) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TextRenderer &c = *cPtr;
            c.SetHorizontalAlign(static_cast<TextRenderer::HorizontalAlign>(align));
        })
        .method("TextHorizontalAlign GetHorizontalAlign() const", [](const ScriptComponentHandle<TextRenderer> &cHandle) -> int {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const TextRenderer &c = *cPtr;
            return static_cast<int>(c.GetHorizontalAlign());
        })
        .method("void SetVerticalAlign(TextVerticalAlign)", [](ScriptComponentHandle<TextRenderer> &cHandle, int align) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TextRenderer &c = *cPtr;
            c.SetVerticalAlign(static_cast<TextRenderer::VerticalAlign>(align));
        })
        .method("TextVerticalAlign GetVerticalAlign() const", [](const ScriptComponentHandle<TextRenderer> &cHandle) -> int {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const TextRenderer &c = *cPtr;
            return static_cast<int>(c.GetVerticalAlign());
        })
        .method("void SetDefaultCharacterAnchor(const Vector2 &in)", SafeCall<&TextRenderer::SetDefaultCharacterAnchor>())
        .method("const Vector2 &GetDefaultCharacterAnchor() const", SafeCall<&TextRenderer::GetDefaultCharacterAnchor>())
        .method("void SetDefaultCharacterPivot(const Vector2 &in)", SafeCall<&TextRenderer::SetDefaultCharacterPivot>())
        .method("const Vector2 &GetDefaultCharacterPivot() const", SafeCall<&TextRenderer::GetDefaultCharacterPivot>())
        .method("void SetTargetObject(Object@)", [](ScriptComponentHandle<TextRenderer> &cHandle, ScriptObjectHandle *obj) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TextRenderer &c = *cPtr; c.SetTargetObject(ResolveObjectArg(obj)); })
        .method("Object@ GetTargetObject() const", [](const ScriptComponentHandle<TextRenderer> &cHandle) -> ScriptObjectHandle * {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const TextRenderer &c = *cPtr; return ScriptObjectHandle::Create(c.GetTargetObject()); })
        .method("string GetTargetObjectID() const", [](const ScriptComponentHandle<TextRenderer> &cHandle) -> std::string {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const TextRenderer &c = *cPtr; return c.GetTargetObjectID().ToString(); })
        .method("void SetMaterialHandle(uint)", [](ScriptComponentHandle<TextRenderer> &cHandle, uint32_t handle) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TextRenderer &c = *cPtr; c.SetMaterialHandle(handle); })
        .method("uint GetMaterialHandle() const", [](const ScriptComponentHandle<TextRenderer> &cHandle) -> uint32_t {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const TextRenderer &c = *cPtr; return c.GetMaterialHandle(); })
        .method("void SetUseLocalizationKey(bool)", SafeCall<&TextRenderer::SetUseLocalizationKey>())
        .method("bool GetUseLocalizationKey() const", SafeCall<&TextRenderer::GetUseLocalizationKey>())
        .method("void SetLocalizationKey(const string &in)", SafeCall<&TextRenderer::SetLocalizationKey>())
        .method("const string &GetLocalizationKey() const", SafeCall<&TextRenderer::GetLocalizationKey>())
        .method("void SetRenderPriority(int)", [](ScriptComponentHandle<TextRenderer> &cHandle, int32_t priority) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TextRenderer &c = *cPtr; c.SetRenderPriority(priority); })
        .method("int GetRenderPriority() const", [](const ScriptComponentHandle<TextRenderer> &cHandle) -> int32_t {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int32_t>(); }
            const TextRenderer &c = *cPtr; return c.GetRenderPriority(); })
        .method("void SetAllowInstancing(bool)", SafeCall<&TextRenderer::SetAllowInstancing>())
        .method("bool GetAllowInstancing() const", SafeCall<&TextRenderer::GetAllowInstancing>())
        .method("void SetPipelineName(const string &in)", SafeCall<&TextRenderer::SetPipelineName>())
        .method("const string &GetPipelineName() const", SafeCall<&TextRenderer::GetPipelineName>())
        .method("uint64 GetCharacterCount() const", [](const ScriptComponentHandle<TextRenderer> &cHandle) -> uint64_t {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint64_t>(); }
            const TextRenderer &c = *cPtr; return static_cast<uint64_t>(c.GetCharacterCount()); })
        .method("void SetCharacterOffset(uint64, const Vector2 &in)", [](ScriptComponentHandle<TextRenderer> &cHandle, uint64_t index, const Vector2 &offset) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TextRenderer &c = *cPtr;
            c.SetCharacterOffset(static_cast<size_t>(index), offset);
        })
        .method("Vector2 GetCharacterOffset(uint64) const", [](ScriptComponentHandle<TextRenderer> &cHandle, uint64_t index) -> Vector2 {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector2>(); }
            const TextRenderer &c = *cPtr;
            return c.GetCharacterOffset(static_cast<size_t>(index));
        })
        .method("void SetCharacterRotation(uint64, float)", [](ScriptComponentHandle<TextRenderer> &cHandle, uint64_t index, float rotation) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TextRenderer &c = *cPtr;
            c.SetCharacterRotation(static_cast<size_t>(index), rotation);
        })
        .method("float GetCharacterRotation(uint64) const", [](ScriptComponentHandle<TextRenderer> &cHandle, uint64_t index) -> float {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const TextRenderer &c = *cPtr;
            return c.GetCharacterRotation(static_cast<size_t>(index));
        })
        .method("void SetCharacterScale(uint64, const Vector2 &in)", [](ScriptComponentHandle<TextRenderer> &cHandle, uint64_t index, const Vector2 &scale) {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            TextRenderer &c = *cPtr;
            c.SetCharacterScale(static_cast<size_t>(index), scale);
        })
        .method("Vector2 GetCharacterScale(uint64) const", [](ScriptComponentHandle<TextRenderer> &cHandle, uint64_t index) -> Vector2 {
            TextRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector2>(); }
            const TextRenderer &c = *cPtr;
            return c.GetCharacterScale(static_cast<size_t>(index));
        });

    RegisterComponentType<Comment>(engine, "Comment")
        .method("void SetComment(const string &in)", SafeCall<&Comment::SetComment>())
        .method("const string &GetComment() const", SafeCall<&Comment::GetComment>());

    RegisterComponentType<ComputeShaderProcessing>(engine, "ComputeShaderProcessing")
        .method("void SetPipelineName(const string &in)", SafeCall<&ComputeShaderProcessing::SetPipelineName>())
        .method("const string &GetPipelineName() const", SafeCall<&ComputeShaderProcessing::GetPipelineName>())
        .method("void SetGroupCounts(uint, uint, uint)", SafeCall<&ComputeShaderProcessing::SetGroupCounts>())
        .method("void GetGroupCounts(uint &out, uint &out, uint &out) const", SafeCall<&ComputeShaderProcessing::GetGroupCounts>());

    RegisterComponentType<RigidBody2D>(engine, "RigidBody2D")
        .method("void SetVelocity(const Vector2 &in)", SafeCall<&RigidBody2D::SetVelocity>())
        .method("Vector2 GetVelocity() const", SafeCall<&RigidBody2D::GetVelocity>())
        .method("void SetAngularVelocity(float)", SafeCall<&RigidBody2D::SetAngularVelocity>())
        .method("float GetAngularVelocity() const", SafeCall<&RigidBody2D::GetAngularVelocity>())
        .method("void SetMass(float)", SafeCall<&RigidBody2D::SetMass>())
        .method("float GetMass() const", SafeCall<&RigidBody2D::GetMass>())
        .method("void SetUseGravity(bool)", SafeCall<&RigidBody2D::SetUseGravity>())
        .method("bool IsGravityEnabled() const", SafeCall<&RigidBody2D::IsGravityEnabled>())
        .method("void SetSelectedCollider(Collider@)", [](ScriptComponentHandle<RigidBody2D> &rbHandle, ScriptComponentHandle<ICollider> *colliderHandle) {
            RigidBody2D *rbPtr = rbHandle.Resolve();
            if (!rbPtr) { ThrowDestroyedObjectException(); return; }
            ICollider *collider = colliderHandle ? colliderHandle->Resolve() : nullptr;
            if (colliderHandle && !collider) { ThrowDestroyedObjectException(); return; }
            rbPtr->SetSelectedCollider(collider); })
        .method("Collider@ GetSelectedCollider() const", [](const ScriptComponentHandle<RigidBody2D> &rbHandle) -> ScriptComponentHandle<ICollider> * {
            RigidBody2D *rbPtr = rbHandle.Resolve();
            if (!rbPtr) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<ICollider>::Create(rbPtr->GetSelectedCollider()); })
        .method("array<Collider@>@ GetOwnerColliders() const", [](const ScriptComponentHandle<RigidBody2D> &rbHandle) -> CScriptArray * {
            RigidBody2D *rbPtr = rbHandle.Resolve();
            if (!rbPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<CScriptArray *>(); }
            const RigidBody2D &rb = *rbPtr; return MakeColliderArray(rb.GetOwnerColliders()); });

    RegisterComponentType<RigidBody3D>(engine, "RigidBody3D")
        .method("void SetBodyType(int)", [](ScriptComponentHandle<RigidBody3D> &rbHandle, int type) {
            RigidBody3D *rbPtr = rbHandle.Resolve();
            if (!rbPtr) { ThrowDestroyedObjectException(); return; }
            RigidBody3D &rb = *rbPtr; rb.SetBodyType(static_cast<reactphysics3d::BodyType>(type)); })
        .method("int GetBodyType() const", [](const ScriptComponentHandle<RigidBody3D> &rbHandle) -> int {
            RigidBody3D *rbPtr = rbHandle.Resolve();
            if (!rbPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const RigidBody3D &rb = *rbPtr; return static_cast<int>(rb.GetBodyType()); })
        .method("void SetMass(float)", SafeCall<&RigidBody3D::SetMass>())
        .method("float GetMass() const", SafeCall<&RigidBody3D::GetMass>())
        .method("void SetUseGravity(bool)", SafeCall<&RigidBody3D::SetUseGravity>())
        .method("bool IsGravityEnabled() const", SafeCall<&RigidBody3D::IsGravityEnabled>())
        .method("void SetInterpolate(bool)", SafeCall<&RigidBody3D::SetInterpolate>())
        .method("bool IsInterpolateEnabled() const", SafeCall<&RigidBody3D::IsInterpolateEnabled>())
        .method("void SetVelocity(const Vector3 &in)", SafeCall<&RigidBody3D::SetVelocity>())
        .method("Vector3 GetVelocity() const", SafeCall<&RigidBody3D::GetVelocity>())
        .method("void SyncFromTransform()", SafeCall<&RigidBody3D::SyncFromTransform>())
        .method("void SetSelectedCollider(Collider@)", [](ScriptComponentHandle<RigidBody3D> &rbHandle, ScriptComponentHandle<ICollider> *colliderHandle) {
            RigidBody3D *rbPtr = rbHandle.Resolve();
            if (!rbPtr) { ThrowDestroyedObjectException(); return; }
            ICollider *collider = colliderHandle ? colliderHandle->Resolve() : nullptr;
            if (colliderHandle && !collider) { ThrowDestroyedObjectException(); return; }
            rbPtr->SetSelectedCollider(collider); })
        .method("Collider@ GetSelectedCollider() const", [](const ScriptComponentHandle<RigidBody3D> &rbHandle) -> ScriptComponentHandle<ICollider> * {
            RigidBody3D *rbPtr = rbHandle.Resolve();
            if (!rbPtr) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<ICollider>::Create(rbPtr->GetSelectedCollider()); })
        .method("array<Collider@>@ GetOwnerColliders() const", [](const ScriptComponentHandle<RigidBody3D> &rbHandle) -> CScriptArray * {
            RigidBody3D *rbPtr = rbHandle.Resolve();
            if (!rbPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<CScriptArray *>(); }
            const RigidBody3D &rb = *rbPtr; return MakeColliderArray(rb.GetOwnerColliders()); });

    RegisterComponentType<MeshRenderer>(engine, "MeshRenderer")
        .method("void SetPipelineName(const string &in)", SafeCall<&MeshRenderer::SetPipelineName>())
        .method("const string &GetPipelineName() const", SafeCall<&MeshRenderer::GetPipelineName>())
        .method("void SetMaterialName(const string &in)", SafeCall<&MeshRenderer::SetMaterialName>())
        .method("const string &GetMaterialName() const", SafeCall<&MeshRenderer::GetMaterialName>())
        .method("void SetMaterialHandle(uint)", [](ScriptComponentHandle<MeshRenderer> &cHandle, uint32_t handle) {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshRenderer &c = *cPtr; c.SetMaterialHandle(handle); })
        .method("uint GetMaterialHandle() const", [](const ScriptComponentHandle<MeshRenderer> &cHandle) -> uint32_t {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const MeshRenderer &c = *cPtr; return c.GetMaterialHandle(); })
        .method("uint GetMaterialSlotCount() const", [](const ScriptComponentHandle<MeshRenderer> &cHandle) -> uint32_t {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const MeshRenderer &c = *cPtr; return static_cast<uint32_t>(c.GetMaterialSlotCount()); })
        .method("void SetMaterialSlotCount(uint)", [](ScriptComponentHandle<MeshRenderer> &cHandle, uint32_t count) {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshRenderer &c = *cPtr; c.SetMaterialSlotCount(count); })
        .method("void SetMaterialNameAt(uint, const string &in)", [](ScriptComponentHandle<MeshRenderer> &cHandle, uint32_t slot, const std::string &name) {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshRenderer &c = *cPtr; c.SetMaterialNameAt(slot, name); })
        .method("const string &GetMaterialNameAt(uint) const", [](ScriptComponentHandle<MeshRenderer> &cHandle, uint32_t slot) -> const std::string & {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<const std::string &>(); }
            const MeshRenderer &c = *cPtr; return c.GetMaterialNameAt(slot); })
        .method("uint GetMaterialHandleAt(uint) const", [](ScriptComponentHandle<MeshRenderer> &cHandle, uint32_t slot) -> uint32_t {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const MeshRenderer &c = *cPtr; return c.GetMaterialHandleAt(slot); })
        .method("Object@ GetTargetObject() const", [](const ScriptComponentHandle<MeshRenderer> &cHandle) -> ScriptObjectHandle * {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const MeshRenderer &c = *cPtr; return ScriptObjectHandle::Create(c.GetTargetObject()); })
        .method("void SetTargetObject(Object@)", [](ScriptComponentHandle<MeshRenderer> &cHandle, ScriptObjectHandle *obj) {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshRenderer &c = *cPtr; c.SetTargetObject(ResolveObjectArg(obj)); })
        .method("string GetTargetObjectID() const", [](const ScriptComponentHandle<MeshRenderer> &cHandle) -> std::string {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const MeshRenderer &c = *cPtr; return c.GetTargetObjectID().ToString(); })
        .method("void SetInstanceColor(const Vector4 &in)", SafeCall<&MeshRenderer::SetInstanceColor>())
        .method("const Vector4 &GetInstanceColor() const", SafeCall<&MeshRenderer::GetInstanceColor>())
        // instanceColorBlendModeは 0=Override, 1=Multiply, 2=Add, 3=Subtract
        .method("void SetInstanceColorBlendMode(int)", [](ScriptComponentHandle<MeshRenderer> &cHandle, int mode) {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshRenderer &c = *cPtr; c.SetInstanceColorBlendMode(static_cast<MeshRenderer::ColorBlendMode>(mode)); })
        .method("int GetInstanceColorBlendMode() const", [](const ScriptComponentHandle<MeshRenderer> &cHandle) {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const MeshRenderer &c = *cPtr; return static_cast<int>(c.GetInstanceColorBlendMode()); })
        .method("void SetInstanceUvTranslate(const Vector2 &in)", SafeCall<&MeshRenderer::SetInstanceUvTranslate>())
        .method("const Vector2 &GetInstanceUvTranslate() const", SafeCall<&MeshRenderer::GetInstanceUvTranslate>())
        .method("void SetInstanceUvRotation(float)", SafeCall<&MeshRenderer::SetInstanceUvRotation>())
        .method("float GetInstanceUvRotation() const", SafeCall<&MeshRenderer::GetInstanceUvRotation>())
        .method("void SetInstanceUvScale(const Vector2 &in)", SafeCall<&MeshRenderer::SetInstanceUvScale>())
        .method("const Vector2 &GetInstanceUvScale() const", SafeCall<&MeshRenderer::GetInstanceUvScale>())
        .method("void SetInstanceUvPivot(const Vector2 &in)", SafeCall<&MeshRenderer::SetInstanceUvPivot>())
        .method("const Vector2 &GetInstanceUvPivot() const", SafeCall<&MeshRenderer::GetInstanceUvPivot>())
        // instanceUvCombineModeは 0=MaterialThenInstance, 1=InstanceThenMaterial, 2=InstanceOnly
        .method("void SetInstanceUvCombineMode(int)", [](ScriptComponentHandle<MeshRenderer> &cHandle, int mode) {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshRenderer &c = *cPtr; c.SetInstanceUvCombineMode(static_cast<MeshRenderer::UVCombineMode>(mode)); })
        .method("int GetInstanceUvCombineMode() const", [](const ScriptComponentHandle<MeshRenderer> &cHandle) {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const MeshRenderer &c = *cPtr; return static_cast<int>(c.GetInstanceUvCombineMode()); })
        .method("void SetRenderPriority(int)", [](ScriptComponentHandle<MeshRenderer> &cHandle, int32_t priority) {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshRenderer &c = *cPtr; c.SetRenderPriority(priority); })
        .method("int GetRenderPriority() const", [](const ScriptComponentHandle<MeshRenderer> &cHandle) -> int32_t {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int32_t>(); }
            const MeshRenderer &c = *cPtr; return c.GetRenderPriority(); })
        .method("void SetAllowInstancing(bool)", SafeCall<&MeshRenderer::SetAllowInstancing>())
        .method("bool GetAllowInstancing() const", SafeCall<&MeshRenderer::GetAllowInstancing>())
        .method("void SetCastShadows(bool)", SafeCall<&MeshRenderer::SetCastShadows>())
        .method("bool GetCastShadows() const", SafeCall<&MeshRenderer::GetCastShadows>())
        .method("uint GetMeshHandle() const", [](const ScriptComponentHandle<MeshRenderer> &cHandle) -> uint32_t {
            MeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const MeshRenderer &c = *cPtr; return c.GetMeshHandle(); })
        .method("Matrix4x4 GetWorldMatrix() const", SafeCall<&MeshRenderer::GetWorldMatrix>());

    RegisterComponentType<SkinnedMeshRenderer>(engine, "SkinnedMeshRenderer")
        .method("void SetPipelineName(const string &in)", SafeCall<&SkinnedMeshRenderer::SetPipelineName>())
        .method("const string &GetPipelineName() const", SafeCall<&SkinnedMeshRenderer::GetPipelineName>())
        .method("void SetMaterialName(const string &in)", SafeCall<&SkinnedMeshRenderer::SetMaterialName>())
        .method("const string &GetMaterialName() const", SafeCall<&SkinnedMeshRenderer::GetMaterialName>())
        .method("void SetMaterialHandle(uint)", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, uint32_t handle) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SkinnedMeshRenderer &c = *cPtr; c.SetMaterialHandle(handle); })
        .method("uint GetMaterialHandle() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) -> uint32_t {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const SkinnedMeshRenderer &c = *cPtr; return c.GetMaterialHandle(); })
        .method("uint GetMaterialSlotCount() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) -> uint32_t {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const SkinnedMeshRenderer &c = *cPtr; return static_cast<uint32_t>(c.GetMaterialSlotCount()); })
        .method("void SetMaterialSlotCount(uint)", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, uint32_t count) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SkinnedMeshRenderer &c = *cPtr; c.SetMaterialSlotCount(count); })
        .method("void SetMaterialNameAt(uint, const string &in)", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, uint32_t slot, const std::string &name) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SkinnedMeshRenderer &c = *cPtr; c.SetMaterialNameAt(slot, name); })
        .method("const string &GetMaterialNameAt(uint) const", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, uint32_t slot) -> const std::string & {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<const std::string &>(); }
            const SkinnedMeshRenderer &c = *cPtr; return c.GetMaterialNameAt(slot); })
        .method("uint GetMaterialHandleAt(uint) const", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, uint32_t slot) -> uint32_t {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const SkinnedMeshRenderer &c = *cPtr; return c.GetMaterialHandleAt(slot); })
        .method("void SetBlendShapeWeight(const string &in, float)", SafeCall<&SkinnedMeshRenderer::SetBlendShapeWeight>())
        .method("float GetBlendShapeWeight(const string &in) const", SafeCall<&SkinnedMeshRenderer::GetBlendShapeWeight>())
        .method("uint GetBlendShapeCount() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) -> uint32_t {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const SkinnedMeshRenderer &c = *cPtr; return static_cast<uint32_t>(c.GetBlendShapes().size()); })
        .method("string GetBlendShapeNameAt(uint) const", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, uint32_t index) -> std::string {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const SkinnedMeshRenderer &c = *cPtr;
            const auto &shapes = c.GetBlendShapes();
            return index < shapes.size() ? shapes[index].name : std::string();
        })
        .method("float GetBlendShapeWeightAt(uint) const", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, uint32_t index) -> float {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const SkinnedMeshRenderer &c = *cPtr;
            const auto &shapes = c.GetBlendShapes();
            return index < shapes.size() ? shapes[index].weight : 0.0f;
        })
        .method("void SetInstanceColor(const Vector4 &in)", SafeCall<&SkinnedMeshRenderer::SetInstanceColor>())
        .method("const Vector4 &GetInstanceColor() const", SafeCall<&SkinnedMeshRenderer::GetInstanceColor>())
        // instanceColorBlendModeは 0=Override, 1=Multiply, 2=Add, 3=Subtract
        .method("void SetInstanceColorBlendMode(int)", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, int mode) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SkinnedMeshRenderer &c = *cPtr; c.SetInstanceColorBlendMode(static_cast<SkinnedMeshRenderer::ColorBlendMode>(mode)); })
        .method("int GetInstanceColorBlendMode() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const SkinnedMeshRenderer &c = *cPtr; return static_cast<int>(c.GetInstanceColorBlendMode()); })
        .method("void SetInstanceUvTranslate(const Vector2 &in)", SafeCall<&SkinnedMeshRenderer::SetInstanceUvTranslate>())
        .method("const Vector2 &GetInstanceUvTranslate() const", SafeCall<&SkinnedMeshRenderer::GetInstanceUvTranslate>())
        .method("void SetInstanceUvRotation(float)", SafeCall<&SkinnedMeshRenderer::SetInstanceUvRotation>())
        .method("float GetInstanceUvRotation() const", SafeCall<&SkinnedMeshRenderer::GetInstanceUvRotation>())
        .method("void SetInstanceUvScale(const Vector2 &in)", SafeCall<&SkinnedMeshRenderer::SetInstanceUvScale>())
        .method("const Vector2 &GetInstanceUvScale() const", SafeCall<&SkinnedMeshRenderer::GetInstanceUvScale>())
        .method("void SetInstanceUvPivot(const Vector2 &in)", SafeCall<&SkinnedMeshRenderer::SetInstanceUvPivot>())
        .method("const Vector2 &GetInstanceUvPivot() const", SafeCall<&SkinnedMeshRenderer::GetInstanceUvPivot>())
        // instanceUvCombineModeは 0=MaterialThenInstance, 1=InstanceThenMaterial, 2=InstanceOnly
        .method("void SetInstanceUvCombineMode(int)", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, int mode) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SkinnedMeshRenderer &c = *cPtr; c.SetInstanceUvCombineMode(static_cast<SkinnedMeshRenderer::UVCombineMode>(mode)); })
        .method("int GetInstanceUvCombineMode() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const SkinnedMeshRenderer &c = *cPtr; return static_cast<int>(c.GetInstanceUvCombineMode()); })
        .method("void SetRenderPriority(int)", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, int32_t priority) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SkinnedMeshRenderer &c = *cPtr; c.SetRenderPriority(priority); })
        .method("int GetRenderPriority() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) -> int32_t {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int32_t>(); }
            const SkinnedMeshRenderer &c = *cPtr; return c.GetRenderPriority(); })
        .method("void SetAllowInstancing(bool)", SafeCall<&SkinnedMeshRenderer::SetAllowInstancing>())
        .method("bool GetAllowInstancing() const", SafeCall<&SkinnedMeshRenderer::GetAllowInstancing>())
        .method("void SetCastShadows(bool)", SafeCall<&SkinnedMeshRenderer::SetCastShadows>())
        .method("bool GetCastShadows() const", SafeCall<&SkinnedMeshRenderer::GetCastShadows>())
        .method("Object@ GetTargetObject() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) -> ScriptObjectHandle * {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const SkinnedMeshRenderer &c = *cPtr; return ScriptObjectHandle::Create(c.GetTargetObject()); })
        .method("void SetTargetObject(Object@)", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, ScriptObjectHandle *obj) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SkinnedMeshRenderer &c = *cPtr; c.SetTargetObject(ResolveObjectArg(obj)); })
        .method("string GetTargetObjectID() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) -> std::string {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const SkinnedMeshRenderer &c = *cPtr; return c.GetTargetObjectID().ToString(); })
        .method("void SetQuality(SkinQuality)", [](ScriptComponentHandle<SkinnedMeshRenderer> &cHandle, int quality) {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            SkinnedMeshRenderer &c = *cPtr; c.SetQuality(static_cast<SkinQuality>(quality)); })
        .method("SkinQuality GetQuality() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) -> int {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const SkinnedMeshRenderer &c = *cPtr; return static_cast<int>(c.GetQuality()); })
        .method("Animator@ GetAnimator() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) -> ScriptComponentHandle<Animator> * {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<Animator>::Create(cPtr->GetAnimator());
        })
        .method("void ResetAnimationToBindPose()", SafeCall<&SkinnedMeshRenderer::ResetAnimationToBindPose>())
        .method("uint GetMeshHandle() const", [](const ScriptComponentHandle<SkinnedMeshRenderer> &cHandle) -> uint32_t {
            SkinnedMeshRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const SkinnedMeshRenderer &c = *cPtr; return c.GetMeshHandle(); })
        .method("Matrix4x4 GetWorldMatrix() const", SafeCall<&SkinnedMeshRenderer::GetWorldMatrix>());

    RegisterComponentType<Camera2D>(engine, "Camera2D")
        .method("void SetSize(float, float)", SafeCall<&Camera2D::SetSize>())
        .method("void SetNearClip(float)", SafeCall<&Camera2D::SetNearClip>())
        .method("void SetFarClip(float)", SafeCall<&Camera2D::SetFarClip>())
        .method("float GetWidth() const", SafeCall<&Camera2D::GetWidth>())
        .method("float GetHeight() const", SafeCall<&Camera2D::GetHeight>())
        .method("float GetNearClip() const", SafeCall<&Camera2D::GetNearClip>())
        .method("float GetFarClip() const", SafeCall<&Camera2D::GetFarClip>())
        .method("void SetAutoSyncSize(bool)", SafeCall<&Camera2D::SetAutoSyncSize>())
        .method("bool GetAutoSyncSize() const", SafeCall<&Camera2D::GetAutoSyncSize>());

    RegisterComponentType<CameraRenderer>(engine, "CameraRenderer")
        .method("void SetPipelineName(const string &in)", SafeCall<&CameraRenderer::SetPipelineName>())
        .method("const string &GetPipelineName() const", SafeCall<&CameraRenderer::GetPipelineName>())
        .method("void SetTargetObject(Object@)", [](ScriptComponentHandle<CameraRenderer> &cHandle, ScriptObjectHandle *obj) {
            CameraRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraRenderer &c = *cPtr; c.SetTargetObject(ResolveObjectArg(obj)); })
        .method("Object@ GetTargetObject() const", [](const ScriptComponentHandle<CameraRenderer> &cHandle) -> ScriptObjectHandle * {
            CameraRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const CameraRenderer &c = *cPtr; return ScriptObjectHandle::Create(c.GetTargetObject()); })
        .method("string GetTargetObjectID() const", [](const ScriptComponentHandle<CameraRenderer> &cHandle) -> std::string {
            CameraRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const CameraRenderer &c = *cPtr; return c.GetTargetObjectID().ToString(); })
        .method("void SetBindVariableNames(array<string>@)", [](ScriptComponentHandle<CameraRenderer> &cHandle, CScriptArray *names) {
            CameraRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraRenderer &c = *cPtr; c.SetBindVariableNames(StringArrayToVector(names)); })
        .method("array<string>@ GetBindVariableNames() const", [](const ScriptComponentHandle<CameraRenderer> &cHandle) -> CScriptArray * {
            CameraRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<CScriptArray *>(); }
            const CameraRenderer &c = *cPtr; return MakeStringArray(c.GetBindVariableNames()); })
        .method("Vector3 GetWorldPosition() const", SafeCall<&CameraRenderer::GetWorldPosition>())
        .method("const Matrix4x4 &GetViewProjectionMatrix() const", SafeCall<&CameraRenderer::GetViewProjectionMatrix>())
        .method("float GetNearClip() const", SafeCall<&CameraRenderer::GetNearClip>())
        .method("float GetFarClip() const", SafeCall<&CameraRenderer::GetFarClip>());

    RegisterComponentType<CameraController>(engine, "CameraController")
        .method("bool IsControllable() const", SafeCall<&CameraController::IsControllable>())
        .method("void AddFollowTarget(Object@)", [](ScriptComponentHandle<CameraController> &cHandle, ScriptObjectHandle *obj) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr;
            EmptyObject *resolved = ResolveObjectArg(obj);
            if (resolved) c.AddFollowTarget(resolved->GetObjectID());
        })
        .method("void RemoveFollowTarget(uint)", [](ScriptComponentHandle<CameraController> &cHandle, uint32_t index) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr; c.RemoveFollowTarget(index); })
        .method("uint GetFollowTargetCount() const", [](const ScriptComponentHandle<CameraController> &cHandle) -> uint32_t {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const CameraController &c = *cPtr;
            return static_cast<uint32_t>(c.GetFollowTargets().size());
        })
        .method("Object@ GetFollowTargetObject(uint) const", [](ScriptComponentHandle<CameraController> &cHandle, uint32_t index) -> ScriptObjectHandle * {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const CameraController &c = *cPtr;
            const auto &targets = c.GetFollowTargets();
            if (index >= targets.size() || !gCurrentSceneContext) return nullptr;
            return ScriptObjectHandle::Create(gCurrentSceneContext->GetSceneObject(targets[index].objectID));
        })
        .method("void SetFollowPositionEnable(uint, bool, bool, bool)", [](ScriptComponentHandle<CameraController> &cHandle, uint32_t index, bool x, bool y, bool z) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr;
            auto &targets = c.GetFollowTargets();
            if (index >= targets.size()) return;
            targets[index].followPositionX = x;
            targets[index].followPositionY = y;
            targets[index].followPositionZ = z;
        })
        .method("void GetFollowPositionEnable(uint, bool &out, bool &out, bool &out) const", [](ScriptComponentHandle<CameraController> &cHandle, uint32_t index, bool &x, bool &y, bool &z) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            const CameraController &c = *cPtr;
            x = y = z = false;
            const auto &targets = c.GetFollowTargets();
            if (index >= targets.size()) return;
            x = targets[index].followPositionX;
            y = targets[index].followPositionY;
            z = targets[index].followPositionZ;
        })
        .method("void SetFollowRotationEnable(uint, bool, bool, bool)", [](ScriptComponentHandle<CameraController> &cHandle, uint32_t index, bool x, bool y, bool z) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr;
            auto &targets = c.GetFollowTargets();
            if (index >= targets.size()) return;
            targets[index].followRotationX = x;
            targets[index].followRotationY = y;
            targets[index].followRotationZ = z;
        })
        .method("void GetFollowRotationEnable(uint, bool &out, bool &out, bool &out) const", [](ScriptComponentHandle<CameraController> &cHandle, uint32_t index, bool &x, bool &y, bool &z) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            const CameraController &c = *cPtr;
            x = y = z = false;
            const auto &targets = c.GetFollowTargets();
            if (index >= targets.size()) return;
            x = targets[index].followRotationX;
            y = targets[index].followRotationY;
            z = targets[index].followRotationZ;
        })
        .method("void SetPositionOffset(const Vector3 &in)", SafeCall<&CameraController::SetPositionOffset>())
        .method("const Vector3 &GetPositionOffset() const", SafeCall<&CameraController::GetPositionOffset>())
        .method("void SetRotationOffset(const Vector3 &in)", SafeCall<&CameraController::SetRotationOffset>())
        .method("const Vector3 &GetRotationOffset() const", SafeCall<&CameraController::GetRotationOffset>())
        .method("void SetTargetFovY(float)", SafeCall<&CameraController::SetTargetFovY>())
        .method("float GetTargetFovY() const", SafeCall<&CameraController::GetTargetFovY>())
        .method("void SetMoveStrength(float)", [](ScriptComponentHandle<CameraController> &cHandle, float v) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr;
            c.GetMoveStrength().usePerAxis = false;
            c.GetMoveStrength().all = v;
        })
        .method("float GetMoveStrength() const", [](const ScriptComponentHandle<CameraController> &cHandle) -> float {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const CameraController &c = *cPtr; return c.GetMoveStrength().all; })
        .method("void SetMoveStrengthPerAxis(const Vector3 &in)", [](ScriptComponentHandle<CameraController> &cHandle, const Vector3 &v) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr;
            c.GetMoveStrength().usePerAxis = true;
            c.GetMoveStrength().perAxis = v;
        })
        .method("Vector3 GetMoveStrengthPerAxis() const", [](const ScriptComponentHandle<CameraController> &cHandle) -> Vector3 {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector3>(); }
            const CameraController &c = *cPtr; return c.GetMoveStrength().perAxis; })
        .method("void SetMoveStrengthUsePerAxis(bool)", [](ScriptComponentHandle<CameraController> &cHandle, bool usePerAxis) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr; c.GetMoveStrength().usePerAxis = usePerAxis; })
        .method("bool GetMoveStrengthUsePerAxis() const", [](const ScriptComponentHandle<CameraController> &cHandle) -> bool {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const CameraController &c = *cPtr; return c.GetMoveStrength().usePerAxis; })
        .method("void SetRotateStrength(float)", [](ScriptComponentHandle<CameraController> &cHandle, float v) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr;
            c.GetRotateStrength().usePerAxis = false;
            c.GetRotateStrength().all = v;
        })
        .method("float GetRotateStrength() const", [](const ScriptComponentHandle<CameraController> &cHandle) -> float {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const CameraController &c = *cPtr; return c.GetRotateStrength().all; })
        .method("void SetRotateStrengthPerAxis(const Vector3 &in)", [](ScriptComponentHandle<CameraController> &cHandle, const Vector3 &v) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr;
            c.GetRotateStrength().usePerAxis = true;
            c.GetRotateStrength().perAxis = v;
        })
        .method("Vector3 GetRotateStrengthPerAxis() const", [](const ScriptComponentHandle<CameraController> &cHandle) -> Vector3 {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector3>(); }
            const CameraController &c = *cPtr; return c.GetRotateStrength().perAxis; })
        .method("void SetRotateStrengthUsePerAxis(bool)", [](ScriptComponentHandle<CameraController> &cHandle, bool usePerAxis) {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController &c = *cPtr; c.GetRotateStrength().usePerAxis = usePerAxis; })
        .method("bool GetRotateStrengthUsePerAxis() const", [](const ScriptComponentHandle<CameraController> &cHandle) -> bool {
            CameraController *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const CameraController &c = *cPtr; return c.GetRotateStrength().usePerAxis; })
        .method("void SetFovLerpFactor(float)", SafeCall<&CameraController::SetFovLerpFactor>())
        .method("float GetFovLerpFactor() const", SafeCall<&CameraController::GetFovLerpFactor>());

    RegisterComponentType<CameraController2D>(engine, "CameraController2D")
        .method("bool IsControllable() const", SafeCall<&CameraController2D::IsControllable>())
        .method("void AddFollowTarget(Object@)", [](ScriptComponentHandle<CameraController2D> &cHandle, ScriptObjectHandle *obj) {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController2D &c = *cPtr;
            EmptyObject *resolved = ResolveObjectArg(obj);
            if (resolved) c.AddFollowTarget(resolved->GetObjectID());
        })
        .method("void RemoveFollowTarget(uint)", [](ScriptComponentHandle<CameraController2D> &cHandle, uint32_t index) {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController2D &c = *cPtr; c.RemoveFollowTarget(index); })
        .method("uint GetFollowTargetCount() const", [](const ScriptComponentHandle<CameraController2D> &cHandle) -> uint32_t {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const CameraController2D &c = *cPtr;
            return static_cast<uint32_t>(c.GetFollowTargets().size());
        })
        .method("Object@ GetFollowTargetObject(uint) const", [](ScriptComponentHandle<CameraController2D> &cHandle, uint32_t index) -> ScriptObjectHandle * {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const CameraController2D &c = *cPtr;
            const auto &targets = c.GetFollowTargets();
            if (index >= targets.size() || !gCurrentSceneContext) return nullptr;
            return ScriptObjectHandle::Create(gCurrentSceneContext->GetSceneObject(targets[index].objectID));
        })
        .method("void SetFollowPositionEnable(uint, bool, bool)", [](ScriptComponentHandle<CameraController2D> &cHandle, uint32_t index, bool x, bool y) {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController2D &c = *cPtr;
            auto &targets = c.GetFollowTargets();
            if (index >= targets.size()) return;
            targets[index].followPositionX = x;
            targets[index].followPositionY = y;
        })
        .method("void GetFollowPositionEnable(uint, bool &out, bool &out) const", [](ScriptComponentHandle<CameraController2D> &cHandle, uint32_t index, bool &x, bool &y) {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            const CameraController2D &c = *cPtr;
            x = y = false;
            const auto &targets = c.GetFollowTargets();
            if (index >= targets.size()) return;
            x = targets[index].followPositionX;
            y = targets[index].followPositionY;
        })
        .method("void SetFollowRotationEnable(uint, bool)", [](ScriptComponentHandle<CameraController2D> &cHandle, uint32_t index, bool z) {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController2D &c = *cPtr;
            auto &targets = c.GetFollowTargets();
            if (index >= targets.size()) return;
            targets[index].followRotationZ = z;
        })
        .method("bool GetFollowRotationEnable(uint) const", [](ScriptComponentHandle<CameraController2D> &cHandle, uint32_t index) -> bool {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const CameraController2D &c = *cPtr;
            const auto &targets = c.GetFollowTargets();
            if (index >= targets.size()) return false;
            return targets[index].followRotationZ;
        })
        .method("void SetPositionOffset(const Vector2 &in)", SafeCall<&CameraController2D::SetPositionOffset>())
        .method("const Vector2 &GetPositionOffset() const", SafeCall<&CameraController2D::GetPositionOffset>())
        .method("void SetRotationOffset(float)", SafeCall<&CameraController2D::SetRotationOffset>())
        .method("float GetRotationOffset() const", SafeCall<&CameraController2D::GetRotationOffset>())
        .method("void SetTargetSize(const Vector2 &in)", SafeCall<&CameraController2D::SetTargetSize>())
        .method("const Vector2 &GetTargetSize() const", SafeCall<&CameraController2D::GetTargetSize>())
        .method("void SetMoveStrength(float)", [](ScriptComponentHandle<CameraController2D> &cHandle, float v) {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController2D &c = *cPtr;
            c.GetMoveStrength().usePerAxis = false;
            c.GetMoveStrength().all = v;
        })
        .method("float GetMoveStrength() const", [](const ScriptComponentHandle<CameraController2D> &cHandle) -> float {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const CameraController2D &c = *cPtr; return c.GetMoveStrength().all; })
        .method("void SetMoveStrengthPerAxis(const Vector2 &in)", [](ScriptComponentHandle<CameraController2D> &cHandle, const Vector2 &v) {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController2D &c = *cPtr;
            c.GetMoveStrength().usePerAxis = true;
            c.GetMoveStrength().perAxis = v;
        })
        .method("Vector2 GetMoveStrengthPerAxis() const", [](const ScriptComponentHandle<CameraController2D> &cHandle) -> Vector2 {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector2>(); }
            const CameraController2D &c = *cPtr; return c.GetMoveStrength().perAxis; })
        .method("void SetMoveStrengthUsePerAxis(bool)", [](ScriptComponentHandle<CameraController2D> &cHandle, bool usePerAxis) {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            CameraController2D &c = *cPtr; c.GetMoveStrength().usePerAxis = usePerAxis; })
        .method("bool GetMoveStrengthUsePerAxis() const", [](const ScriptComponentHandle<CameraController2D> &cHandle) -> bool {
            CameraController2D *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const CameraController2D &c = *cPtr; return c.GetMoveStrength().usePerAxis; })
        .method("void SetRotateLerpFactor(float)", SafeCall<&CameraController2D::SetRotateLerpFactor>())
        .method("float GetRotateLerpFactor() const", SafeCall<&CameraController2D::GetRotateLerpFactor>())
        .method("void SetSizeLerpFactor(float)", SafeCall<&CameraController2D::SetSizeLerpFactor>())
        .method("float GetSizeLerpFactor() const", SafeCall<&CameraController2D::GetSizeLerpFactor>());

    RegisterComponentType<Light>(engine, "Light")
        .method("void SetType(LightType)", SafeCall<&Light::SetType>())
        .method("LightType GetType() const", SafeCall<&Light::GetType>())
        .method("void SetColor(const Vector4 &in)", SafeCall<&Light::SetColor>())
        .method("const Vector4 &GetColor() const", SafeCall<&Light::GetColor>())
        .method("void SetIntensity(float)", SafeCall<&Light::SetIntensity>())
        .method("float GetIntensity() const", SafeCall<&Light::GetIntensity>())
        .method("void SetRadius(float)", SafeCall<&Light::SetRadius>())
        .method("float GetRadius() const", SafeCall<&Light::GetRadius>())
        .method("void SetDistance(float)", SafeCall<&Light::SetDistance>())
        .method("float GetDistance() const", SafeCall<&Light::GetDistance>())
        .method("void SetDecay(float)", SafeCall<&Light::SetDecay>())
        .method("float GetDecay() const", SafeCall<&Light::GetDecay>())
        .method("void SetInnerAngle(float)", SafeCall<&Light::SetInnerAngle>())
        .method("float GetInnerAngle() const", SafeCall<&Light::GetInnerAngle>())
        .method("void SetOuterAngle(float)", SafeCall<&Light::SetOuterAngle>())
        .method("float GetOuterAngle() const", SafeCall<&Light::GetOuterAngle>())
        .method("void SetSourceRadius(float)", SafeCall<&Light::SetSourceRadius>())
        .method("float GetSourceRadius() const", SafeCall<&Light::GetSourceRadius>())
        .method("void SetSourceWidth(float)", SafeCall<&Light::SetSourceWidth>())
        .method("float GetSourceWidth() const", SafeCall<&Light::GetSourceWidth>())
        .method("void SetSourceHeight(float)", SafeCall<&Light::SetSourceHeight>())
        .method("float GetSourceHeight() const", SafeCall<&Light::GetSourceHeight>())
        .method("void SetSourceLength(float)", SafeCall<&Light::SetSourceLength>())
        .method("float GetSourceLength() const", SafeCall<&Light::GetSourceLength>())
        .method("void SetSourceDepth(float)", SafeCall<&Light::SetSourceDepth>())
        .method("float GetSourceDepth() const", SafeCall<&Light::GetSourceDepth>())
        .method("void SetCastShadows(bool)", SafeCall<&Light::SetCastShadows>())
        .method("bool IsCastShadows() const", SafeCall<&Light::IsCastShadows>())
        .method("void SetShadowDistance(float)", SafeCall<&Light::SetShadowDistance>())
        .method("float GetShadowDistance() const", SafeCall<&Light::GetShadowDistance>())
        .method("void SetShadowMapResolution(uint)", SafeCall<&Light::SetShadowMapResolution>())
        .method("uint GetShadowMapResolution() const", SafeCall<&Light::GetShadowMapResolution>())
        .method("void SetShadowBias(float)", SafeCall<&Light::SetShadowBias>())
        .method("float GetShadowBias() const", SafeCall<&Light::GetShadowBias>())
        .method("void SetShadowSoftness(float)", SafeCall<&Light::SetShadowSoftness>())
        .method("float GetShadowSoftness() const", SafeCall<&Light::GetShadowSoftness>())
        .method("float GetEffectiveShadowSoftness() const", SafeCall<&Light::GetEffectiveShadowSoftness>());

    RegisterComponentType<LightRenderer>(engine, "LightRenderer")
        .method("void SetPipelineName(const string &in)", SafeCall<&LightRenderer::SetPipelineName>())
        .method("const string &GetPipelineName() const", SafeCall<&LightRenderer::GetPipelineName>())
        .method("void SetTargetObject(Object@)", [](ScriptComponentHandle<LightRenderer> &cHandle, ScriptObjectHandle *obj) {
            LightRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            LightRenderer &c = *cPtr; c.SetTargetObject(ResolveObjectArg(obj)); })
        .method("Object@ GetTargetObject() const", [](const ScriptComponentHandle<LightRenderer> &cHandle) -> ScriptObjectHandle * {
            LightRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const LightRenderer &c = *cPtr; return ScriptObjectHandle::Create(c.GetTargetObject()); })
        .method("string GetTargetObjectID() const", [](const ScriptComponentHandle<LightRenderer> &cHandle) -> std::string {
            LightRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const LightRenderer &c = *cPtr; return c.GetTargetObjectID().ToString(); })
        .method("Light@ GetLight() const", [](const ScriptComponentHandle<LightRenderer> &cHandle) -> ScriptComponentHandle<Light> * {
            LightRenderer *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<Light>::Create(cPtr->GetLight());
        })
        .method("LightType GetLightType() const", SafeCall<&LightRenderer::GetLightType>())
        .method("Vector3 GetWorldPosition() const", SafeCall<&LightRenderer::GetWorldPosition>())
        .method("Vector3 GetWorldDirection() const", SafeCall<&LightRenderer::GetWorldDirection>())
        .method("Vector3 GetWorldRight() const", SafeCall<&LightRenderer::GetWorldRight>())
        .method("Vector3 GetWorldUp() const", SafeCall<&LightRenderer::GetWorldUp>());

    RegisterComponentType<NormalWindowObject>(engine, "NormalWindowObject")
        .method("void SetTitle(const string &in)", SafeCall<static_cast<void (NormalWindowObject::*)(const std::string &)>(&NormalWindowObject::SetTitle)>())
        .method("const string &GetTitle() const", [](const ScriptComponentHandle<NormalWindowObject> &self) -> const std::string & {
            static const std::string kEmpty;
            NormalWindowObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetTitle();
        })
        .method("void SetSize(uint, uint)", SafeCall<static_cast<void (NormalWindowObject::*)(std::uint32_t, std::uint32_t)>(&NormalWindowObject::SetSize)>())
        .method("void SetSyncWithTransform(bool)", SafeCall<static_cast<void (NormalWindowObject::*)(bool) noexcept>(&NormalWindowObject::SetSyncWithTransform)>())
        .method("bool IsSyncWithTransformEnabled() const", SafeCall<static_cast<bool (NormalWindowObject::*)() const noexcept>(&NormalWindowObject::IsSyncWithTransformEnabled)>())
        .method("bool SetMessageIntercepted(uint msg, bool enabled)", SafeCall<static_cast<bool (NormalWindowObject::*)(std::uint32_t, bool)>(&NormalWindowObject::SetMessageIntercepted)>())
        .method("bool IsMessageIntercepted(uint msg) const", SafeCall<static_cast<bool (NormalWindowObject::*)(std::uint32_t) const>(&NormalWindowObject::IsMessageIntercepted)>())
        .method("void CloseWindow()", SafeCall<static_cast<void (NormalWindowObject::*)()>(&NormalWindowObject::CloseWindow)>())
        // 基底のWindowObject型への暗黙変換（WindowMessageInfoのsourceComponentとの比較用）
        .method("WindowObject@ opImplCast()", [](ScriptComponentHandle<NormalWindowObject> &self) -> ScriptComponentHandle<IWindowObjectComponent> * {
            NormalWindowObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<IWindowObjectComponent>::Create(static_cast<IWindowObjectComponent *>(obj));
        });
    engine->RegisterObjectMethod("WindowObject", "NormalWindowObject@ opCast()",
        asFUNCTION((WindowObjectDownCast<NormalWindowObject>)), asCALL_CDECL_OBJLAST);

    RegisterComponentType<OverlayWindowObject>(engine, "OverlayWindowObject")
        .method("void SetTitle(const string &in)", SafeCall<static_cast<void (OverlayWindowObject::*)(const std::string &)>(&OverlayWindowObject::SetTitle)>())
        .method("const string &GetTitle() const", [](const ScriptComponentHandle<OverlayWindowObject> &self) -> const std::string & {
            static const std::string kEmpty;
            OverlayWindowObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetTitle();
        })
        .method("void SetSize(uint, uint)", SafeCall<static_cast<void (OverlayWindowObject::*)(std::uint32_t, std::uint32_t)>(&OverlayWindowObject::SetSize)>())
        .method("void SetSyncWithTransform(bool)", SafeCall<static_cast<void (OverlayWindowObject::*)(bool) noexcept>(&OverlayWindowObject::SetSyncWithTransform)>())
        .method("bool IsSyncWithTransformEnabled() const", SafeCall<static_cast<bool (OverlayWindowObject::*)() const noexcept>(&OverlayWindowObject::IsSyncWithTransformEnabled)>())
        .method("bool SetMessageIntercepted(uint msg, bool enabled)", SafeCall<static_cast<bool (OverlayWindowObject::*)(std::uint32_t, bool)>(&OverlayWindowObject::SetMessageIntercepted)>())
        .method("bool IsMessageIntercepted(uint msg) const", SafeCall<static_cast<bool (OverlayWindowObject::*)(std::uint32_t) const>(&OverlayWindowObject::IsMessageIntercepted)>())
        .method("void CloseWindow()", SafeCall<static_cast<void (OverlayWindowObject::*)()>(&OverlayWindowObject::CloseWindow)>())
        .method("WindowObject@ opImplCast()", [](ScriptComponentHandle<OverlayWindowObject> &self) -> ScriptComponentHandle<IWindowObjectComponent> * {
            OverlayWindowObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<IWindowObjectComponent>::Create(static_cast<IWindowObjectComponent *>(obj));
        });
    engine->RegisterObjectMethod("WindowObject", "OverlayWindowObject@ opCast()",
        asFUNCTION((WindowObjectDownCast<OverlayWindowObject>)), asCALL_CDECL_OBJLAST);

    RegisterComponentType<ScreenBufferObject>(engine, "ScreenBufferObject")
        .method("void SetName(const string &in)", SafeCall<&ScreenBufferObject::SetName>())
        .method("const string &GetName() const", SafeCall<&ScreenBufferObject::GetName>())
        .method("void SetSize(uint, uint)", SafeCall<&ScreenBufferObject::SetSize>())
        .method("void SetSaveDirectory(const string &in)", SafeCall<&ScreenBufferObject::SetSaveDirectory>())
        .method("const string &GetSaveDirectory() const", SafeCall<&ScreenBufferObject::GetSaveDirectory>())
        .method("void SetSaveFileNamePrefix(const string &in)", SafeCall<&ScreenBufferObject::SetSaveFileNamePrefix>())
        .method("const string &GetSaveFileNamePrefix() const", SafeCall<&ScreenBufferObject::GetSaveFileNamePrefix>())
        .method("void SetSaveFormat(const string &in)", SafeCall<&ScreenBufferObject::SetSaveFormat>())
        .method("const string &GetSaveFormat() const", SafeCall<&ScreenBufferObject::GetSaveFormat>())
        .method("bool RequestSave(const string &in filePath = \"\")", SafeCall<&ScreenBufferObject::RequestSave>());

    RegisterComponentType<ScreenBufferViewport>(engine, "ScreenBufferViewport")
        .method("void SetSourceObject(Object@)", [](ScriptComponentHandle<ScreenBufferViewport> &cHandle, ScriptObjectHandle *obj) {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            ScreenBufferViewport &c = *cPtr; c.SetSourceObject(ResolveObjectArg(obj)); })
        .method("Object@ GetSourceObject() const", [](const ScriptComponentHandle<ScreenBufferViewport> &cHandle) -> ScriptObjectHandle * {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const ScreenBufferViewport &c = *cPtr; return ScriptObjectHandle::Create(c.GetSourceObject()); })
        .method("string GetSourceObjectID() const", [](const ScriptComponentHandle<ScreenBufferViewport> &cHandle) -> std::string {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const ScreenBufferViewport &c = *cPtr; return c.GetSourceObjectID().ToString(); })
        .method("void SetDisplayCameraObject(Object@)", [](ScriptComponentHandle<ScreenBufferViewport> &cHandle, ScriptObjectHandle *obj) {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            ScreenBufferViewport &c = *cPtr; c.SetDisplayCameraObject(ResolveObjectArg(obj)); })
        .method("Object@ GetDisplayCameraObject() const", [](const ScriptComponentHandle<ScreenBufferViewport> &cHandle) -> ScriptObjectHandle * {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const ScreenBufferViewport &c = *cPtr; return ScriptObjectHandle::Create(c.GetDisplayCameraObject()); })
        .method("string GetDisplayCameraObjectID() const", [](const ScriptComponentHandle<ScreenBufferViewport> &cHandle) -> std::string {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const ScreenBufferViewport &c = *cPtr; return c.GetDisplayCameraObjectID().ToString(); })
        .method("SpriteRenderer@ GetSpriteRenderer() const", [](const ScriptComponentHandle<ScreenBufferViewport> &cHandle) -> ScriptComponentHandle<SpriteRenderer> * {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<SpriteRenderer>::Create(cPtr->GetSpriteRenderer());
        })
        .method("MeshFilter@ GetMeshFilter() const", [](const ScriptComponentHandle<ScreenBufferViewport> &cHandle) -> ScriptComponentHandle<MeshFilter> * {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<MeshFilter>::Create(cPtr->GetMeshFilter());
        })
        // fitModeは 0=None, 1=Stretch, 2=Letterbox
        .method("void SetFitMode(int)", [](ScriptComponentHandle<ScreenBufferViewport> &cHandle, int mode) {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            ScreenBufferViewport &c = *cPtr;
            c.SetFitMode(static_cast<ScreenBufferViewport::FitMode>(mode));
        })
        .method("int GetFitMode() const", [](const ScriptComponentHandle<ScreenBufferViewport> &cHandle) {
            ScreenBufferViewport *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const ScreenBufferViewport &c = *cPtr;
            return static_cast<int>(c.GetFitMode());
        })
        .method("bool TryGetOffscreenMousePosition(Vector2 &out)", SafeCall<&ScreenBufferViewport::TryGetOffscreenMousePosition>())
        .method("bool IsMouseOverOffscreen() const", SafeCall<&ScreenBufferViewport::IsMouseOverOffscreen>());

    RegisterComponentType<ScreenAnchor>(engine, "ScreenAnchor")
        .method("void SetCameraObject(Object@)", [](ScriptComponentHandle<ScreenAnchor> &cHandle, ScriptObjectHandle *obj) {
            ScreenAnchor *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            ScreenAnchor &c = *cPtr; c.SetCameraObject(ResolveObjectArg(obj)); })
        .method("Object@ GetCameraObject() const", [](const ScriptComponentHandle<ScreenAnchor> &cHandle) -> ScriptObjectHandle * {
            ScreenAnchor *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const ScreenAnchor &c = *cPtr; return ScriptObjectHandle::Create(c.GetCameraObject()); })
        .method("string GetCameraObjectID() const", [](const ScriptComponentHandle<ScreenAnchor> &cHandle) -> std::string {
            ScreenAnchor *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const ScreenAnchor &c = *cPtr; return c.GetCameraObjectID().ToString(); })
        .method("void SetAnchorPoint(const Vector2 &in)", SafeCall<&ScreenAnchor::SetAnchorPoint>())
        .method("const Vector2 &GetAnchorPoint() const", SafeCall<&ScreenAnchor::GetAnchorPoint>())
        .method("void SetOffset(const Vector2 &in)", SafeCall<&ScreenAnchor::SetOffset>())
        .method("const Vector2 &GetOffset() const", SafeCall<&ScreenAnchor::GetOffset>());

    RegisterComponentType<UIButton>(engine, "UIButton")
        .method("void SetDisplayCameraObject(Object@)", [](ScriptComponentHandle<UIButton> &cHandle, ScriptObjectHandle *obj) {
            UIButton *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            UIButton &c = *cPtr; c.SetDisplayCameraObject(ResolveObjectArg(obj)); })
        .method("bool IsHovered() const", SafeCall<&UIButton::IsHovered>())
        .method("bool IsPressed() const", SafeCall<&UIButton::IsPressed>())
        .method("bool IsClicked() const", SafeCall<&UIButton::IsClicked>())
        .method("bool TryGetLocalHoverPosition(Vector2 &out)", SafeCall<&UIButton::TryGetLocalHoverPosition>());

    RegisterComponentType<MeshButton>(engine, "MeshButton")
        .method("void SetDisplayCameraObject(Object@)", [](ScriptComponentHandle<MeshButton> &cHandle, ScriptObjectHandle *obj) {
            MeshButton *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshButton &c = *cPtr; c.SetDisplayCameraObject(ResolveObjectArg(obj)); })
        .method("Object@ GetDisplayCameraObject() const", [](const ScriptComponentHandle<MeshButton> &cHandle) -> ScriptObjectHandle * {
            MeshButton *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<ScriptObjectHandle *>(); }
            const MeshButton &c = *cPtr; return ScriptObjectHandle::Create(c.GetDisplayCameraObject()); })
        .method("string GetDisplayCameraObjectID() const", [](const ScriptComponentHandle<MeshButton> &cHandle) -> std::string {
            MeshButton *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const MeshButton &c = *cPtr; return c.GetDisplayCameraObjectID().ToString(); })
        .method("void SetPreciseMeshTest(bool)", SafeCall<&MeshButton::SetPreciseMeshTest>())
        .method("bool GetPreciseMeshTest() const", SafeCall<&MeshButton::GetPreciseMeshTest>())
        .method("bool IsHovered() const", SafeCall<&MeshButton::IsHovered>())
        .method("bool IsPressed() const", SafeCall<&MeshButton::IsPressed>())
        .method("bool IsClicked() const", SafeCall<&MeshButton::IsClicked>())
        .method("void SetDragAxis(const Vector3 &in)", SafeCall<&MeshButton::SetDragAxis>())
        .method("const Vector3 &GetDragAxis() const", SafeCall<&MeshButton::GetDragAxis>())
        .method("bool TryGetAxisDragOffset(float &out)", SafeCall<&MeshButton::TryGetAxisDragOffset>());

    RegisterComponentType<ShadowMapObject>(engine, "ShadowMapObject")
        .method("void SetName(const string &in)", SafeCall<&ShadowMapObject::SetName>())
        .method("const string &GetName() const", SafeCall<&ShadowMapObject::GetName>())
        .method("void SetSize(uint, uint)", SafeCall<&ShadowMapObject::SetSize>());

    // コライダー（形状固有パラメータ）
    RegisterColliderType<BoxCollider>(engine, "BoxCollider")
        .method("void SetSize(const Vector3 &in)", SafeCall<&BoxCollider::SetSize>())
        .method("const Vector3 &GetSize() const", SafeCall<&BoxCollider::GetSize>())
        .method("void SetCenter(const Vector3 &in)", SafeCall<&BoxCollider::SetCenter>())
        .method("const Vector3 &GetCenter() const", SafeCall<&BoxCollider::GetCenter>());

    RegisterColliderType<SphereCollider>(engine, "SphereCollider")
        .method("void SetRadius(float)", SafeCall<&SphereCollider::SetRadius>())
        .method("float GetRadius() const", SafeCall<&SphereCollider::GetRadius>())
        .method("void SetCenter(const Vector3 &in)", SafeCall<&SphereCollider::SetCenter>())
        .method("const Vector3 &GetCenter() const", SafeCall<&SphereCollider::GetCenter>());

    RegisterColliderType<CapsuleCollider>(engine, "CapsuleCollider")
        .method("void SetRadius(float)", SafeCall<&CapsuleCollider::SetRadius>())
        .method("float GetRadius() const", SafeCall<&CapsuleCollider::GetRadius>())
        .method("void SetHeight(float)", SafeCall<&CapsuleCollider::SetHeight>())
        .method("float GetHeight() const", SafeCall<&CapsuleCollider::GetHeight>())
        .method("void SetCenter(const Vector3 &in)", SafeCall<&CapsuleCollider::SetCenter>())
        .method("const Vector3 &GetCenter() const", SafeCall<&CapsuleCollider::GetCenter>());

    RegisterColliderType<MeshCollider>(engine, "MeshCollider")
        .method("void SetConvex(bool)", SafeCall<&MeshCollider::SetConvex>())
        .method("bool IsConvex() const", SafeCall<&MeshCollider::IsConvex>())
        .method("void SetMeshHandle(uint)", [](ScriptComponentHandle<MeshCollider> &cHandle, uint32_t handle) {
            MeshCollider *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return; }
            MeshCollider &c = *cPtr; c.SetMeshHandle(handle); })
        .method("uint GetMeshHandle() const", [](const ScriptComponentHandle<MeshCollider> &cHandle) -> uint32_t {
            MeshCollider *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const MeshCollider &c = *cPtr; return c.GetMeshHandle(); })
        .method("uint GetEffectiveMeshHandle() const", [](const ScriptComponentHandle<MeshCollider> &cHandle) -> uint32_t {
            MeshCollider *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const MeshCollider &c = *cPtr; return c.GetEffectiveMeshHandle(); });

    RegisterColliderType<RayCollider>(engine, "RayCollider")
        .method("void SetDirection(const Vector3 &in)", SafeCall<&RayCollider::SetDirection>())
        .method("const Vector3 &GetDirection() const", SafeCall<&RayCollider::GetDirection>())
        .method("void SetMaxDistance(float)", SafeCall<&RayCollider::SetMaxDistance>())
        .method("float GetMaxDistance() const", SafeCall<&RayCollider::GetMaxDistance>())
        .method("bool CastRay(HitInfo &out)", [](const ScriptComponentHandle<RayCollider> &selfHandle, ScriptHitInfo &outHit) -> bool {
            RayCollider *self = selfHandle.Resolve();
            if (!self) { ThrowDestroyedObjectException(); return false; }
            HitInfo3D hit{};
            const bool result = self->CastRay(hit);
            outHit.normal = hit.normal;
            outHit.penetration = hit.penetration;
            outHit.selfObject = ScriptObjectHandle::Create(hit.selfObject);
            outHit.otherObject = ScriptObjectHandle::Create(hit.otherObject);
            outHit.selfCollider = ScriptComponentHandle<ICollider>::Create(hit.selfCollider);
            outHit.otherCollider = ScriptComponentHandle<ICollider>::Create(hit.otherCollider);
            return result;
        });

    RegisterColliderType<Box2DCollider>(engine, "Box2DCollider")
        .method("void SetSize(const Vector2 &in)", SafeCall<&Box2DCollider::SetSize>())
        .method("const Vector2 &GetSize() const", SafeCall<&Box2DCollider::GetSize>())
        .method("void SetCenter(const Vector2 &in)", SafeCall<&Box2DCollider::SetCenter>())
        .method("const Vector2 &GetCenter() const", SafeCall<&Box2DCollider::GetCenter>());

    RegisterColliderType<Circle2DCollider>(engine, "Circle2DCollider")
        .method("void SetRadius(float)", SafeCall<&Circle2DCollider::SetRadius>())
        .method("float GetRadius() const", SafeCall<&Circle2DCollider::GetRadius>())
        .method("void SetCenter(const Vector2 &in)", SafeCall<&Circle2DCollider::SetCenter>())
        .method("const Vector2 &GetCenter() const", SafeCall<&Circle2DCollider::GetCenter>());

    RegisterColliderType<Capsule2DCollider>(engine, "Capsule2DCollider")
        .method("void SetStart(const Vector2 &in)", SafeCall<&Capsule2DCollider::SetStart>())
        .method("const Vector2 &GetStart() const", SafeCall<&Capsule2DCollider::GetStart>())
        .method("void SetEnd(const Vector2 &in)", SafeCall<&Capsule2DCollider::SetEnd>())
        .method("const Vector2 &GetEnd() const", SafeCall<&Capsule2DCollider::GetEnd>())
        .method("void SetRadius(float)", SafeCall<&Capsule2DCollider::SetRadius>())
        .method("float GetRadius() const", SafeCall<&Capsule2DCollider::GetRadius>());

    RegisterColliderType<Ray2DCollider>(engine, "Ray2DCollider")
        .method("void SetDirection(const Vector2 &in)", SafeCall<&Ray2DCollider::SetDirection>())
        .method("const Vector2 &GetDirection() const", SafeCall<&Ray2DCollider::GetDirection>())
        .method("void SetLength(float)", SafeCall<&Ray2DCollider::SetLength>())
        .method("float GetLength() const", SafeCall<&Ray2DCollider::GetLength>());

    //==================================================
    // ポストプロセスエフェクト
    //==================================================
    // それぞれ内部の Params 構造体を直接は公開せず、フィールドごとの Get/Set をラムダで提供する

    RegisterComponentType<SSAOEffect>(engine, "SSAOEffect")
        .method("float GetRadius() const", [](const ScriptComponentHandle<SSAOEffect> &eHandle) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const SSAOEffect &e = *ePtr; return e.GetParams().radius; })
        .method("void SetRadius(float)", [](ScriptComponentHandle<SSAOEffect> &eHandle, float v) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            SSAOEffect &e = *ePtr; auto p = e.GetParams(); p.radius = v; e.SetParams(p); })
        .method("float GetIntensity() const", [](const ScriptComponentHandle<SSAOEffect> &eHandle) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const SSAOEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<SSAOEffect> &eHandle, float v) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            SSAOEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("float GetPower() const", [](const ScriptComponentHandle<SSAOEffect> &eHandle) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const SSAOEffect &e = *ePtr; return e.GetParams().power; })
        .method("void SetPower(float)", [](ScriptComponentHandle<SSAOEffect> &eHandle, float v) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            SSAOEffect &e = *ePtr; auto p = e.GetParams(); p.power = v; e.SetParams(p); })
        .method("float GetBias() const", [](const ScriptComponentHandle<SSAOEffect> &eHandle) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const SSAOEffect &e = *ePtr; return e.GetParams().bias; })
        .method("void SetBias(float)", [](ScriptComponentHandle<SSAOEffect> &eHandle, float v) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            SSAOEffect &e = *ePtr; auto p = e.GetParams(); p.bias = v; e.SetParams(p); })
        .method("uint GetSampleCount() const", [](const ScriptComponentHandle<SSAOEffect> &eHandle) -> uint32_t {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const SSAOEffect &e = *ePtr; return e.GetParams().sampleCount; })
        .method("void SetSampleCount(uint)", [](ScriptComponentHandle<SSAOEffect> &eHandle, uint32_t v) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            SSAOEffect &e = *ePtr; auto p = e.GetParams(); p.sampleCount = v; e.SetParams(p); })
        .method("int GetBlurRadius() const", [](const ScriptComponentHandle<SSAOEffect> &eHandle) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const SSAOEffect &e = *ePtr; return e.GetParams().blurRadius; })
        .method("void SetBlurRadius(int)", [](ScriptComponentHandle<SSAOEffect> &eHandle, int v) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            SSAOEffect &e = *ePtr; auto p = e.GetParams(); p.blurRadius = v; e.SetParams(p); })
        .method("float GetDepthThreshold() const", [](const ScriptComponentHandle<SSAOEffect> &eHandle) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const SSAOEffect &e = *ePtr; return e.GetParams().depthThreshold; })
        .method("void SetDepthThreshold(float)", [](ScriptComponentHandle<SSAOEffect> &eHandle, float v) {
            SSAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            SSAOEffect &e = *ePtr; auto p = e.GetParams(); p.depthThreshold = v; e.SetParams(p); });

    RegisterComponentType<GTAOEffect>(engine, "GTAOEffect")
        .method("float GetRadius() const", [](const ScriptComponentHandle<GTAOEffect> &eHandle) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const GTAOEffect &e = *ePtr; return e.GetParams().radius; })
        .method("void SetRadius(float)", [](ScriptComponentHandle<GTAOEffect> &eHandle, float v) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GTAOEffect &e = *ePtr; auto p = e.GetParams(); p.radius = v; e.SetParams(p); })
        .method("float GetIntensity() const", [](const ScriptComponentHandle<GTAOEffect> &eHandle) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const GTAOEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<GTAOEffect> &eHandle, float v) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GTAOEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("float GetPower() const", [](const ScriptComponentHandle<GTAOEffect> &eHandle) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const GTAOEffect &e = *ePtr; return e.GetParams().power; })
        .method("void SetPower(float)", [](ScriptComponentHandle<GTAOEffect> &eHandle, float v) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GTAOEffect &e = *ePtr; auto p = e.GetParams(); p.power = v; e.SetParams(p); })
        .method("float GetBias() const", [](const ScriptComponentHandle<GTAOEffect> &eHandle) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const GTAOEffect &e = *ePtr; return e.GetParams().bias; })
        .method("void SetBias(float)", [](ScriptComponentHandle<GTAOEffect> &eHandle, float v) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GTAOEffect &e = *ePtr; auto p = e.GetParams(); p.bias = v; e.SetParams(p); })
        .method("uint GetDirectionCount() const", [](const ScriptComponentHandle<GTAOEffect> &eHandle) -> uint32_t {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const GTAOEffect &e = *ePtr; return e.GetParams().directionCount; })
        .method("void SetDirectionCount(uint)", [](ScriptComponentHandle<GTAOEffect> &eHandle, uint32_t v) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GTAOEffect &e = *ePtr; auto p = e.GetParams(); p.directionCount = v; e.SetParams(p); })
        .method("uint GetStepCount() const", [](const ScriptComponentHandle<GTAOEffect> &eHandle) -> uint32_t {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const GTAOEffect &e = *ePtr; return e.GetParams().stepCount; })
        .method("void SetStepCount(uint)", [](ScriptComponentHandle<GTAOEffect> &eHandle, uint32_t v) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GTAOEffect &e = *ePtr; auto p = e.GetParams(); p.stepCount = v; e.SetParams(p); })
        .method("int GetBlurRadius() const", [](const ScriptComponentHandle<GTAOEffect> &eHandle) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const GTAOEffect &e = *ePtr; return e.GetParams().blurRadius; })
        .method("void SetBlurRadius(int)", [](ScriptComponentHandle<GTAOEffect> &eHandle, int v) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GTAOEffect &e = *ePtr; auto p = e.GetParams(); p.blurRadius = v; e.SetParams(p); })
        .method("float GetDepthThreshold() const", [](const ScriptComponentHandle<GTAOEffect> &eHandle) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const GTAOEffect &e = *ePtr; return e.GetParams().depthThreshold; })
        .method("void SetDepthThreshold(float)", [](ScriptComponentHandle<GTAOEffect> &eHandle, float v) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GTAOEffect &e = *ePtr; auto p = e.GetParams(); p.depthThreshold = v; e.SetParams(p); })
        .method("bool GetShowAOOnly() const", [](const ScriptComponentHandle<GTAOEffect> &eHandle) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const GTAOEffect &e = *ePtr; return e.GetParams().showAOOnly; })
        .method("void SetShowAOOnly(bool)", [](ScriptComponentHandle<GTAOEffect> &eHandle, bool v) {
            GTAOEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GTAOEffect &e = *ePtr; auto p = e.GetParams(); p.showAOOnly = v; e.SetParams(p); });

    RegisterComponentType<BloomEffect>(engine, "BloomEffect")
        .method("float GetThreshold() const", [](const ScriptComponentHandle<BloomEffect> &eHandle) {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const BloomEffect &e = *ePtr; return e.GetParams().threshold; })
        .method("void SetThreshold(float)", [](ScriptComponentHandle<BloomEffect> &eHandle, float v) {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            BloomEffect &e = *ePtr; auto p = e.GetParams(); p.threshold = v; e.SetParams(p); })
        .method("float GetSoftKnee() const", [](const ScriptComponentHandle<BloomEffect> &eHandle) {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const BloomEffect &e = *ePtr; return e.GetParams().softKnee; })
        .method("void SetSoftKnee(float)", [](ScriptComponentHandle<BloomEffect> &eHandle, float v) {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            BloomEffect &e = *ePtr; auto p = e.GetParams(); p.softKnee = v; e.SetParams(p); })
        .method("float GetIntensity() const", [](const ScriptComponentHandle<BloomEffect> &eHandle) {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const BloomEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<BloomEffect> &eHandle, float v) {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            BloomEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("float GetBlurRadius() const", [](const ScriptComponentHandle<BloomEffect> &eHandle) {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const BloomEffect &e = *ePtr; return e.GetParams().blurRadius; })
        .method("void SetBlurRadius(float)", [](ScriptComponentHandle<BloomEffect> &eHandle, float v) {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            BloomEffect &e = *ePtr; auto p = e.GetParams(); p.blurRadius = v; e.SetParams(p); })
        .method("uint GetIterations() const", [](const ScriptComponentHandle<BloomEffect> &eHandle) -> uint32_t {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const BloomEffect &e = *ePtr; return e.GetParams().iterations; })
        .method("void SetIterations(uint)", [](ScriptComponentHandle<BloomEffect> &eHandle, uint32_t v) {
            BloomEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            BloomEffect &e = *ePtr; auto p = e.GetParams(); p.iterations = v; e.SetParams(p); });

    RegisterComponentType<BoxFilterEffect>(engine, "BoxFilterEffect")
        .method("float GetIntensity() const", [](const ScriptComponentHandle<BoxFilterEffect> &eHandle) {
            BoxFilterEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const BoxFilterEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<BoxFilterEffect> &eHandle, float v) {
            BoxFilterEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            BoxFilterEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("void SetHalfSize(int, int)", [](ScriptComponentHandle<BoxFilterEffect> &eHandle, int x, int y) {
            BoxFilterEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            BoxFilterEffect &e = *ePtr;
            auto p = e.GetParams(); p.halfSize[0] = x; p.halfSize[1] = y; e.SetParams(p);
        })
        .method("int GetHalfSizeX() const", [](const ScriptComponentHandle<BoxFilterEffect> &eHandle) {
            BoxFilterEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const BoxFilterEffect &e = *ePtr; return e.GetParams().halfSize[0]; })
        .method("int GetHalfSizeY() const", [](const ScriptComponentHandle<BoxFilterEffect> &eHandle) {
            BoxFilterEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const BoxFilterEffect &e = *ePtr; return e.GetParams().halfSize[1]; });

    RegisterComponentType<ChromaticAberrationEffect>(engine, "ChromaticAberrationEffect")
        .method("void SetDirection(const Vector2 &in)", [](ScriptComponentHandle<ChromaticAberrationEffect> &eHandle, const Vector2 &dir) {
            ChromaticAberrationEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            ChromaticAberrationEffect &e = *ePtr;
            auto p = e.GetParams(); p.directionX = dir.x; p.directionY = dir.y; e.SetParams(p);
        })
        .method("Vector2 GetDirection() const", [](const ScriptComponentHandle<ChromaticAberrationEffect> &eHandle) -> Vector2 {
            ChromaticAberrationEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector2>(); }
            const ChromaticAberrationEffect &e = *ePtr;
            const auto &p = e.GetParams(); return Vector2(p.directionX, p.directionY);
        })
        .method("float GetStrength() const", [](const ScriptComponentHandle<ChromaticAberrationEffect> &eHandle) {
            ChromaticAberrationEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const ChromaticAberrationEffect &e = *ePtr; return e.GetParams().strength; })
        .method("void SetStrength(float)", [](ScriptComponentHandle<ChromaticAberrationEffect> &eHandle, float v) {
            ChromaticAberrationEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            ChromaticAberrationEffect &e = *ePtr; auto p = e.GetParams(); p.strength = v; e.SetParams(p); });

    RegisterComponentType<ColorAdjustEffect>(engine, "ColorAdjustEffect")
        .method("float GetBrightness() const", [](const ScriptComponentHandle<ColorAdjustEffect> &eHandle) {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const ColorAdjustEffect &e = *ePtr; return e.GetParams().brightness; })
        .method("void SetBrightness(float)", [](ScriptComponentHandle<ColorAdjustEffect> &eHandle, float v) {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            ColorAdjustEffect &e = *ePtr; auto p = e.GetParams(); p.brightness = v; e.SetParams(p); })
        .method("float GetContrast() const", [](const ScriptComponentHandle<ColorAdjustEffect> &eHandle) {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const ColorAdjustEffect &e = *ePtr; return e.GetParams().contrast; })
        .method("void SetContrast(float)", [](ScriptComponentHandle<ColorAdjustEffect> &eHandle, float v) {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            ColorAdjustEffect &e = *ePtr; auto p = e.GetParams(); p.contrast = v; e.SetParams(p); })
        .method("float GetSaturation() const", [](const ScriptComponentHandle<ColorAdjustEffect> &eHandle) {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const ColorAdjustEffect &e = *ePtr; return e.GetParams().saturation; })
        .method("void SetSaturation(float)", [](ScriptComponentHandle<ColorAdjustEffect> &eHandle, float v) {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            ColorAdjustEffect &e = *ePtr; auto p = e.GetParams(); p.saturation = v; e.SetParams(p); })
        .method("float GetTemperature() const", [](const ScriptComponentHandle<ColorAdjustEffect> &eHandle) {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const ColorAdjustEffect &e = *ePtr; return e.GetParams().temperature; })
        .method("void SetTemperature(float)", [](ScriptComponentHandle<ColorAdjustEffect> &eHandle, float v) {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            ColorAdjustEffect &e = *ePtr; auto p = e.GetParams(); p.temperature = v; e.SetParams(p); })
        .method("Vector3 GetColorBalance() const", [](const ScriptComponentHandle<ColorAdjustEffect> &eHandle) -> Vector3 {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector3>(); }
            const ColorAdjustEffect &e = *ePtr;
            const auto &p = e.GetParams(); return Vector3(p.colorBalance[0], p.colorBalance[1], p.colorBalance[2]);
        })
        .method("void SetColorBalance(const Vector3 &in)", [](ScriptComponentHandle<ColorAdjustEffect> &eHandle, const Vector3 &v) {
            ColorAdjustEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            ColorAdjustEffect &e = *ePtr;
            auto p = e.GetParams(); p.colorBalance[0] = v.x; p.colorBalance[1] = v.y; p.colorBalance[2] = v.z; e.SetParams(p);
        });

    RegisterComponentType<DepthOfFieldEffect>(engine, "DepthOfFieldEffect")
        .method("float GetFocusDistance() const", [](const ScriptComponentHandle<DepthOfFieldEffect> &eHandle) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DepthOfFieldEffect &e = *ePtr; return e.GetParams().focusDistance; })
        .method("void SetFocusDistance(float)", [](ScriptComponentHandle<DepthOfFieldEffect> &eHandle, float v) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DepthOfFieldEffect &e = *ePtr; auto p = e.GetParams(); p.focusDistance = v; e.SetParams(p); })
        .method("float GetFocusRange() const", [](const ScriptComponentHandle<DepthOfFieldEffect> &eHandle) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DepthOfFieldEffect &e = *ePtr; return e.GetParams().focusRange; })
        .method("void SetFocusRange(float)", [](ScriptComponentHandle<DepthOfFieldEffect> &eHandle, float v) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DepthOfFieldEffect &e = *ePtr; auto p = e.GetParams(); p.focusRange = v; e.SetParams(p); })
        .method("float GetNearBlurDistance() const", [](const ScriptComponentHandle<DepthOfFieldEffect> &eHandle) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DepthOfFieldEffect &e = *ePtr; return e.GetParams().nearBlurDistance; })
        .method("void SetNearBlurDistance(float)", [](ScriptComponentHandle<DepthOfFieldEffect> &eHandle, float v) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DepthOfFieldEffect &e = *ePtr; auto p = e.GetParams(); p.nearBlurDistance = v; e.SetParams(p); })
        .method("float GetFarBlurDistance() const", [](const ScriptComponentHandle<DepthOfFieldEffect> &eHandle) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DepthOfFieldEffect &e = *ePtr; return e.GetParams().farBlurDistance; })
        .method("void SetFarBlurDistance(float)", [](ScriptComponentHandle<DepthOfFieldEffect> &eHandle, float v) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DepthOfFieldEffect &e = *ePtr; auto p = e.GetParams(); p.farBlurDistance = v; e.SetParams(p); })
        .method("float GetMaxBlurRadiusPixels() const", [](const ScriptComponentHandle<DepthOfFieldEffect> &eHandle) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DepthOfFieldEffect &e = *ePtr; return e.GetParams().maxBlurRadiusPixels; })
        .method("void SetMaxBlurRadiusPixels(float)", [](ScriptComponentHandle<DepthOfFieldEffect> &eHandle, float v) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DepthOfFieldEffect &e = *ePtr; auto p = e.GetParams(); p.maxBlurRadiusPixels = v; e.SetParams(p); })
        .method("uint GetSampleCount() const", [](const ScriptComponentHandle<DepthOfFieldEffect> &eHandle) -> uint32_t {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const DepthOfFieldEffect &e = *ePtr; return e.GetParams().sampleCount; })
        .method("void SetSampleCount(uint)", [](ScriptComponentHandle<DepthOfFieldEffect> &eHandle, uint32_t v) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DepthOfFieldEffect &e = *ePtr; auto p = e.GetParams(); p.sampleCount = v; e.SetParams(p); })
        .method("int GetDilateRadius() const", [](const ScriptComponentHandle<DepthOfFieldEffect> &eHandle) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const DepthOfFieldEffect &e = *ePtr; return e.GetParams().dilateRadius; })
        .method("void SetDilateRadius(int)", [](ScriptComponentHandle<DepthOfFieldEffect> &eHandle, int v) {
            DepthOfFieldEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DepthOfFieldEffect &e = *ePtr; auto p = e.GetParams(); p.dilateRadius = v; e.SetParams(p); });

    RegisterComponentType<DissolveEffect>(engine, "DissolveEffect")
        .method("float GetMaskThreshold() const", [](const ScriptComponentHandle<DissolveEffect> &eHandle) {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DissolveEffect &e = *ePtr; return e.GetParams().maskThreshold; })
        .method("void SetMaskThreshold(float)", [](ScriptComponentHandle<DissolveEffect> &eHandle, float v) {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DissolveEffect &e = *ePtr; auto p = e.GetParams(); p.maskThreshold = v; e.SetParams(p); })
        .method("float GetEdgeThickness() const", [](const ScriptComponentHandle<DissolveEffect> &eHandle) {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DissolveEffect &e = *ePtr; return e.GetParams().edgeThickness; })
        .method("void SetEdgeThickness(float)", [](ScriptComponentHandle<DissolveEffect> &eHandle, float v) {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DissolveEffect &e = *ePtr; auto p = e.GetParams(); p.edgeThickness = v; e.SetParams(p); })
        .method("string GetBaseTexturePath() const", [](const ScriptComponentHandle<DissolveEffect> &eHandle) -> std::string {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const DissolveEffect &e = *ePtr;
            return TextureManager::GetTextureAssetPath(e.GetParams().baseTexture);
        })
        .method("void SetBaseTexturePath(const string &in)", [](ScriptComponentHandle<DissolveEffect> &eHandle, const std::string &path) {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DissolveEffect &e = *ePtr;
            auto p = e.GetParams();
            p.baseTexture = path.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(path);
            e.SetParams(p);
        })
        .method("string GetMaskTexturePath() const", [](const ScriptComponentHandle<DissolveEffect> &eHandle) -> std::string {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const DissolveEffect &e = *ePtr;
            return TextureManager::GetTextureAssetPath(e.GetParams().maskTexture);
        })
        .method("void SetMaskTexturePath(const string &in)", [](ScriptComponentHandle<DissolveEffect> &eHandle, const std::string &path) {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DissolveEffect &e = *ePtr;
            auto p = e.GetParams();
            p.maskTexture = path.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(path);
            e.SetParams(p);
        })
        .method("Vector4 GetBaseTextureColor() const", [](const ScriptComponentHandle<DissolveEffect> &eHandle) -> Vector4 {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector4>(); }
            const DissolveEffect &e = *ePtr;
            const auto &c = e.GetParams().baseTextureColor; return Vector4(c[0], c[1], c[2], c[3]);
        })
        .method("void SetBaseTextureColor(const Vector4 &in)", [](ScriptComponentHandle<DissolveEffect> &eHandle, const Vector4 &c) {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DissolveEffect &e = *ePtr;
            auto p = e.GetParams();
            p.baseTextureColor[0] = c.x; p.baseTextureColor[1] = c.y; p.baseTextureColor[2] = c.z; p.baseTextureColor[3] = c.w;
            e.SetParams(p);
        })
        .method("Vector4 GetEdgeColor() const", [](const ScriptComponentHandle<DissolveEffect> &eHandle) -> Vector4 {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector4>(); }
            const DissolveEffect &e = *ePtr;
            const auto &c = e.GetParams().edgeColor; return Vector4(c[0], c[1], c[2], c[3]);
        })
        .method("void SetEdgeColor(const Vector4 &in)", [](ScriptComponentHandle<DissolveEffect> &eHandle, const Vector4 &c) {
            DissolveEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DissolveEffect &e = *ePtr;
            auto p = e.GetParams();
            p.edgeColor[0] = c.x; p.edgeColor[1] = c.y; p.edgeColor[2] = c.z; p.edgeColor[3] = c.w;
            e.SetParams(p);
        });

    RegisterComponentType<DitherEffect>(engine, "DitherEffect")
        .method("float GetIntensity() const", [](const ScriptComponentHandle<DitherEffect> &eHandle) {
            DitherEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DitherEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<DitherEffect> &eHandle, float v) {
            DitherEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DitherEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("bool IsColorDither() const", [](const ScriptComponentHandle<DitherEffect> &eHandle) {
            DitherEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const DitherEffect &e = *ePtr; return e.GetParams().color; })
        .method("void SetColorDither(bool)", [](ScriptComponentHandle<DitherEffect> &eHandle, bool v) {
            DitherEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DitherEffect &e = *ePtr; auto p = e.GetParams(); p.color = v; e.SetParams(p); });

    RegisterComponentType<DotMatrixEffect>(engine, "DotMatrixEffect")
        .method("float GetDotSpacing() const", [](const ScriptComponentHandle<DotMatrixEffect> &eHandle) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DotMatrixEffect &e = *ePtr; return e.GetParams().dotSpacing; })
        .method("void SetDotSpacing(float)", [](ScriptComponentHandle<DotMatrixEffect> &eHandle, float v) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DotMatrixEffect &e = *ePtr; auto p = e.GetParams(); p.dotSpacing = v; e.SetParams(p); })
        .method("float GetDotRadius() const", [](const ScriptComponentHandle<DotMatrixEffect> &eHandle) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DotMatrixEffect &e = *ePtr; return e.GetParams().dotRadius; })
        .method("void SetDotRadius(float)", [](ScriptComponentHandle<DotMatrixEffect> &eHandle, float v) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DotMatrixEffect &e = *ePtr; auto p = e.GetParams(); p.dotRadius = v; e.SetParams(p); })
        .method("float GetThreshold() const", [](const ScriptComponentHandle<DotMatrixEffect> &eHandle) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DotMatrixEffect &e = *ePtr; return e.GetParams().threshold; })
        .method("void SetThreshold(float)", [](ScriptComponentHandle<DotMatrixEffect> &eHandle, float v) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DotMatrixEffect &e = *ePtr; auto p = e.GetParams(); p.threshold = v; e.SetParams(p); })
        .method("float GetIntensity() const", [](const ScriptComponentHandle<DotMatrixEffect> &eHandle) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const DotMatrixEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<DotMatrixEffect> &eHandle, float v) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DotMatrixEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("bool IsMonochrome() const", [](const ScriptComponentHandle<DotMatrixEffect> &eHandle) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<bool>(); }
            const DotMatrixEffect &e = *ePtr; return e.GetParams().monochrome; })
        .method("void SetMonochrome(bool)", [](ScriptComponentHandle<DotMatrixEffect> &eHandle, bool v) {
            DotMatrixEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            DotMatrixEffect &e = *ePtr; auto p = e.GetParams(); p.monochrome = v; e.SetParams(p); });

    RegisterComponentType<FXAAEffect>(engine, "FXAAEffect")
        .method("float GetThreshold() const", [](const ScriptComponentHandle<FXAAEffect> &eHandle) {
            FXAAEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const FXAAEffect &e = *ePtr; return e.GetParams().threshold; })
        .method("void SetThreshold(float)", [](ScriptComponentHandle<FXAAEffect> &eHandle, float v) {
            FXAAEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            FXAAEffect &e = *ePtr; auto p = e.GetParams(); p.threshold = v; e.SetParams(p); })
        .method("float GetThresholdMin() const", [](const ScriptComponentHandle<FXAAEffect> &eHandle) {
            FXAAEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const FXAAEffect &e = *ePtr; return e.GetParams().thresholdMin; })
        .method("void SetThresholdMin(float)", [](ScriptComponentHandle<FXAAEffect> &eHandle, float v) {
            FXAAEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            FXAAEffect &e = *ePtr; auto p = e.GetParams(); p.thresholdMin = v; e.SetParams(p); })
        .method("float GetStrength() const", [](const ScriptComponentHandle<FXAAEffect> &eHandle) {
            FXAAEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const FXAAEffect &e = *ePtr; return e.GetParams().strength; })
        .method("void SetStrength(float)", [](ScriptComponentHandle<FXAAEffect> &eHandle, float v) {
            FXAAEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            FXAAEffect &e = *ePtr; auto p = e.GetParams(); p.strength = v; e.SetParams(p); })
        .method("float GetSubpixelBlend() const", [](const ScriptComponentHandle<FXAAEffect> &eHandle) {
            FXAAEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const FXAAEffect &e = *ePtr; return e.GetParams().subpixelBlend; })
        .method("void SetSubpixelBlend(float)", [](ScriptComponentHandle<FXAAEffect> &eHandle, float v) {
            FXAAEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            FXAAEffect &e = *ePtr; auto p = e.GetParams(); p.subpixelBlend = v; e.SetParams(p); });

    RegisterComponentType<GaussianFilterEffect>(engine, "GaussianFilterEffect")
        .method("int GetRadius() const", [](const ScriptComponentHandle<GaussianFilterEffect> &eHandle) {
            GaussianFilterEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const GaussianFilterEffect &e = *ePtr; return e.GetParams().radius; })
        .method("void SetRadius(int)", [](ScriptComponentHandle<GaussianFilterEffect> &eHandle, int v) {
            GaussianFilterEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GaussianFilterEffect &e = *ePtr; auto p = e.GetParams(); p.radius = v; e.SetParams(p); })
        .method("float GetSigma() const", [](const ScriptComponentHandle<GaussianFilterEffect> &eHandle) {
            GaussianFilterEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const GaussianFilterEffect &e = *ePtr; return e.GetParams().sigma; })
        .method("void SetSigma(float)", [](ScriptComponentHandle<GaussianFilterEffect> &eHandle, float v) {
            GaussianFilterEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GaussianFilterEffect &e = *ePtr; auto p = e.GetParams(); p.sigma = v; e.SetParams(p); });

    RegisterComponentType<GrayscaleEffect>(engine, "GrayscaleEffect")
        .method("float GetIntensity() const", [](const ScriptComponentHandle<GrayscaleEffect> &eHandle) {
            GrayscaleEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const GrayscaleEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<GrayscaleEffect> &eHandle, float v) {
            GrayscaleEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            GrayscaleEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); });

    RegisterComponentType<MotionBlurEffect>(engine, "MotionBlurEffect")
        .method("float GetIntensity() const", [](const ScriptComponentHandle<MotionBlurEffect> &eHandle) {
            MotionBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const MotionBlurEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<MotionBlurEffect> &eHandle, float v) {
            MotionBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            MotionBlurEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("float GetVelocityScale() const", [](const ScriptComponentHandle<MotionBlurEffect> &eHandle) {
            MotionBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const MotionBlurEffect &e = *ePtr; return e.GetParams().velocityScale; })
        .method("void SetVelocityScale(float)", [](ScriptComponentHandle<MotionBlurEffect> &eHandle, float v) {
            MotionBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            MotionBlurEffect &e = *ePtr; auto p = e.GetParams(); p.velocityScale = v; e.SetParams(p); })
        .method("float GetMaxBlurPixels() const", [](const ScriptComponentHandle<MotionBlurEffect> &eHandle) {
            MotionBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const MotionBlurEffect &e = *ePtr; return e.GetParams().maxBlurPixels; })
        .method("void SetMaxBlurPixels(float)", [](ScriptComponentHandle<MotionBlurEffect> &eHandle, float v) {
            MotionBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            MotionBlurEffect &e = *ePtr; auto p = e.GetParams(); p.maxBlurPixels = v; e.SetParams(p); })
        .method("uint GetSamples() const", [](const ScriptComponentHandle<MotionBlurEffect> &eHandle) -> uint32_t {
            MotionBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const MotionBlurEffect &e = *ePtr; return e.GetParams().samples; })
        .method("void SetSamples(uint)", [](ScriptComponentHandle<MotionBlurEffect> &eHandle, uint32_t v) {
            MotionBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            MotionBlurEffect &e = *ePtr; auto p = e.GetParams(); p.samples = v; e.SetParams(p); });

    RegisterComponentType<OutlineEffect>(engine, "OutlineEffect")
        .method("float GetThreshold() const", [](const ScriptComponentHandle<OutlineEffect> &eHandle) {
            OutlineEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const OutlineEffect &e = *ePtr; return e.GetParams().threshold; })
        .method("void SetThreshold(float)", [](ScriptComponentHandle<OutlineEffect> &eHandle, float v) {
            OutlineEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            OutlineEffect &e = *ePtr; auto p = e.GetParams(); p.threshold = v; e.SetParams(p); })
        .method("float GetThickness() const", [](const ScriptComponentHandle<OutlineEffect> &eHandle) {
            OutlineEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const OutlineEffect &e = *ePtr; return e.GetParams().thickness; })
        .method("void SetThickness(float)", [](ScriptComponentHandle<OutlineEffect> &eHandle, float v) {
            OutlineEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            OutlineEffect &e = *ePtr; auto p = e.GetParams(); p.thickness = v; e.SetParams(p); })
        .method("Vector4 GetColor() const", [](const ScriptComponentHandle<OutlineEffect> &eHandle) -> Vector4 {
            OutlineEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector4>(); }
            const OutlineEffect &e = *ePtr;
            const auto &c = e.GetParams().color; return Vector4(c[0], c[1], c[2], c[3]);
        })
        .method("void SetColor(const Vector4 &in)", [](ScriptComponentHandle<OutlineEffect> &eHandle, const Vector4 &c) {
            OutlineEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            OutlineEffect &e = *ePtr;
            auto p = e.GetParams();
            p.color[0] = c.x; p.color[1] = c.y; p.color[2] = c.z; p.color[3] = c.w;
            e.SetParams(p);
        });

    RegisterComponentType<RadialBlurEffect>(engine, "RadialBlurEffect")
        .method("float GetIntensity() const", [](const ScriptComponentHandle<RadialBlurEffect> &eHandle) {
            RadialBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const RadialBlurEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<RadialBlurEffect> &eHandle, float v) {
            RadialBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            RadialBlurEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("int GetSampleCount() const", [](const ScriptComponentHandle<RadialBlurEffect> &eHandle) {
            RadialBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<int>(); }
            const RadialBlurEffect &e = *ePtr; return e.GetParams().sampleCount; })
        .method("void SetSampleCount(int)", [](ScriptComponentHandle<RadialBlurEffect> &eHandle, int v) {
            RadialBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            RadialBlurEffect &e = *ePtr; auto p = e.GetParams(); p.sampleCount = v; e.SetParams(p); })
        .method("Vector2 GetCenter() const", [](const ScriptComponentHandle<RadialBlurEffect> &eHandle) -> Vector2 {
            RadialBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector2>(); }
            const RadialBlurEffect &e = *ePtr;
            const auto &c = e.GetParams().radialCenter; return Vector2(c[0], c[1]);
        })
        .method("void SetCenter(const Vector2 &in)", [](ScriptComponentHandle<RadialBlurEffect> &eHandle, const Vector2 &c) {
            RadialBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            RadialBlurEffect &e = *ePtr;
            auto p = e.GetParams(); p.radialCenter[0] = c.x; p.radialCenter[1] = c.y; e.SetParams(p);
        })
        .method("float GetStartRadius() const", [](const ScriptComponentHandle<RadialBlurEffect> &eHandle) {
            RadialBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const RadialBlurEffect &e = *ePtr; return e.GetParams().startRadius; })
        .method("void SetStartRadius(float)", [](ScriptComponentHandle<RadialBlurEffect> &eHandle, float v) {
            RadialBlurEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            RadialBlurEffect &e = *ePtr; auto p = e.GetParams(); p.startRadius = v; e.SetParams(p); });

    RegisterComponentType<TemporalBlendEffect>(engine, "TemporalBlendEffect")
        .method("float GetHistoryWeight() const", [](const ScriptComponentHandle<TemporalBlendEffect> &eHandle) {
            TemporalBlendEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const TemporalBlendEffect &e = *ePtr; return e.GetParams().historyWeight; })
        .method("void SetHistoryWeight(float)", [](ScriptComponentHandle<TemporalBlendEffect> &eHandle, float v) {
            TemporalBlendEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            TemporalBlendEffect &e = *ePtr; auto p = e.GetParams(); p.historyWeight = v; e.SetParams(p); });

    RegisterComponentType<VignetteEffect>(engine, "VignetteEffect")
        .method("Vector2 GetCenter() const", [](const ScriptComponentHandle<VignetteEffect> &eHandle) -> Vector2 {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector2>(); }
            const VignetteEffect &e = *ePtr;
            const auto &c = e.GetParams().center; return Vector2(c[0], c[1]);
        })
        .method("void SetCenter(const Vector2 &in)", [](ScriptComponentHandle<VignetteEffect> &eHandle, const Vector2 &c) {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            VignetteEffect &e = *ePtr;
            auto p = e.GetParams(); p.center[0] = c.x; p.center[1] = c.y; e.SetParams(p);
        })
        .method("Vector4 GetColor() const", [](const ScriptComponentHandle<VignetteEffect> &eHandle) -> Vector4 {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<Vector4>(); }
            const VignetteEffect &e = *ePtr; return e.GetParams().color; })
        .method("void SetColor(const Vector4 &in)", [](ScriptComponentHandle<VignetteEffect> &eHandle, const Vector4 &c) {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            VignetteEffect &e = *ePtr;
            auto p = e.GetParams(); p.color = c; e.SetParams(p);
        })
        .method("float GetIntensity() const", [](const ScriptComponentHandle<VignetteEffect> &eHandle) {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const VignetteEffect &e = *ePtr; return e.GetParams().intensity; })
        .method("void SetIntensity(float)", [](ScriptComponentHandle<VignetteEffect> &eHandle, float v) {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            VignetteEffect &e = *ePtr; auto p = e.GetParams(); p.intensity = v; e.SetParams(p); })
        .method("float GetInnerRadius() const", [](const ScriptComponentHandle<VignetteEffect> &eHandle) {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const VignetteEffect &e = *ePtr; return e.GetParams().innerRadius; })
        .method("void SetInnerRadius(float)", [](ScriptComponentHandle<VignetteEffect> &eHandle, float v) {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            VignetteEffect &e = *ePtr; auto p = e.GetParams(); p.innerRadius = v; e.SetParams(p); })
        .method("float GetSmoothness() const", [](const ScriptComponentHandle<VignetteEffect> &eHandle) {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<float>(); }
            const VignetteEffect &e = *ePtr; return e.GetParams().smoothness; })
        .method("void SetSmoothness(float)", [](ScriptComponentHandle<VignetteEffect> &eHandle, float v) {
            VignetteEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            VignetteEffect &e = *ePtr; auto p = e.GetParams(); p.smoothness = v; e.SetParams(p); });

    RegisterComponentType<ScreenWideDitherBlendEffect>(engine, "ScreenWideDitherBlendEffect")
        .method("uint GetPassCount() const", [](const ScriptComponentHandle<ScreenWideDitherBlendEffect> &eHandle) -> uint32_t {
            ScreenWideDitherBlendEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return SafeCallDefault<uint32_t>(); }
            const ScreenWideDitherBlendEffect &e = *ePtr; return e.GetPassCount(); })
        .method("void SetPassCount(uint)", [](ScriptComponentHandle<ScreenWideDitherBlendEffect> &eHandle, uint32_t count) {
            ScreenWideDitherBlendEffect *ePtr = eHandle.Resolve();
            if (!ePtr) { ThrowDestroyedObjectException(); return; }
            ScreenWideDitherBlendEffect &e = *ePtr; e.SetPassCount(count); });

    //==================================================
    // メディア再生
    //==================================================

    RegisterComponentType<GifSource>(engine, "GifSource")
        .method("bool Play()", SafeCall<&GifSource::Play>())
        .method("void Stop()", SafeCall<&GifSource::Stop>())
        .method("bool Pause()", SafeCall<&GifSource::Pause>())
        .method("bool Resume()", SafeCall<&GifSource::Resume>())
        .method("bool IsPlaying() const", SafeCall<&GifSource::IsPlaying>())
        .method("bool IsPaused() const", SafeCall<&GifSource::IsPaused>())
        .method("void SetGifAssetPath(const string &in)", SafeCall<&GifSource::SetGifAssetPath>())
        .method("const string &GetGifAssetPath() const", SafeCall<&GifSource::GetGifAssetPath>())
        .method("void SetLoop(bool)", SafeCall<&GifSource::SetLoop>())
        .method("bool GetLoop() const", SafeCall<&GifSource::GetLoop>())
        .method("void SetPlayOnAwake(bool)", SafeCall<&GifSource::SetPlayOnAwake>())
        .method("bool GetPlayOnAwake() const", SafeCall<&GifSource::GetPlayOnAwake>());

    RegisterComponentType<VideoSource>(engine, "VideoSource")
        .method("bool Play()", SafeCall<&VideoSource::Play>())
        .method("void Stop()", SafeCall<&VideoSource::Stop>())
        .method("bool Pause()", SafeCall<&VideoSource::Pause>())
        .method("bool Resume()", SafeCall<&VideoSource::Resume>())
        .method("bool IsPlaying() const", SafeCall<&VideoSource::IsPlaying>())
        .method("bool IsPaused() const", SafeCall<&VideoSource::IsPaused>())
        .method("void SetVideoAssetPath(const string &in)", SafeCall<&VideoSource::SetVideoAssetPath>())
        .method("const string &GetVideoAssetPath() const", SafeCall<&VideoSource::GetVideoAssetPath>())
        .method("void SetLoop(bool)", SafeCall<&VideoSource::SetLoop>())
        .method("bool GetLoop() const", SafeCall<&VideoSource::GetLoop>())
        .method("void SetVolume(float)", SafeCall<&VideoSource::SetVolume>())
        .method("float GetVolume() const", SafeCall<&VideoSource::GetVolume>())
        .method("void SetPlayOnAwake(bool)", SafeCall<&VideoSource::SetPlayOnAwake>())
        .method("bool GetPlayOnAwake() const", SafeCall<&VideoSource::GetPlayOnAwake>())
        .method("void SetRouteAudioToAudioSource(bool)", SafeCall<&VideoSource::SetRouteAudioToAudioSource>())
        .method("bool GetRouteAudioToAudioSource() const", SafeCall<&VideoSource::GetRouteAudioToAudioSource>());

    RegisterComponentType<TextureSource>(engine, "TextureSource")
        .method("void SetTextureAssetPath(const string &in)", SafeCall<&TextureSource::SetTextureAssetPath>())
        .method("const string &GetTextureAssetPath() const", SafeCall<&TextureSource::GetTextureAssetPath>());

    //==================================================
    // Prefab
    //==================================================

    auto prefabInstanceBinder = RegisterComponentType<PrefabInstanceComponent>(engine, "PrefabInstanceComponent");
    prefabInstanceBinder
        .method("string GetPrefabID() const", [](const ScriptComponentHandle<PrefabInstanceComponent> &cHandle) -> std::string {
            PrefabInstanceComponent *cPtr = cHandle.Resolve();
            if (!cPtr) { ThrowDestroyedObjectException(); return SafeCallDefault<std::string>(); }
            const PrefabInstanceComponent &c = *cPtr; return c.GetPrefabID().ToString(); });
#if defined(USE_IMGUI)
    // GetPrefabPath()はUSE_IMGUI限定の実装（PrefabInstanceComponent.cpp）のため、Release構成では登録しない
    prefabInstanceBinder.method("string GetPrefabPath() const", SafeCall<&PrefabInstanceComponent::GetPrefabPath>());
#endif
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
    // wrapAsHandleはrefcount=1で生成する。出力先ハンドルへそのまま所有権を移す（他にAddRef元は無い）
    void *handle = component ? it->second.wrapAsHandle(component) : nullptr;
    *static_cast<void **>(ref) = handle;
    return handle != nullptr;
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
        void *handle = it->second.wrapAsHandle(components[i]);
        array->SetValue(i, &handle);
        // SetValueが内部で独自にAddRefするため、生成時点(refcount=1)の所有権は解放して配列だけに残す
        if (handle) it->second.releaseHandle(handle);
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
    void *handle = added ? it->second.wrapAsHandle(added) : nullptr;
    *static_cast<void **>(ref) = handle;
    return handle != nullptr;
}

/// @brief ?&in で渡されたコンポーネントハンドルをオブジェクトから削除する
/// @param ref 削除したいコンポーネントハンドルへのポインタ
/// @param typeId 渡されたハンドルのスクリプト型ID
/// @return 削除できた場合は true
bool RemoveComponentFromHandle(EmptyObject &obj, void *ref, int typeId) {
    if (!ref || !(typeId & asTYPEID_OBJHANDLE)) return false;
    const int baseTypeId = typeId & ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST);
    auto it = gComponentTypeBindings.find(baseTypeId);
    if (it == gComponentTypeBindings.end()) return false;

    void *handlePtr = *static_cast<void **>(ref);
    if (!handlePtr) return false;
    IObjectComponent *component = it->second.resolveHandle(handlePtr);
    if (!component) { ThrowDestroyedObjectException(); return false; }
    return obj.RemoveComponent(component);
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
        // ScriptObjectHandle::Createはrefcount=1で生成される。SetValueは配列側で
        // 独自にAddRefするため、格納後にこちら側の分をReleaseして所有権を配列だけに残す
        ScriptObjectHandle *handle = ScriptObjectHandle::Create(objects[i]);
        void *handlePtr = handle;
        array->SetValue(i, &handlePtr);
        if (handle) handle->Release();
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

    // Objectはスクリプトから参照先の生死を安全に判定できるよう、生ポインタではなくUUID保持の
    // 参照カウント式ハンドル(ScriptObjectHandle)で登録する。各メソッドは呼び出しの都度Resolve()で
    // 生存確認し、削除済みならAngelScriptの例外として処理する（詳細はScriptObjectHandle.h参照）
    asbind20::ref_class<ScriptObjectHandle>(engine, "Object")
        .addref(&ScriptObjectHandle::AddRef)
        .release(&ScriptObjectHandle::Release)
        .method("const string &GetName() const", [](const ScriptObjectHandle &self) -> const std::string & {
            static const std::string kEmpty;
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetName();
        })
        .method("void SetName(const string &in)", [](ScriptObjectHandle &self, const std::string &name) {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return; }
            obj->SetName(name);
        })
        .method("bool IsActive() const", [](const ScriptObjectHandle &self) -> bool {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return false; }
            return obj->IsActive();
        })
        .method("void SetActive(bool)", [](ScriptObjectHandle &self, bool active) {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return; }
            obj->SetActive(active);
        })
        .method("void SetComponentsActiveExceptTransformAndScript(bool)", [](ScriptObjectHandle &self, bool active) {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return; }
            obj->SetComponentsActiveExceptTransformAndScript(active);
        })
        .method("void SetTag(const string &in)", [](ScriptObjectHandle &self, const std::string &tag) {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return; }
            obj->SetTag(tag);
        })
        .method("Tag GetTag() const", [](const ScriptObjectHandle &self) -> Tag {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return Tag(); }
            return obj->GetTag();
        })
        .method("const string &GetTagName() const", [](const ScriptObjectHandle &self) -> const std::string & {
            static const std::string kEmpty;
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return kEmpty; }
            return obj->GetTagName();
        })
        .method("Transform@ GetTransform()", [](ScriptObjectHandle &self) -> ScriptComponentHandle<Transform> * {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptComponentHandle<Transform>::Create(obj->GetComponent<Transform>());
        })
        .method("bool GetComponent(?&out)", [](ScriptObjectHandle &self, void *ref, int typeId) -> bool {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return false; }
            return GetComponentIntoHandle(*obj, ref, typeId);
        })
        .method("bool GetComponents(?&out)", [](ScriptObjectHandle &self, void *ref, int typeId) -> bool {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return false; }
            return GetComponentsIntoArray(*obj, ref, typeId);
        })
        .method("bool AddComponent(?&out)", [](ScriptObjectHandle &self, void *ref, int typeId) -> bool {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return false; }
            return AddComponentIntoHandle(*obj, ref, typeId);
        })
        .method("bool RemoveComponent(?&in)", [](ScriptObjectHandle &self, void *ref, int typeId) -> bool {
            EmptyObject *obj = self.Resolve();
            if (!obj) { ThrowDestroyedObjectException(); return false; }
            return RemoveComponentFromHandle(*obj, ref, typeId);
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
            [](const SceneContext &scene, const std::string &name) -> ScriptObjectHandle * { return ScriptObjectHandle::Create(scene.GetSceneObject(name)); })
        .method("array<Object@>@ GetObjects(const string &in) const", [](const SceneContext &scene, const std::string &name) -> CScriptArray * {
            return MakeObjectArray(scene.GetSceneObjects(name));
        })
        .method("void SetNextSceneName(const string &in)", &SceneContext::SetNextSceneName)
        .method("bool ChangeToNextScene()", &SceneContext::ChangeToNextScene)
        .method("bool HasNextSceneName() const", &SceneContext::HasNextSceneName)
        .method("void ClearNextSceneName()", &SceneContext::ClearNextSceneName)
        // オブジェクトの生成・複製・削除
        .method("Object@ CreateObject(const string &in name = \"\")", [](SceneContext &scene, const std::string &name) -> ScriptObjectHandle * {
            return ScriptObjectHandle::Create(scene.CreateEmptyObject(name));
        })
        .method("Object@ CloneObject(Object@ source, const string &in name = \"\")", [](SceneContext &scene, ScriptObjectHandle *source, const std::string &name) -> ScriptObjectHandle * {
            EmptyObject *sourceObj = source ? source->Resolve() : nullptr;
            if (source && !sourceObj) { ThrowDestroyedObjectException(); return nullptr; }
            return ScriptObjectHandle::Create(scene.CloneObject(sourceObj, name));
        })
        .method("bool DeleteObject(Object@ obj)", [](SceneContext &scene, ScriptObjectHandle *handle) -> bool {
            EmptyObject *obj = handle ? handle->Resolve() : nullptr;
            if (handle && !obj) { ThrowDestroyedObjectException(); return false; }
            return scene.DeleteObject(obj);
        })
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
            JSON data = LoadJSON(ProjectPaths::ToPhysical(path));
            if (data.is_discarded()) return nullptr;
            return new ScriptJsonValue(std::move(data));
        })
        .function("bool SaveJsonFile(const string &in path, const Json &in data, int indent = 4)", [](const std::string &path, const ScriptJsonValue &data, int indent) -> bool {
            return SaveJSON(data.data, ProjectPaths::ToPhysical(path), indent);
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
        // ローカライズ（Locales/ および Assets/Locales/ に置かれた翻訳データを参照する）。
        // アプリケーション側の表示言語（GetCurrentApplicationLanguage）を参照する。
        // エディター自身の表示言語（EditorPreferencesのコンボ）とは独立しているため、
        // スクリプトから切り替えてもエディターUI自体の言語には影響しない
        .function("const string &Translation(const string &in)", &ApplicationTranslation)
        .function("const string &GetCurrentLanguage()", &GetCurrentApplicationLanguage)
        // 呼んだ言語をPlayerSettingsへ保存する（配布したゲームでの言語選択を再起動後も復元するため）。
        // Translation.h側のSetCurrentApplicationLanguage自体は永続化の副作用を持たないため、
        // ここでラップする
        .function("void SetCurrentLanguage(const string &in)", [](const std::string &lang) {
            SetCurrentApplicationLanguage(lang);
            PlayerSettings::SetString(PlayerSettings::kLanguageKey, lang);
        })
        .function("array<string>@ GetLoadedLanguages()", []() -> CScriptArray * {
            return MakeStringArray(GetLoadedLanguages());
        })
        .function("const string &GetLanguageDisplayName(const string &in)", &GetLanguageDisplayName)
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
        // マウス（任意のウィンドウ視点でのマウス座標。自分のウィンドウならWindowObject側のメソッド版でも取得可）
        .function("Vector2 GetMousePosition(WindowObject@ window)", [](ScriptComponentHandle<IWindowObjectComponent> *window) -> Vector2 {
            IWindowObjectComponent *resolved = window ? window->Resolve() : nullptr;
            if (window && !resolved) { ThrowDestroyedObjectException(); return Vector2::Zero(); }
            return GetWindowMousePosition(resolved);
        })
        .function("bool IsMouseInsideWindow(WindowObject@ window)", [](ScriptComponentHandle<IWindowObjectComponent> *window) -> bool {
            IWindowObjectComponent *resolved = window ? window->Resolve() : nullptr;
            if (window && !resolved) { ThrowDestroyedObjectException(); return false; }
            return IsWindowMouseInside(resolved);
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
        .function("Object@ GetOwnerObject()", []() -> ScriptObjectHandle * {
            // ObjectContext::GetOwner はconstポインタを返すが、スクリプトからは自身のオブジェクトを操作できてよい
            return gCurrentObjectContext ? ScriptObjectHandle::Create(const_cast<EmptyObject *>(gCurrentObjectContext->GetOwner())) : nullptr;
        })
        .function("Transform@ GetTransform()", []() -> ScriptComponentHandle<Transform> * {
            return gCurrentObjectContext ? ScriptComponentHandle<Transform>::Create(gCurrentObjectContext->GetComponent<Transform>()) : nullptr;
        })
        .function("Scene@ GetScene()", []() -> SceneContext * { return gCurrentSceneContext; })
        .function("Object@ FindObject(const string &in)", [](const std::string &name) -> ScriptObjectHandle * {
            return gCurrentSceneContext ? ScriptObjectHandle::Create(gCurrentSceneContext->GetSceneObject(name)) : nullptr;
        })
        // objがまだシーン内に存在する有効なオブジェクトかどうかを判定する。
        // ObjectはUUID保持のScriptObjectHandle経由のため、ハンドル自体は常に有効（生存）だが、
        // 参照先のEmptyObjectが削除済みの場合はResolve()がnullptrを返す
        .function("bool IsValidObject(Object@)", [](ScriptObjectHandle *handle) -> bool {
            return handle && handle->Resolve() != nullptr;
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
    RegisterDebugDrawBindings(engine);
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
