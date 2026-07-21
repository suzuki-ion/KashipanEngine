#pragma once
#include <string>
#include <vector>

#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

class IObjectComponent;
class ObjectContext;

/// @brief 同オブジェクトのコンポーネントパラメータへのバインド先1件分
/// @details float値の適用先を「コンポーネント型名＋同型内インデックス＋パラメータ名（＋成分）」で指す。
///          KeyFrameAnimator / InputCommandApplier 等、値をパラメータへ流し込むコンポーネントで共用する。
///          適用先は以下の2種類:
///          - ADD_MEMBER_VARIABLE で登録されたメンバ変数（float/doubleはそのまま、Vector2/3/4は成分を指定。
///            書き込み後は登録されたonModifiedコールバックが呼ばれる）
///          - ScriptComponent の [SerializeField] 付き float 変数（isScriptVariable = true）
struct ParameterBinding {
    std::string componentType;     ///< 適用先コンポーネントの型名（例: "Transform"）
    int componentIndex = 0;        ///< 同型コンポーネントが複数ある場合のインデックス（追加順）
    std::string parameterName;     ///< メンバ変数名（例: "translate_"）またはスクリプト変数名
    int channel = 0;               ///< Vector2/3/4の成分 (0=x, 1=y, 2=z, 3=w)。float/doubleでは無視
    bool isScriptVariable = false; ///< ScriptComponentの[SerializeField]変数への適用かどうか
};

/// @brief float値をバインド先へ書き込む
/// @param objectContext 対象オブジェクトのコンテキスト
/// @param binding バインド先
/// @param value 書き込む値
/// @param self 呼び出し元コンポーネント（自分自身への適用を防ぐ。nullptr可）
/// @return 書き込みに成功した場合は true
bool ApplyParameterBinding(ObjectContext *objectContext, const ParameterBinding &binding, float value, const IObjectComponent *self);

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

/// @brief バインド一覧の編集UI（見出し・追加ボタン・対象選択コンボ・削除ボタン）を表示する
void ShowParameterBindingListImGui(std::vector<ParameterBinding> &bindings, const std::vector<ParameterBindingCandidate> &candidates);

#endif // USE_IMGUI

} // namespace KashipanEngine
