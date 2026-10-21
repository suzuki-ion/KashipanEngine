#pragma once
#include <cstdint>
#include <string>

#include "Math/Vector3.h"

class asIScriptEngine;

namespace KashipanEngine {

class EmptyObject;
class ObjectContext;
class SceneContext;
class ICollider;
class IWindowObjectComponent;
class ScriptObjectHandle;
template <typename T> class ScriptComponentHandle;

/// @brief スクリプトの衝突コールバックへ渡す衝突情報（スクリプト側の HitInfo 型）
/// @details selfObject/otherObjectはAngelScript側の参照カウント式ハンドル(ScriptObjectHandle)、
///          selfCollider/otherColliderも同様の参照カウント式ハンドル(ScriptComponentHandle<ICollider>)。
///          設定側（ScriptComponent::CallCollisionMethod）がCreate()で生成しExecute()後にRelease()する
struct ScriptHitInfo final {
    Vector3 normal{ 0.0f, 0.0f, 0.0f };
    float penetration = 0.0f;
    ScriptObjectHandle *selfObject = nullptr;
    ScriptObjectHandle *otherObject = nullptr;
    ScriptComponentHandle<ICollider> *selfCollider = nullptr;
    ScriptComponentHandle<ICollider> *otherCollider = nullptr;
};

/// @brief スクリプトのウィンドウメッセージコールバックへ渡す情報（スクリプト側の WindowMessageInfo 型）
/// @details sourceComponentはAngelScript側の参照カウント式ハンドル(ScriptComponentHandle<IWindowObjectComponent>)。
///          設定側（ScriptComponent::CallWindowMessageMethod）がCreate()で生成しExecute()後にRelease()する
struct ScriptWindowMessageInfo final {
    /// @brief 通知元のウィンドウコンポーネント（どのコンポーネントのウィンドウかの識別用）
    ScriptComponentHandle<IWindowObjectComponent> *sourceComponent = nullptr;
    std::uint32_t message = 0;
    std::uint64_t wparam = 0;
    std::int64_t lparam = 0;
};

/// @brief エンジンの機能をAngelScriptへ登録する
/// @details 数学型(Vector2/Vector3/Vector4/Quaternion/Matrix3x3/Matrix4x4)、全オブジェクトコンポーネント、
///          オブジェクト(Object)、シーン(Scene)、衝突情報(HitInfo)、ScriptComponentBehaviorインターフェース、
///          ログ/時間/入力コマンド/音声などのグローバル関数を登録する。
///          文字列型(string)と配列型(array)を使用するため、RegisterStdString/RegisterScriptArray の後に呼ぶこと。
/// @param engine 登録先のスクリプトエンジン
void RegisterEngineScriptBindings(asIScriptEngine *engine);

/// @brief エンジンに登録済みの型・関数からVSCodeのAngelScript Language Server用の型定義ファイル(as.predefined)を生成する
/// @details 列挙型・funcdef・インターフェース・クラス・グローバルプロパティ・グローバル関数（名前空間ごと）を
///          AngelScriptの宣言構文で書き出す。RegisterEngineScriptBindings の後に呼ぶこと。
/// @param engine 登録済みのスクリプトエンジン
/// @param filePath 出力先ファイルパス（通常は "as.predefined"）
/// @return 生成に成功した場合は true
bool GenerateScriptPredefinedFile(asIScriptEngine *engine, const std::string &filePath);

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
