#pragma once
#include <cstdint>
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
class CScriptArray;
struct Vector3;

namespace KashipanEngine {

class SceneScriptEngine;
class ICollider;
class IWindowObjectComponent;
class EmptyObject;

/// @brief AngelScriptのスクリプトファイルをコンパイルして実行するオブジェクトコンポーネント
/// @details スクリプト内で ScriptComponentBehavior インターフェースを実装したクラスを定義すると、
///          そのクラスがインスタンス化され、以下のメソッドが呼び出される。
///          - void Start()  : 初期化時に一度だけ
///          - void Update() : 毎フレーム
///          - void End()    : 終了時（コンポーネント削除・非アクティブ化・リロード時）
///          - void OnCollisionEnter/Stay/Exit(const HitInfo &in) : 同オブジェクトのコライダーの衝突時
///          - void OnWindowMessage(const WindowMessageInfo &in) : 同オブジェクトのWindowObject系
///            コンポーネントのウィンドウがメッセージを受信した時（通知元コンポーネント情報付き）
///          クラスのメンバ変数やグローバル変数に `[SerializeField]` メタデータを付けると、
///          ImGuiのインスペクター上で編集でき、シーンへの保存/読込の対象になる。
///          対応型: bool / int / uint / float / double / string / Vector2 / Vector3 / Vector4 / Quaternion
///          / Object（シーン内オブジェクトへの参照。インスペクターでコンボからの選択、
///          およびヒエラルキーからのドラッグ&ドロップで指定できる）
///          および [System.Serializable] を付けたスクリプトクラス（public変数は自動対象、
///          private/protected変数は [SerializeField] が必要）、
///          これら対応型の array<T>（ネスト配列・Serializableクラスの配列も可。ただし
///          `array<T>@ 変数名 = null;` のように必ず明示的なハンドル構文＋null初期値で宣言すること。
///          `array<T> 変数名;` 等の値的構文はAngelScript側のGC追跡が壊れ、インスペクター表示時や
///          エンジン終了時にクラッシュする。壊れたハンドルは検出時に自動で新しい空配列へ差し替えるが、
///          根本原因はAngelScript側にあるため必ずnull初期値で宣言すること）
///          表示用の属性（Unity互換）: [Range(min, max)] [TextArea(minLines, maxLines)] [Multiline]
///          [ColorPicker]（Vector4を色として編集） [Header("見出し")] [Space(高さpx)] [Tooltip("説明")]
///          `[SerializeField, Range(1, 10)]` のように1つのブロックへカンマ区切りでまとめて記述できる
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

    /// @brief このスクリプトの [SerializeField] 付き変数を名前で取得する（スクリプト間のデータ受け渡し用）
    /// @details シーン変数と異なり、他オブジェクトの ScriptComponent を GetComponent で取得した上で
    ///          直接読み書きする経路。対応型は [SerializeField] と同じプリミティブ/数学型/Object@のみで、
    ///          array<T> や [System.Serializable] クラスは非対応（モジュールをまたぐと型が一致しないため）
    /// @param name 変数名
    /// @param ref 書き込み先のアドレス
    /// @param typeId 書き込み先の型ID
    /// @return 名前・型が一致する変数が見つかった場合は true
    bool GetVariable(const std::string &name, void *ref, int typeId) const;
    /// @brief このスクリプトの [SerializeField] 付き変数へ名前で値を設定する（スクリプト間のデータ受け渡し用）
    /// @details 対応型・非対応型はGetVariableと同じ
    bool SetVariable(const std::string &name, void *ref, int typeId);

    /// @brief スクリプト変数に付与された属性（Unity互換メタデータ）
    struct FieldAttributes {
        bool serializeField = false;  ///< [SerializeField]
        bool hasRange = false;        ///< [Range(min, max)]
        float rangeMin = 0.0f;
        float rangeMax = 0.0f;
        bool multiline = false;       ///< [Multiline] / [TextArea(minLines, maxLines)]
        int minLines = 3;
        int maxLines = 3;
        bool colorPicker = false;     ///< [ColorPicker]（Vector4を色として編集する）
        std::string header;           ///< [Header("...")]（空なら無し）
        bool hasSpace = false;        ///< [Space(高さpx)]
        float space = 8.0f;
        std::string tooltip;          ///< [Tooltip("...")]（空なら無し）
    };

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
        /// @brief 変数の格納先アドレス（ルート階層のみ。子フィールドはハンドルの差し替えに
        ///        追従するため、アクセス時に毎回親オブジェクトから解決する）
        void *address = nullptr;
        /// @brief 子フィールドの場合の、親クラス内でのプロパティインデックス
        uint32_t propertyIndex = 0;
        /// @brief [System.Serializable] が付いたスクリプトクラス型かどうか
        bool isScriptObject = false;
        /// @brief array<T> 型かどうか（trueの場合、children[0]が要素のフィールド情報になる）
        bool isArray = false;
        FieldAttributes attributes;
        /// @brief isScriptObject の場合のサブフィールド（isArray の場合は要素のフィールド情報1件）
        std::vector<SerializedField> children;
    };
    /// @brief コライダーへ設定した衝突コールバックのフック情報（定義はcpp内）
    struct ColliderHooks;
    /// @brief WindowObject系コンポーネントへ設定したメッセージコールバックのフック情報（定義はcpp内）
    struct WindowObjectHooks;

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
        EmptyObject *selfObject, EmptyObject *otherObject,
        ICollider *selfCollider, ICollider *otherCollider);
    /// @brief Behaviorインスタンスのウィンドウメッセージメソッドを WindowMessageInfo 引数付きで実行する
    void CallWindowMessageMethod(asIScriptFunction *method, IWindowObjectComponent *sourceComponent,
        std::uint32_t message, std::uint64_t wparam, std::int64_t lparam);

    /// @brief 同オブジェクトのコライダー数を数える
    size_t CountColliders() const;
    /// @brief 同オブジェクトの全コライダーへ衝突コールバックを設定する（既存のコールバックはチェーンされる）
    void HookColliders();
    /// @brief 設定した衝突コールバックを元に戻す
    void UnhookColliders();

    /// @brief 同オブジェクトのWindowObject系コンポーネント数を数える
    size_t CountWindowObjects() const;
    /// @brief 同オブジェクトの全WindowObject系コンポーネントへメッセージコールバックを設定する（既存のコールバックはチェーンされる）
    void HookWindowObjects();
    /// @brief 設定したメッセージコールバックを元に戻す
    void UnhookWindowObjects();

    /// @brief 指定タイプIDがシリアライズ対応のプリミティブ/数学型かを判定する
    bool IsSupportedFieldType(int typeId) const;
    /// @brief 指定タイプIDがObject型（ハンドル修飾の有無を問わない）かを判定する
    bool IsObjectFieldType(int typeId) const;
    /// @brief 単純な値型（プリミティブ/数学型/Object@）のフィールド値を型IDに応じてコピーする
    void CopyLeafFieldValue(int typeId, void *dst, const void *src) const;
    /// @brief [SerializeField] 付き変数（グローバル変数とBehaviorクラスのメンバ変数）を収集する
    void CollectSerializedFields(CScriptBuilder &builder);
    /// @brief フィールドが [System.Serializable] クラス型の場合に子フィールドを再帰的に収集する
    void CollectSerializableChildren(SerializedField &field, CScriptBuilder &builder, asIScriptEngine *engine, int depth);
    /// @brief フィールドが array<T> 型の場合に要素のフィールド情報を構築する（要素型が未対応なら何もしない）
    void SetupArrayField(SerializedField &field, CScriptBuilder &builder, asIScriptEngine *engine, int depth);
    /// @brief array<T> ハンドルが実際にそのフィールドの型として妥当かを検証する
    /// @details 何らかの理由でハンドルスロットの内容が壊れている場合、GetSize()等の呼び出しが
    ///          不正なメモリアクセスでクラッシュするため、呼び出し前に必ずこれで確認する
    bool IsArrayHandleValid(CScriptArray *array, int fieldTypeId) const;
    /// @brief フィールドの現在値をJSONへ変換する（Serializableクラスはネストしたオブジェクトになる）
    JSON CaptureField(const SerializedField &field, void *address) const;
    /// @brief JSONの値をフィールドへ適用する
    void ApplyField(const SerializedField &field, void *address, const JSON &value);
    /// @brief 収集済みフィールドの現在値をJSONへ書き出す（未ビルド時は pendingFieldValues_ をそのまま返す）
    JSON CaptureFieldValuesToJson() const;
    /// @brief JSONの値を収集済みフィールドへ適用する
    void ApplyFieldValuesFromJson(const JSON &json);
#if defined(USE_IMGUI)
    /// @brief フィールドの編集UIを属性（Range/TextArea等）に応じて表示する
    void DrawFieldImGui(SerializedField &field, void *address);
#endif

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
    asIScriptFunction *onWindowMessageMethod_ = nullptr;

    std::string lastError_;
    /// @brief 直近のビルド失敗時のコンパイルメッセージ（成功時は空）
    std::vector<std::string> buildErrorMessages_;

    std::vector<SerializedField> serializedFields_;
    /// @brief 次回ビルド時に適用する [SerializeField] の値（読込済み・リロード退避用）
    JSON pendingFieldValues_ = JSON::object();
    /// @brief 登録済み型のタイプID（ビルド時にキャッシュ）
    int stringTypeId_ = 0;
    int vector2TypeId_ = 0;
    int vector3TypeId_ = 0;
    int vector4TypeId_ = 0;
    int quaternionTypeId_ = 0;
    int objectTypeId_ = 0;

    /// @brief コライダーへ設定した衝突コールバックのフック情報
    /// @details 不完全型を保持するためshared_ptrを使用（デリータが型消去されるため宣言時に完全型が不要）
    std::shared_ptr<ColliderHooks> colliderHooks_;
    /// @brief WindowObject系コンポーネントへ設定したメッセージコールバックのフック情報
    std::shared_ptr<WindowObjectHooks> windowObjectHooks_;
    /// @brief 衝突コールバックのラムダから自身の生存確認を行うためのトークン
    std::shared_ptr<ScriptComponent *> aliveToken_ = std::make_shared<ScriptComponent *>(this);
};

REGISTER_COMPONENT_OBJECT(ScriptComponent)

} // namespace KashipanEngine
