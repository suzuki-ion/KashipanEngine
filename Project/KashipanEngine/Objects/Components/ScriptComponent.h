#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"

class asIScriptContext;
class asIScriptEngine;
class asIScriptFunction;
class asIScriptObject;
class asITypeInfo;
class CScriptBuilder;
struct Vector3;

namespace KashipanEngine {

class SceneScriptEngine;
class ICollider;
class EmptyObject;

/// @brief AngelScriptのスクリプトファイルをコンパイルして実行するオブジェクトコンポーネント
/// @details スクリプト内で ScriptComponentBehavior インターフェースを実装したクラスを定義すると、
///          そのクラスがインスタンス化され、以下のメソッドが呼び出される。
///          - void Start()  : 初期化時に一度だけ
///          - void Update() : 毎フレーム
///          - void End()    : 終了時（コンポーネント削除・非アクティブ化・リロード時）
///          - void OnCollisionEnter/Stay/Exit(const HitInfo &in) : 同オブジェクトのコライダーの衝突時
///          クラスのメンバ変数やグローバル変数に `[SerializeField]` メタデータを付けると、
///          ImGuiのインスペクター上で編集でき、シーンへの保存/読込の対象になる。
///          対応型: bool / int / uint / float / double / string / Vector2 / Vector3 / Vector4 / Quaternion
class ScriptComponent final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(ScriptComponent, 0xFF, )
    COMPONENT_CATEGORY("Script")
    ~ScriptComponent() override;

    std::unique_ptr<IObjectComponent> Clone() const override;

    /// @brief 実行するスクリプトファイルのパスを設定する
    void SetScriptPath(const std::string &scriptPath) { scriptPath_ = scriptPath; }
    const std::string &GetScriptPath() const noexcept { return scriptPath_; }

    /// @brief スクリプトを（再）コンパイルする。既にビルド済みの場合は End() を呼んでから再構築する
    /// @details [SerializeField] 付き変数の現在値はリロード後も維持される
    /// @return コンパイルに成功した場合は true
    bool Reload();

    /// @brief 直近のコンパイル/実行時エラー内容を取得する（無い場合は空文字）
    const std::string &GetLastError() const noexcept { return lastError_; }

protected:
    void Initialize() override;
    void Finalize() override;
    void Update() override;

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

private:
    /// @brief [SerializeField] が付いたスクリプト変数の情報
    struct SerializedField {
        std::string name;
        int typeId = 0;
        void *address = nullptr;
    };
    /// @brief コライダーへ設定した衝突コールバックのフック情報（定義はcpp内）
    struct ColliderHooks;

    SceneScriptEngine *GetSceneScriptEngine() const;
    SceneScriptEngine *GetOrAddSceneScriptEngine() const;
    void ReleaseScript();

    /// @brief モジュール内から ScriptComponentBehavior を実装したクラスを探してインスタンス化する
    /// @return 成功した場合は true（失敗時は lastError_ にエラー内容を格納する）
    bool CreateBehaviorInstance(asIScriptEngine *engine, CScriptBuilder &builder);
    /// @brief Behaviorインスタンスのメソッドを引数無しで実行する
    void CallMethod(asIScriptFunction *method);
    /// @brief Behaviorインスタンスの衝突メソッドを HitInfo 引数付きで実行する
    void CallCollisionMethod(asIScriptFunction *method, const Vector3 &normal, float penetration,
        EmptyObject *selfObject, EmptyObject *otherObject);

    /// @brief 同オブジェクトのコライダー数を数える
    size_t CountColliders() const;
    /// @brief 同オブジェクトの全コライダーへ衝突コールバックを設定する（既存のコールバックはチェーンされる）
    void HookColliders();
    /// @brief 設定した衝突コールバックを元に戻す
    void UnhookColliders();

    /// @brief [SerializeField] 付き変数（グローバル変数とBehaviorクラスのメンバ変数）を収集する
    void CollectSerializedFields(CScriptBuilder &builder);
    /// @brief 収集済みフィールドの現在値をJSONへ書き出す（未ビルド時は pendingFieldValues_ をそのまま返す）
    JSON CaptureFieldValuesToJson() const;
    /// @brief JSONの値を収集済みフィールドへ適用する
    void ApplyFieldValuesFromJson(const JSON &json);

    std::string scriptPath_;
    std::string moduleName_;
    asIScriptContext *context_ = nullptr;

    /// @brief ScriptComponentBehaviorを実装したスクリプトクラスのインスタンス
    asIScriptObject *behaviorObject_ = nullptr;
    asITypeInfo *behaviorType_ = nullptr;
    asIScriptFunction *startMethod_ = nullptr;
    asIScriptFunction *updateMethod_ = nullptr;
    asIScriptFunction *endMethod_ = nullptr;
    asIScriptFunction *onCollisionEnterMethod_ = nullptr;
    asIScriptFunction *onCollisionStayMethod_ = nullptr;
    asIScriptFunction *onCollisionExitMethod_ = nullptr;

    std::string lastError_;

    std::vector<SerializedField> serializedFields_;
    /// @brief 次回ビルド時に適用する [SerializeField] の値（読込済み・リロード退避用）
    JSON pendingFieldValues_ = JSON::object();
    /// @brief 登録済み型のタイプID（ビルド時にキャッシュ）
    int stringTypeId_ = 0;
    int vector2TypeId_ = 0;
    int vector3TypeId_ = 0;
    int vector4TypeId_ = 0;
    int quaternionTypeId_ = 0;

    /// @brief コライダーへ設定した衝突コールバックのフック情報
    /// @details 不完全型を保持するためshared_ptrを使用（デリータが型消去されるため宣言時に完全型が不要）
    std::shared_ptr<ColliderHooks> colliderHooks_;
    /// @brief 衝突コールバックのラムダから自身の生存確認を行うためのトークン
    std::shared_ptr<ScriptComponent *> aliveToken_ = std::make_shared<ScriptComponent *>(this);
};

REGISTER_COMPONENT_OBJECT(ScriptComponent)

} // namespace KashipanEngine
