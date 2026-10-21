#pragma once
#include <string>
#include <vector>

#include "Utilities/FileIO/JSON.h"
#include "Utilities/MyAny.h"

namespace KashipanEngine {

class IObjectComponent;
class ObjectContext;

/// @brief 同オブジェクトのコンポーネントパラメータへのバインド先1件分
/// @details 値の適用先を「コンポーネント型名＋同型内インデックス＋パラメータ名（＋成分）」で指す。
///          KeyFrameAnimator / InputCommandApplier / SceneVariableApplier 等、値をパラメータへ流し込む
///          コンポーネントで共用する。適用先は以下の2種類:
///          - ADD_MEMBER_VARIABLE で登録されたメンバ変数（書き込み後は登録されたonModifiedコールバックが呼ばれる）
///          - ScriptComponent の [SerializeField] 付き float 変数（isScriptVariable = true。数値のみ対応）
struct ParameterBinding {
    std::string componentType;     ///< 適用先コンポーネントの型名（例: "Transform"）
    int componentIndex = 0;        ///< 同型コンポーネントが複数ある場合のインデックス（追加順）
    std::string parameterName;     ///< メンバ変数名（例: "translate_"）またはスクリプト変数名
    /// @brief Vector2/3/4の成分 (0=x, 1=y, 2=z, 3=w)。float/doubleでは無視。
    ///        -1 は「値全体」を意味し、ApplyParameterBindingValue でVector2/3/4・String・
    ///        Quaternion・Matrix4x4をそのまま書き込む対象を指す（ApplyParameterBindingでは未使用）
    int channel = 0;
    bool isScriptVariable = false; ///< ScriptComponentの[SerializeField]変数への適用かどうか
};

/// @brief float値をバインド先へ書き込む
/// @param objectContext 対象オブジェクトのコンテキスト
/// @param binding バインド先
/// @param value 書き込む値
/// @param self 呼び出し元コンポーネント（自分自身への適用を防ぐ。nullptr可）
/// @return 書き込みに成功した場合は true
bool ApplyParameterBinding(ObjectContext *objectContext, const ParameterBinding &binding, float value, const IObjectComponent *self);

/// @brief 任意型（MyAny）の値をバインド先へ書き込む
/// @details バインド先メンバの型に応じて挙動が変わる:
///          - Float/Double/Bool/Int32: valueが数値系（Bool/Int32/Float/Double）なら変換して書き込む
///          - Vector2/3/4: binding.channel >= 0 なら数値をその成分へ、channel < 0 なら
///            valueが同じ型（Vector2/3/4）のときに値全体を書き込む
///          - String/Quaternion/Matrix4x4: valueが同じ型のときに値全体を書き込む
///          型が合わない・変換できない場合は何もせず false を返す
/// @param objectContext 対象オブジェクトのコンテキスト
/// @param binding バインド先
/// @param value 書き込む値
/// @param self 呼び出し元コンポーネント（自分自身への適用を防ぐ。nullptr可）
/// @return 書き込みに成功した場合は true
bool ApplyParameterBindingValue(ObjectContext *objectContext, const ParameterBinding &binding, const MyAny &value, const IObjectComponent *self);

JSON SaveParameterBindingToJson(const ParameterBinding &binding);
ParameterBinding LoadParameterBindingFromJson(const JSON &json);

#if defined(USE_IMGUI)

/// @brief バインド先のコンボ表示用候補
struct ParameterBindingCandidate {
    ParameterBinding binding;
    std::string label;
};

/// @brief 同オブジェクトの全コンポーネントからバインド可能なパラメータを列挙する
/// @details 候補は各コンポーネントのfloat/double/Vector2/3/4メンバ変数（Vector型は成分単位）と、
///          ScriptComponentの[SerializeField]付きfloat変数。表示ラベルでソートされる
/// @param self 呼び出し元コンポーネント（候補から除外する。nullptr可）
std::vector<ParameterBindingCandidate> CollectParameterBindingCandidates(ObjectContext *objectContext, const IObjectComponent *self);

/// @brief 値の型（sourceType）と互換性のあるバインド候補を列挙する（SceneVariableApplier等、値の型が実行時に決まる用途向け）
/// @details sourceTypeが数値系（Bool/Int32/Float/Double）の場合は CollectParameterBindingCandidates と同様に
///          float/double/Vector2/3/4（成分単位）・ScriptComponentのfloat変数を列挙する。
///          それ以外（Vector2/3/4・String・Quaternion・Matrix4x4）の場合は、完全に同じ型のメンバ変数のみを
///          値全体の書き込み対象（channel = -1）として列挙する
/// @param self 呼び出し元コンポーネント（候補から除外する。nullptr可）
std::vector<ParameterBindingCandidate> CollectParameterBindingCandidatesForType(ObjectContext *objectContext, const IObjectComponent *self, const TypeInfo &sourceType);

/// @brief バインド一覧の編集UI（見出し・追加ボタン・対象選択コンボ・削除ボタン）を表示する
void ShowParameterBindingListImGui(std::vector<ParameterBinding> &bindings, const std::vector<ParameterBindingCandidate> &candidates);

#endif // USE_IMGUI

} // namespace KashipanEngine
