#pragma once
#include "Math/Vector3.h"

class asIScriptEngine;

namespace KashipanEngine {

class EmptyObject;
class ObjectContext;
class SceneContext;

/// @brief スクリプトの衝突コールバックへ渡す衝突情報（スクリプト側の HitInfo 型）
struct ScriptHitInfo final {
    Vector3 normal{ 0.0f, 0.0f, 0.0f };
    float penetration = 0.0f;
    EmptyObject *selfObject = nullptr;
    EmptyObject *otherObject = nullptr;
};

/// @brief エンジンの機能をAngelScriptへ登録する
/// @details 数学型(Vector2/Vector3/Vector4/Quaternion/Matrix3x3/Matrix4x4)、全オブジェクトコンポーネント、
///          オブジェクト(Object)、シーン(Scene)、衝突情報(HitInfo)、ScriptComponentBehaviorインターフェース、
///          ログ/時間/入力コマンド/音声などのグローバル関数を登録する。
///          文字列型(string)と配列型(array)を使用するため、RegisterStdString/RegisterScriptArray の後に呼ぶこと。
/// @param engine 登録先のスクリプトエンジン
void RegisterEngineScriptBindings(asIScriptEngine *engine);

/// @brief スクリプト実行中のオーナーコンテキストを設定するRAIIスコープ
/// @details スクリプト側の GetOwnerObject()/GetTransform()/GetScene()/FindObject()/GetComponent() は
///          このスコープで設定されたコンテキストを参照する。関数呼び出しの間だけ生存させること。
class ScriptExecutionScope final {
public:
    ScriptExecutionScope(ObjectContext *objectContext, SceneContext *sceneContext);
    ~ScriptExecutionScope();

    ScriptExecutionScope(const ScriptExecutionScope &) = delete;
    ScriptExecutionScope &operator=(const ScriptExecutionScope &) = delete;
    ScriptExecutionScope(ScriptExecutionScope &&) = delete;
    ScriptExecutionScope &operator=(ScriptExecutionScope &&) = delete;

    /// @brief 現在実行中のスクリプトのオーナーオブジェクトコンテキストを取得（未設定の場合は nullptr）
    static ObjectContext *GetCurrentObjectContext();
    /// @brief 現在実行中のスクリプトのシーンコンテキストを取得（未設定の場合は nullptr）
    static SceneContext *GetCurrentSceneContext();

private:
    ObjectContext *prevObjectContext_ = nullptr;
    SceneContext *prevSceneContext_ = nullptr;
};

} // namespace KashipanEngine
