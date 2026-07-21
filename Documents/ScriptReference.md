# KashipanEngine スクリプトリファレンス

KashipanEngineに組み込まれたAngelScriptの利用方法と、スクリプトから使用できるAPIのリファレンスです。

- スクリプト言語: [AngelScript](https://www.angelcode.com/angelscript/)
- 対象コンポーネント: `ScriptComponent`（オブジェクトコンポーネント） / `SceneScriptEngine`（シーンコンポーネント）

エディター拡張用スクリプト（`EditorTools`フォルダ、メニュー項目や専用ウィンドウの追加）については [EditorToolScriptReference.md](EditorToolScriptReference.md) を参照してください。

---

## 目次

1. [基本的な使い方](#基本的な使い方)
2. [ファイルの分割（#include）](#ファイルの分割include)
3. [ScriptComponentBehavior（ライフサイクル）](#scriptcomponentbehaviorライフサイクル)
4. [SerializeField（変数のインスペクター編集・保存）](#serializefield変数のインスペクター編集保存)
    - [Object参照（シーン内オブジェクトの指定）](#object参照シーン内オブジェクトの指定)
    - [表示用の属性（Unity互換）](#表示用の属性unity互換)
    - [System.Serializable（クラスのシリアライズ）](#systemserializableクラスのシリアライズ)
    - [配列（array&lt;T&gt;）](#配列arrayt)
5. [コンポーネントの取得・追加・削除](#コンポーネントの取得追加削除)
6. [グローバル関数](#グローバル関数)
7. [オブジェクト・シーン型](#オブジェクトシーン型)
    - [Tag（タグ）](#tagタグ)
8. [シーン変数（スクリプト間の値の受け渡し）](#シーン変数スクリプト間の値の受け渡し)
    - [ScriptComponent経由のスクリプト間データ受け渡し](#scriptcomponent経由のスクリプト間データ受け渡し)
9. [dictionary（辞書型）](#dictionary辞書型)
10. [Json（JSONファイルの保存・読み込み）](#jsonjsonファイルの保存読み込み)
11. [コンポーネント型](#コンポーネント型)
12. [数学型](#数学型)
13. [Math名前空間](#math名前空間)
14. [Easing（イージング）](#easingイージング)
15. [Random（乱数）](#random乱数)
16. [WaveFunctionCollapse（波動関数崩壊アルゴリズム）](#wavefunctioncollapse波動関数崩壊アルゴリズム)
17. [StageGraphGenerator / StageGridBuilder（ステージの部屋グラフ生成）](#stagegraphgenerator--stagegridbuilderステージの部屋グラフ生成)
18. [サンプルスクリプト](#サンプルスクリプト)
19. [注意事項](#注意事項)

---

## 基本的な使い方

1. `.as` 拡張子のスクリプトファイルを作成する
2. オブジェクトに `ScriptComponent` を追加する（エディターの Add Component → Script → ScriptComponent）
3. インスペクターの `Script Path` にスクリプトファイルのパスを設定する
4. スクリプト内で `ScriptComponentBehavior` を実装したクラスを定義する

- スクリプトエンジン本体はシーンコンポーネント `SceneScriptEngine` が管理します。シーンに未追加の場合は `ScriptComponent` の初期化時に自動で追加されるため、手動で追加する必要はありません。
- スクリプトは `ScriptComponent` ごとに独立したモジュールとしてコンパイルされます。
- インスペクターの `Reload` ボタンでスクリプトを再コンパイルできます（`[SerializeField]` 付き変数の値は維持されます）。
- コンパイルエラー・実行時例外はエンジンのログとインスペクターに出力されます（ビルド失敗時はコンパイラのエラーメッセージもインスペクターに表示されます）。
- 標準アドオンの `string`（文字列）・`array`（配列）・`dictionary`（[辞書型](#dictionary辞書型)）が使用できます。

### VSCodeでのコード補完（as.predefined）

スクリプトエンジンの初期化時に、登録済みの全API定義を書き出した `as.predefined` ファイルが実行ディレクトリ（`Assets` フォルダと同じ階層）へ自動生成されます。VSCodeの **AngelScript Language Server** 拡張機能はこのファイルを読み込んでコード補完・型チェックを行うため、スクリプトを含むフォルダをVSCodeで開くだけでエンジンAPIの補完が有効になります。エンジン側のバインディングを変更した場合は、エンジンを一度起動すると最新の内容で再生成されます。

## ファイルの分割（#include）

他の `.as` ファイルを取り込むために `#include` ディレクティブが使用できます。

```angelscript
// Assets/Scripts/Player.as
#include "Utils/MathHelpers.as"

class Player : ScriptComponentBehavior {
    void Update() {
        float smoothed = SmoothStep(0.0f, 1.0f, 0.5f); // MathHelpers.as で定義した関数
    }
}
```

- 相対パスで指定した場合、`#include` を書いたファイルと同じディレクトリからの相対パスとして解決されます（ネストしたincludeも、そのファイル自身の場所を基準に解決されます）。
- 絶対パス（`C:\...` やスラッシュ始まり）を指定した場合はそのまま使用されます。
- 同じファイルが複数箇所からincludeされても二重に取り込まれません。
- includeされたファイルもコンパイルエラー時のメッセージ（ファイル名・行番号）に反映されます。

## ScriptComponentBehavior（ライフサイクル）

スクリプト内で `ScriptComponentBehavior` インターフェースを実装したクラスを定義すると、モジュール内で最初に見つかったクラスがインスタンス化され、以下のメソッドが呼び出されます。**全てのメソッドは省略可能**で、定義されているものだけが呼ばれます。

```angelscript
class Player : ScriptComponentBehavior {
    void Awake() {}                                  // インスタンス生成時に一度だけ
    void Start() {}                                  // ゲームループ中、最初のUpdate()の直前に一度だけ
    void Update() {}                                 // 毎フレーム
    void End() {}                                    // 終了時
    void OnCollisionEnter(const HitInfo &in hit) {}  // 衝突開始時
    void OnCollisionStay(const HitInfo &in hit) {}   // 衝突継続中（毎フレーム）
    void OnCollisionExit(const HitInfo &in hit) {}   // 衝突終了時
    void OnWindowMessage(const WindowMessageInfo &in info) {} // ウィンドウメッセージ受信時
}
```

| メソッド | 呼び出しタイミング |
|---|---|
| `void Awake()` | コンポーネントの初期化時（スクリプトのコンパイル成功後、インスタンス生成直後）、およびReload成功後に一度（UnityのAwake相当） |
| `void Start()` | ゲームループ中、そのインスタンスに対して最初にUpdate()が呼ばれる直前に一度だけ（UnityのStart相当。非アクティブのまま一度もUpdate()が呼ばれなければ実行されない） |
| `void Update()` | 毎フレーム |
| `void End()` | コンポーネントの削除・非アクティブ化・Reload時 |
| `void OnCollisionEnter(const HitInfo &in)` | 同オブジェクトのコライダーが他のコライダーと衝突を開始したとき |
| `void OnCollisionStay(const HitInfo &in)` | 衝突が継続している間、毎フレーム |
| `void OnCollisionExit(const HitInfo &in)` | 衝突が終了したとき |
| `void OnWindowMessage(const WindowMessageInfo &in)` | 同オブジェクトのWindowObject系コンポーネントのウィンドウがメッセージを受信したとき（[詳細](#ウィンドウメッセージonwindowmessage)） |

衝突イベントは同オブジェクトに付いている全ての `ICollider` 派生コンポーネント（2D/3D両方）が対象です。C++側で既に衝突コールバックが設定されている場合、そのコールバックが先に呼ばれた後にスクリプト側が呼ばれます。

高速に移動するオブジェクトが相手をすり抜けて衝突イベントが発生しない場合は、コライダーの[連続衝突判定（CCD）](#連続衝突判定ccd)を有効にしてください。

ウィンドウメッセージイベントは同オブジェクトに付いている全ての `NormalWindowObject` / `OverlayWindowObject` コンポーネントが対象です（[詳細](#ウィンドウメッセージonwindowmessage)）。

### HitInfo

衝突メソッドへ渡される衝突情報です。

| プロパティ | 型 | 内容 |
|---|---|---|
| `normal` | `Vector3` | 衝突面の法線 |
| `penetration` | `float` | めり込み量 |
| `selfObject` | `Object@` | 自身のオブジェクト |
| `otherObject` | `Object@` | 衝突相手のオブジェクト |
| `selfCollider` | `Collider@` | 衝突判定を行った自身のコライダーコンポーネント |
| `otherCollider` | `Collider@` | 衝突相手のコライダーコンポーネント |

`selfCollider` / `otherCollider` で「どのコライダーコンポーネント同士の衝突なのか」を特定できます。1つのオブジェクトに複数のコライダーが付いている場合の判別や、相手コライダーのトリガー状態・タグの確認に使用してください。`Collider` は全コライダー型の基底型です（[詳細](#collider基底型)）。

### WindowMessageInfo

`OnWindowMessage` へ渡されるウィンドウメッセージ情報です。

| プロパティ | 型 | 内容 |
|---|---|---|
| `sourceComponent` | `WindowObject@` | 通知元のウィンドウコンポーネント |
| `message` | `uint` | メッセージ種別（`WM_SIZE` / `WM_KEYDOWN` など） |
| `wParam` | `uint64` | メッセージの追加情報（WPARAM） |
| `lParam` | `int64` | メッセージの追加情報（LPARAM） |

`sourceComponent` で「どのウィンドウコンポーネントからの通知か」を特定できます。`WindowObject` はウィンドウ系コンポーネントの基底型です（[詳細](#windowobject基底型)）。

## SerializeField（変数のインスペクター編集・保存）

変数の宣言に `[SerializeField]` メタデータを付けると、その変数は以下の対象になります。

- ImGuiインスペクターの「Serialize Fields」セクションでの編集
- シーン保存時のJSONへの書き出し / シーン読込時の復元
- `Reload` 時の値の維持

```angelscript
class Player : ScriptComponentBehavior {
    [SerializeField]
    float moveSpeed = 5.0f;

    [SerializeField]
    Vector3 offset(0.0f, 1.0f, 0.0f);
}

// グローバル変数にも付けられる
[SerializeField]
int stageNo = 1;
```

対応している型:

| 分類 | 型 |
|---|---|
| プリミティブ | `bool` / `int` / `uint` / `float` / `double` |
| 文字列 | `string` |
| 数学型 | `Vector2` / `Vector3` / `Vector4` / `Quaternion` |
| オブジェクト参照 | `Object@`（[詳細](#object参照シーン内オブジェクトの指定)） |
| クラス | [System.Serializable](#systemserializableクラスのシリアライズ) を付けたスクリプトクラス |
| 配列 | 上記対応型の `array<T>`（[詳細](#配列arrayt)。ネスト配列・Serializableクラスの配列も可） |

上記以外の型に付けた場合、インスペクターには `(unsupported type)` と表示され、保存対象になりません。

### Object参照（シーン内オブジェクトの指定）

`Object@` 型の変数に `[SerializeField]` を付けると、インスペクター上でシーン内の他オブジェクトを参照として指定できます。

```angelscript
class CameraRig : ScriptComponentBehavior {
    [SerializeField, Tooltip("追従対象のオブジェクト")]
    Object@ target;

    void Update() {
        if (target is null) return;
        Transform@ targetTf;
        if (target.GetComponent(@targetTf)) {
            // targetTf を使った処理
        }
    }
}
```

- インスペクターにはコンボボックスが表示され、シーン内オブジェクトから選択できます。
- ヒエラルキーウィンドウのオブジェクトをインスペクターの当該フィールドへドラッグ&ドロップして指定することもできます。
- 内部的にはオブジェクトのUUIDとして保存されるため、シーンの保存/読込やスクリプトの`Reload`をまたいでも参照が維持されます（参照先オブジェクト自体が削除された場合は `null` になります）。
- `array<Object@>` として複数のオブジェクトをまとめて参照することもできます。

### 表示用の属性（Unity互換）

`[SerializeField]` 付き変数には、UnityのC#と同様の属性を付けてインスペクターでの表示方法を変更できます。

| 属性 | 対象の型 | 効果 |
|---|---|---|
| `[Range(min, max)]` | `int` / `uint` / `float` / `double` | ドラッグ入力の代わりに min〜max のスライダーで編集する |
| `[TextArea]` / `[TextArea(minLines, maxLines)]` | `string` | 複数行のテキストエリアで編集する。内容の行数に応じて minLines〜maxLines（既定 3〜3）の範囲で高さが自動調整される |
| `[Multiline]` / `[Multiline(行数)]` | `string` | 固定行数（既定3行）の複数行テキストエリアで編集する |
| `[ColorPicker]` | `Vector4` | RGBAの色としてカラーピッカーで編集する |
| `[Header("見出しのテキスト")]` | 任意 | フィールドの上に見出し（セパレーター付き）を表示する |
| `[Space]` / `[Space(高さpx)]` | 任意 | フィールドの上に余白を挿入する（既定8px） |
| `[Tooltip("説明文")]` | 任意 | 項目にマウスを乗せたときに説明文を表示する |

属性はUnityと同様に、1つのブロックへカンマ区切りでまとめて書いても、別々のブロックに分けて書いても機能します。

```angelscript
class Player : ScriptComponentBehavior {
    // まとめて書く場合
    [SerializeField, Range(1, 10), Tooltip("移動速度")]
    float moveSpeed = 5.0f;

    // 分けて書く場合
    [SerializeField]
    [Header("見た目の設定")]
    [ColorPicker]
    Vector4 tintColor(1.0f, 1.0f, 1.0f, 1.0f);

    [SerializeField, Space, TextArea(3, 8)]
    string description;
}
```

### System.Serializable（クラスのシリアライズ）

スクリプトで定義したクラスに `[System.Serializable]` を付けると、そのクラス型の `[SerializeField]` 付き変数がインスペクターでツリー展開され、中のメンバ変数を編集・保存できます。

- クラスの **publicメンバは自動的に対象**になります（`[SerializeField]` 不要）
- **private / protectedメンバは `[SerializeField]` を付けた場合のみ**対象になります
- メンバ変数にも `[Range]` などの表示用の属性を付けられます
- Serializableクラスの中にさらにSerializableクラスを持つネストにも対応しています

```angelscript
[System.Serializable]
class JumpSettings {
    float power = 5.0f;              // publicなので自動的に対象
    [Range(0.1, 2.0)]
    float chargeTime = 0.5f;

    [SerializeField]
    private float internalValue = 0; // privateはSerializeFieldが必要
}

class Player : ScriptComponentBehavior {
    [SerializeField, Header("ジャンプ設定")]
    JumpSettings jump;
}
```

### 配列（array&lt;T&gt;）

対応型の `array<T>` にも `[SerializeField]` を付けられます。要素はJSON配列としてシーンへ保存されます。

> **重要: 必ず `array<T>@ 変数名 = null;` のように、明示的なハンドル構文＋`null`初期値で宣言してください。**
> `array<T> 変数名;` や `array<T> 変数名 = array<T>();`（`@`を書かない値的構文）で宣言すると、
> スクリプトクラスのコンストラクタ内で `array<T>` のファクトリが呼び出される際にAngelScript側のGC追跡が
> 壊れ、インスペクター表示時のクラッシュや、エンジン終了時の二重解放クラッシュを引き起こします。
> `= null` から始めれば、エンジン側（`ScriptComponent`）がその場で安全に空の配列を生成します。

```angelscript
class Player : ScriptComponentBehavior {
    [SerializeField]
    array<int>@ scores = null;

    [SerializeField, Range(0, 100)]      // Rangeなどの編集用属性は各要素に適用される
    array<float>@ weights = null;

    [SerializeField]
    array<Vector3>@ waypoints = null;

    [SerializeField]
    array<array<int>>@ grid = null;      // ネスト配列も可（内側の要素型はこれまで通りでよい）

    [SerializeField]
    array<JumpSettings>@ presets = null; // [System.Serializable]クラスの配列も可
}
```

インスペクターでは配列がツリー表示され、以下の操作ができます。

- **Size** 入力での一括リサイズ
- **＋** ボタンで末尾へ要素を追加
- 各要素の **−** ボタンでその要素を削除
- 各要素の編集（`Range` / `ColorPicker` / `TextArea` 等の編集用属性は各要素へ引き継がれます。`Header` / `Space` / `Tooltip` は配列自体に表示されます）

補足:

- 要素型が未対応の配列（コンポーネントハンドルの配列など）はシリアライズ対象になりません。
- ネストの深さ制限（8段）はSerializableクラスと共有です。
- `array<T@>`（ハンドル）で宣言したSerializableクラス配列のnull要素は、シーン読込時・エディターでの要素追加時に自動でインスタンスが生成されます。
- 万一 `@ = null` を付け忘れて壊れたハンドルが検出された場合、エンジン側で自動的に新しい空の配列へ差し替えて動作は継続しますが（クラッシュはしません）、この場合もエンジン終了時に別のクラッシュが起きる可能性があるため、必ず上記の宣言方法に従ってください。

## コンポーネントの取得・追加・削除

取得/追加したいコンポーネント型のハンドル変数（または配列）を引数に渡すと、型に応じたコンポーネントが取得/生成されます。

> **重要**: `GetComponent` / `AddComponent` / `RemoveComponent` は任意の型を受け取れるようAngelScriptの汎用引数（`?&out` / `?&in`）で実装されています。この仕組みでは、**渡す変数がハンドル型であっても呼び出し側で明示的に `@` を付けないとハンドルとして扱われません**（`GetComponent(vel)` ではなく `GetComponent(@vel)`）。`@` を付け忘れると、渡した型の値そのものを構築しようとして `No default constructor for object of type '...'.` / `No appropriate opAssign method found in '...' for value assignment` というコンパイルエラーになります（コンポーネント型は値としての構築・代入をサポートしていないため）。これはAngelScript自体の仕様（`dictionary.Set(key, @handle)` 等と同じ）であり、下記の例は全て `@` を付けています。

```angelscript
// 単体取得: 最初に見つかったコンポーネントを取得
Velocity@ vel;
if (GetOwnerObject().GetComponent(@vel)) {
    vel.AddVelocity(Vector3(0.0f, 5.0f, 0.0f));
}

// 全件取得: array<T@>@（配列自体もハンドルで宣言し、@ を付けて渡す）
array<AudioSource@>@ sources;
if (GetOwnerObject().GetComponents(@sources)) {
    for (uint i = 0; i < sources.length(); i++) {
        sources[i].Stop();
    }
}

// 追加: 渡した変数の型のコンポーネントを新規生成して追加する
BoxCollider@ col;
if (GetOwnerObject().AddComponent(@col)) {
    col.SetTrigger(true);
}

// 削除: 既に取得済みのハンドルを渡すとそのインスタンスを削除する
GetOwnerObject().RemoveComponent(@col);

// グローバル版は自身のオブジェクトが対象（obj.GetComponent(...) 等の省略形）
Transform@ tf;
GetComponent(@tf);
```

| 関数 | 戻り値 | 説明 |
|---|---|---|
| `bool Object::GetComponent(?&out)` | 見つかった場合 `true` | 型に一致する最初のコンポーネントをハンドルへ格納（`@変数` で呼び出すこと） |
| `bool Object::GetComponents(?&out)` | 成功した場合 `true` | 型に一致する全コンポーネントを `array<T@>` へ格納（0個でも配列は生成される。配列自体も `array<T@>@` とハンドルで宣言し `@変数` で呼び出すこと） |
| `bool Object::AddComponent(?&out)` | 追加できた場合 `true` | 渡した変数の型のコンポーネントを新規生成して追加し、ハンドルへ格納する（最大追加数を超える等で失敗する場合がある。`@変数` で呼び出すこと） |
| `bool Object::RemoveComponent(?&in)` | 削除できた場合 `true` | 渡したハンドルが指すコンポーネントインスタンスを削除する（`@変数` で呼び出すこと） |
| `bool GetComponent(?&out)` | 同上 | 自身のオブジェクトを対象にした省略形 |
| `bool GetComponents(?&out)` | 同上 | 自身のオブジェクトを対象にした省略形 |
| `bool AddComponent(?&out)` | 同上 | 自身のオブジェクトを対象にした省略形 |
| `bool RemoveComponent(?&in)` | 同上 | 自身のオブジェクトを対象にした省略形 |

- `AddComponent` で生成されたコンポーネントは規定値で初期化されます。既存コンポーネントの複製は [Scene.CloneObject](#オブジェクトシーン型) でオブジェクトごと複製してください（コンポーネント単体の複製はできません）。
- `RemoveComponent` で削除した後、渡したハンドル自体は自動では `null` になりません。削除後に同じハンドルへアクセスしないでください。

## グローバル関数

### ログ

| 関数 | 説明 |
|---|---|
| `void Log(const string &in)` | 情報ログを出力する |
| `void LogWarning(const string &in)` | 警告ログを出力する |
| `void LogError(const string &in)` | エラーログを出力する |

### 時間

| 関数 | 説明 |
|---|---|
| `float GetDeltaTime()` | 前フレームからの経過時間（秒）を取得する |
| `float GetGameSpeed()` | ゲームスピードを取得する |
| `void SetGameSpeed(float)` | ゲームスピードを設定する |

### 数学

| 関数 | 説明 |
|---|---|
| `float GetPI()` | 円周率を取得する |
| `float ToDegrees(float radians)` | ラジアンから度に変換する |
| `float ToRadians(float degrees)` | 度からラジアンに変換する |
| `float/Vector2/Vector3/Vector4 Clamp(value, min, max)` | `value` を `min`～`max` の範囲に収める |

### ゲームループ制御

| 関数 | 説明 |
|---|---|
| `void RequestExitGameLoop()` | ゲームループの終了を要求する |

ゲーム実行時（エディター無しビルド）はゲームループを抜けてアプリケーションが終了します。**エディター上ではエディター自体は閉じず、再生停止（Stopボタンと同じ動作）として扱われる**ため、ゲーム終了の動作確認にも安全に使用できます。`Scene` 型の `RequestExitGameLoop()` メソッドも同じ動作です。

### 入力コマンド

エンジンの `InputCommand`（`Assets/KashipanEngine/InputCommand.json` に保存される入力バインディング）を評価します。

| 関数 | 説明 |
|---|---|
| `bool IsCommandTriggered(const string &in action)` | コマンドの入力判定を取得する |
| `float GetCommandValue(const string &in action)` | コマンドの評価値（スティック軸など -1.0～1.0）を取得する |

### 音声

| 関数 | 説明 |
|---|---|
| `uint PlayAudio(const string &in path, float volume = 1.0f)` | 音声を再生する。パスはAssetsルートからの相対パスまたはファイル名。戻り値は再生ハンドル（失敗時は0） |
| `bool StopAudio(uint playHandle)` | 再生を停止する |
| `bool IsAudioPlaying(uint playHandle)` | 再生中かどうかを取得する |

※オブジェクトに紐づいた再生（3D空間音響など）は `AudioSource` コンポーネントを使用してください。

### モデル

| 関数 | 説明 |
|---|---|
| `uint GetModelHandleFromAssetPath(const string &in path)` | アセットパス（例: `"PrimitiveMesh-Box"` 等）からモデルハンドルを取得する。`MeshFilter::SetMeshHandle` へそのまま渡せる |

### 実行コンテキスト

現在実行中のスクリプトのオーナーオブジェクト・シーンへアクセスします。

| 関数 | 説明 |
|---|---|
| `Object@ GetOwnerObject()` | このスクリプトが付いているオブジェクトを取得する |
| `Transform@ GetTransform()` | オーナーオブジェクトのTransformを取得する（無い場合は `null`） |
| `Scene@ GetScene()` | 現在のシーンを取得する |
| `Object@ FindObject(const string &in name)` | シーン内から名前が一致する最初のオブジェクトを取得する（無い場合は `null`） |

## オブジェクト・シーン型

### Object（ゲームオブジェクト）

| メソッド | 説明 |
|---|---|
| `const string &GetName() const` | オブジェクト名を取得する |
| `void SetName(const string &in)` | オブジェクト名を設定する |
| `bool IsActive() const` | アクティブ状態を取得する |
| `void SetActive(bool)` | アクティブ状態を設定する |
| `void SetTag(const string &in)` | タグを設定する（[詳細](#tagタグ)） |
| `Tag GetTag() const` | タグを取得する（比較用） |
| `const string &GetTagName() const` | タグの文字列を取得する |
| `Transform@ GetTransform()` | Transformコンポーネントを取得する |
| `bool GetComponent(?&out)` | コンポーネントを取得する（[詳細](#コンポーネントの取得追加削除)） |
| `bool GetComponents(?&out)` | コンポーネントを全件取得する |
| `bool AddComponent(?&out)` | コンポーネントを新規追加する |
| `bool RemoveComponent(?&in)` | コンポーネントを削除する |

### Scene（シーン）

| メソッド | 説明 |
|---|---|
| `const string &GetName() const` | シーン名を取得する |
| `Object@ GetObject(const string &in name) const` | 名前が一致する最初のオブジェクトを取得する |
| `array<Object@>@ GetObjects(const string &in name) const` | 名前が一致する**全ての**オブジェクトを取得する（0件でも配列は返る） |
| `Object@ CreateObject(const string &in name = "")` | 空のオブジェクトを新規生成してシーンへ追加する |
| `Object@ CloneObject(Object@ source, const string &in name = "")` | 既存オブジェクトを複製してシーンへ追加する（`source` はこのシーンに属している必要がある。子オブジェクトや親子関係は複製されない） |
| `bool DeleteObject(Object@ obj)` | オブジェクトを削除する（子オブジェクトがあれば道連れに削除される） |
| `void SetNextSceneName(const string &in)` | 次のシーン名を設定する |
| `bool ChangeToNextScene()` | 次のシーンへ切り替える |
| `bool HasNextSceneName() const` | 次のシーン名が設定されているかを取得する |
| `void ClearNextSceneName()` | 次のシーン名をクリアする |
| `void RequestExitGameLoop()` | ゲームループの終了を要求する（[詳細](#ゲームループ制御)） |
| `bool SetVariable(const string &in key, ?&in value)` | シーン変数を設定する（[詳細](#シーン変数スクリプト間の値の受け渡し)） |
| `bool GetVariable(const string &in key, ?&out value)` | シーン変数を取得する |
| `bool HasVariable(const string &in key)` | シーン変数が存在するかどうか |
| `bool RemoveVariable(const string &in key)` | シーン変数を削除する |
| `bool SetGlobalVariable(const string &in key, ?&in value)` | グローバルシーン変数を設定する（シーンを跨いで保持される） |
| `bool GetGlobalVariable(const string &in key, ?&out value)` | グローバルシーン変数を取得する |
| `bool HasGlobalVariable(const string &in key)` | グローバルシーン変数が存在するかどうか |
| `bool RemoveGlobalVariable(const string &in key)` | グローバルシーン変数を削除する |

### Tag（タグ）

文字列からハッシュ値（FNV-1a 64bit）を計算して保持する軽量な値型です。比較（`==` / `!=`）はハッシュ値同士で行われるため、文字列比較より高速にオブジェクトやコンポーネントの分類・判別ができます。

| メンバ | 説明 |
|---|---|
| `Tag()` | 既定のタグを作成する（空文字列のタグと等しい） |
| `Tag(const string &in name)` | 文字列からタグを作成する |
| `uint64 GetHash() const` | 内部のハッシュ値を取得する |
| `bool IsEmpty() const` | 空文字列のタグ（未設定）かどうか |
| `==` / `!=` | ハッシュ値同士の比較 |

タグはエディターのインスペクター（オブジェクト・各コンポーネント・シーンコンポーネントの「Tag」欄）からも設定でき、シーンへ保存されます。スクリプト側では `SetTag` / `GetTag` / `GetTagName` で読み書きします。

```angelscript
class Player : ScriptComponentBehavior {
    // スクリプト側でTagを作成して比較に使う
    Tag enemyTag("Enemy");

    void Start() {
        // オブジェクトへのタグ設定（コンポーネントにも同様に設定できる）
        GetOwnerObject().SetTag("Player");
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        // タグでの判別（名前と違い、ハッシュ同士の高速な比較になる）
        if (hit.otherObject.GetTag() == enemyTag) {
            Log("敵と衝突: " + hit.otherObject.GetTagName());
        }
    }
}
```

- 同じ文字列からは常に同じハッシュ値が計算されるため、別々の場所で `Tag("Enemy")` と作成したもの同士も等しくなります。
- タグ未設定（空文字列）の状態は `IsEmpty()` で判定できます。`Tag()`（既定値）とも等しくなります。

## シーン変数（スクリプト間の値の受け渡し）

`ScriptComponent` は1つにつき独立したスクリプトモジュールとしてコンパイルされるため、あるスクリプトのグローバル変数やクラスのメンバー変数に、別のスクリプトから同じ型として直接アクセスすることはできません。スクリプト間で値をやり取りする方法は主に2つあります。

- **シーン変数**（このセクション）: `Scene` が持つキー・バリューの共有領域を経由する方法。相手のオブジェクトを意識せず、緩く値をやり取りしたい場合に向いています
- **[ScriptComponent経由の直接アクセス](#scriptcomponent経由のスクリプト間データ受け渡し)**: 相手オブジェクトの `ScriptComponent` を取得し、`[SerializeField]` 変数を名前で直接読み書きする方法。相手のオブジェクトが分かっている場合に向いています

### シーン変数

```angelscript
// scriptA.as（敵を倒した側）
GetScene().SetVariable("lastKilledEnemyName", GetOwnerObject().GetName());
GetScene().SetGlobalVariable("score", currentScore + 100); // シーン遷移後も保持したい値

// scriptB.as（UI表示側、別オブジェクト）
string enemyName;
if (GetScene().GetVariable("lastKilledEnemyName", enemyName)) {
    Log("倒した敵: " + enemyName);
}
int score;
GetScene().GetGlobalVariable("score", score);
```

- **シーン変数**（`SetVariable`/`GetVariable`/`HasVariable`/`RemoveVariable`）は、そのシーンが読み込まれている間だけ有効です。シーンが切り替わると失われます。
- **グローバルシーン変数**（`SetGlobalVariable`/`GetGlobalVariable`/`HasGlobalVariable`/`RemoveGlobalVariable`）は `SceneManager` が保持するため、シーンを切り替えても値が残ります。スコアやフラグなどシーンを跨いで引き継ぎたい値に使用してください。
- `SetVariable`/`SetGlobalVariable` は同じキーへ再度呼び出すと値を上書きします（型が変わっても構いません）。
- `GetVariable`/`GetGlobalVariable` は、キーが存在しない場合、または既存の値と渡した変数の型が異なる場合に `false` を返します（`value` は変更されません）。
- 対応している型は `[SerializeField]` と同じです: `bool` / `int` / `uint` / `float` / `double` / `string` / `Vector2` / `Vector3` / `Vector4` / `Quaternion`。

### ScriptComponent経由のスクリプト間データ受け渡し

相手のオブジェクトが分かっている場合は、シーン変数を経由せずに相手オブジェクトの `ScriptComponent` を取得して、`[SerializeField]` 変数を名前で直接取得・設定できます。

```angelscript
// scriptA.as（相手を直接操作する側）
Object@ enemy = GetScene().GetObject("Enemy");
if (enemy !is null) {
    ScriptComponent@ sc;
    if (enemy.GetComponent(@sc)) {
        float hp;
        if (sc.GetVariable("hp", hp)) {
            Log("敵のHP: " + hp);
            sc.SetVariable("hp", hp - 10.0f);
        }
    }
}
```

```angelscript
// scriptB.as（Enemyオブジェクト側。hpはSerializeFieldが付いているので外部から読み書きできる）
class Enemy : ScriptComponentBehavior {
    [SerializeField]
    float hp = 100.0f;
}
```

| メソッド | 説明 |
|---|---|
| `bool GetVariable(const string &in name, ?&out) const` | 指定名の `[SerializeField]` 変数の現在値を取得する |
| `bool SetVariable(const string &in name, ?&in)` | 指定名の `[SerializeField]` 変数へ値を設定する |

- 取得・設定できるのは相手スクリプトの **`[SerializeField]` が付いた変数のみ**です（インスペクターに公開されている＝外部から触ってよいと明示された変数、という既存の意味をそのまま利用しています）
- 対応している型はシーン変数と同じプリミティブ/数学型に加えて `Object@`（[詳細](#object参照シーン内オブジェクトの指定)）です。`array<T>` や `[System.Serializable]` クラスは対象外です（スクリプトごとに独立したモジュールとしてコンパイルされるため、同じ名前・同じ形のクラスでも型が一致せず安全に受け渡せないため）
- 変数名が存在しない、または渡した変数と型が一致しない場合は `false` を返すだけで、例外にはなりません（値は変更されません）

## dictionary（辞書型）

AngelScript標準アドオンの辞書型です。文字列のキーに任意の型の値を関連付けて保持します。

```angelscript
dictionary dict;
dict.set("hp", 100);
dict.set("name", "Player");
dict.set("position", Vector3(1.0f, 2.0f, 3.0f));

// 取得（キーが無い、または型が合わない場合はfalse）
int hp;
if (dict.get("hp", hp)) {
    Log("HP: " + hp);
}

// キーの存在確認・削除
if (dict.exists("name")) {
    dict.delete("name");
}

// キー一覧の列挙
array<string>@ keys = dict.getKeys();
for (uint i = 0; i < keys.length(); i++) {
    Log(keys[i]);
}

// 初期化リスト構文・インデックスアクセスも使用できる
dictionary levels = {{"stage1", 10}, {"stage2", 25}};
int64 score = int64(levels["stage1"]);
```

| メソッド | 説明 |
|---|---|
| `void set(const string &in key, const ?&in value)` | 値を設定する（既存キーは上書き。型が変わっても構わない） |
| `bool get(const string &in key, ?&out value) const` | 値を取得する（キーが無い/型が合わない場合は `false`） |
| `bool exists(const string &in key) const` | キーが存在するかどうか |
| `bool delete(const string &in key)` | キーを削除する |
| `void deleteAll()` | 全てのキーを削除する |
| `bool isEmpty() const` | 空かどうか |
| `uint getSize() const` | 要素数を取得する |
| `array<string>@ getKeys() const` | 全キーの配列を取得する |

- 値としてプリミティブ型・数学型・文字列のほか、ハンドル（`array@` や `dictionary@` 自身など）も保持できるため、ネストしたデータ構造も作れます。
- `foreach (auto value, auto key : dict)` によるループにも対応しています。
- 辞書の内容は `[SerializeField]` やシーン変数の対象外です。永続化したい場合は [Json](#jsonjsonファイルの保存読み込み) を使用してください。`Json.Set("key", dict)` / `Json.Get("key", dict)` で辞書ごとJSONへ変換してファイルへ保存・復元できます（[詳細](#配列辞書との相互変換汎用setgetpush)）。

## Json（JSONファイルの保存・読み込み）

`Json` 型（参照型）を使って、スクリプトからjsonファイルの保存・読み込みやJSONテキストの生成・解析ができます。セーブデータやゲーム設定・レベルデータの入出力に使用できます。

```angelscript
// 保存
Json@ json = Json();
json.SetString("name", "Player");
json.SetInt("hp", 100);
json.SetVector3("position", Vector3(1.0f, 2.0f, 3.0f));

Json@ inventory = Json();
inventory.PushString("sword");
inventory.PushString("shield");
json.SetJson("inventory", inventory);   // ネストしたオブジェクト・配列

if (SaveJsonFile("SaveData/save1.json", json)) {
    Log("保存しました");
}

// 読み込み
Json@ loaded = LoadJsonFile("SaveData/save1.json");
if (loaded !is null) {
    string name = loaded.GetString("name");
    int64 hp = loaded.GetInt("hp", 100);            // 第2引数はキーが無い場合のデフォルト値
    Vector3 pos = loaded.GetVector3("position");

    Json@ items = loaded.GetJson("inventory");
    if (items !is null) {
        for (uint i = 0; i < items.Size(); i++) {
            Log("item: " + items.At(i).AsString());
        }
    }
}
```

### グローバル関数

| 関数 | 説明 |
|---|---|
| `Json@ LoadJsonFile(const string &in path)` | jsonファイルを読み込む（失敗時は `null`） |
| `bool SaveJsonFile(const string &in path, const Json &in data, int indent = 4)` | jsonファイルへ保存する（保存先フォルダが無い場合は自動生成される） |

パスは実行ディレクトリ（`Assets` フォルダと同じ階層）基準の相対パス、または絶対パスで指定します。

### Jsonのメンバ

| メンバ | 説明 |
|---|---|
| `Json()` | 空のJson値を作成する（null状態。キー設定でオブジェクトに、Pushで配列になる） |
| `bool IsNull() / IsObject() / IsArray() / IsString() / IsNumber() / IsBool() const` | 保持している値の型判定 |
| `bool Has(const string &in key) const` | キーが存在するかどうか |
| `bool Remove(const string &in key)` | キーを削除する |
| `void Clear()` | 内容を破棄してnull状態に戻す |
| `array<string>@ GetKeys() const` | オブジェクトの全キーを取得する |
| `void SetBool/SetInt/SetFloat/SetString(const string &in key, 値)` | キーへ値を設定する（`bool` / `int64` / `double` / `string`） |
| `void SetVector2/SetVector3/SetVector4/SetQuaternion(const string &in key, 値)` | キーへ数学型を設定する（`{"x": ..., "y": ...}` 形式で保存される） |
| `void SetJson(const string &in key, const Json &in value)` | キーへ別のJson（オブジェクト・配列）を設定する |
| `void SetNull(const string &in key)` | キーへnullを設定する |
| `bool Set(const string &in key, const ?&in value)` | キーへ任意の対応型を設定する（`array<T>` / `dictionary` を含む。[詳細](#配列辞書との相互変換汎用setgetpush)） |
| `bool Get(const string &in key, ?&out value) const` | キーの値を任意の対応型へ読み込む（`array<T>` / `dictionary` を含む） |
| `bool GetBool(key, bool defaultValue = false) const` | 値を取得する（キーが無い/型が合わない場合はデフォルト値） |
| `int64 GetInt(key, int64 defaultValue = 0) const` | 同上（整数） |
| `double GetFloat(key, double defaultValue = 0) const` | 同上（実数） |
| `string GetString(key, const string &in defaultValue = "") const` | 同上（文字列） |
| `Vector2/Vector3/Vector4/Quaternion GetVector2/GetVector3/GetVector4/GetQuaternion(key, デフォルト値)` | 同上（数学型） |
| `Json@ GetJson(const string &in key) const` | ネストしたJsonを取得する（キーが無い場合は `null`） |
| `uint Size() const` | 配列・オブジェクトの要素数を取得する |
| `Json@ At(uint index) const` | 配列の要素を取得する（範囲外・配列以外は `null`） |
| `void PushBool/PushInt/PushFloat/PushString(値)` | 配列へ値を追加する |
| `void PushJson(const Json &in value)` | 配列へ別のJsonを追加する |
| `bool Push(const ?&in value)` | 配列へ任意の対応型を追加する（`array<T>` / `dictionary` を含む） |
| `bool AsBool/AsInt/AsFloat/AsString(デフォルト値) const` | 自身が保持する値を直接取得する（`At` で取り出した配列要素向け） |
| `string ToString(int indent = -1) const` | JSONテキストへ変換する（`indent >= 0` で整形出力） |
| `bool Parse(const string &in text)` | JSONテキストを解析して内容を置き換える（失敗時は `false` でnull状態になる） |

- `GetJson` / `At` は**部分木のコピー**を持つ新しいインスタンスを返します。取得した子Jsonへの変更は親へ反映されないため、変更後は `SetJson` / `PushJson` で書き戻してください。
- 数値はJSONの仕様どおり整数・実数の区別なく保存されます。`GetInt` / `GetFloat` はどちらの数値でも取得できます。

### 配列・辞書との相互変換（汎用Set/Get/Push）

`Set` / `Get` / `Push` は渡した変数の型に応じて自動変換する汎用版です。`array<T>` はJSON配列、`dictionary` はJSONオブジェクトとして再帰的に変換されるため、まとまったデータをそのまま保存・復元できます。

```angelscript
Json@ json = Json();

// array<T> → JSON配列（ネスト配列もOK）
array<int> scores = {10, 20, 30};
array<Vector3> waypoints = {Vector3(0,0,0), Vector3(1,0,2)};
json.Set("scores", scores);
json.Set("waypoints", waypoints);

// dictionary → JSONオブジェクト（ネストした辞書・配列も変換される）
dictionary save;
save.set("hp", 100);
save.set("name", "Player");
json.Set("data", save);

SaveJsonFile("SaveData/save1.json", json);

// 読み込み（逆方向の変換）
Json@ loaded = LoadJsonFile("SaveData/save1.json");
if (loaded !is null) {
    array<int> loadedScores;
    loaded.Get("scores", loadedScores);      // JSON配列 → array<int>

    dictionary loadedData;
    loaded.Get("data", loadedData);          // JSONオブジェクト → dictionary

    int64 hp;
    loadedData.get("hp", hp);
}
```

対応型: プリミティブ全種（`int8`〜`uint64` 含む） / `string` / `Vector2`〜`Vector4` / `Quaternion` / `Json` / それらの `array<T>`（ネスト可） / `dictionary`。

- `Get` へ `array<T>@` や `dictionary@` の**nullハンドル**を渡した場合は自動で生成されます。
- 保存時、`dictionary` 内の変換できない値（コンポーネントハンドル等）は**スキップ**されます。`array<T>` は要素型が未対応の場合、全体が失敗して `false` が返ります。
- `dictionary` への読み込み時、数値は `int64` / `double` として格納されます。ネストしたJSONオブジェクトは `dictionary@`、JSON配列は要素型を推定した `array<T>@`（全要素が整数なら `array<int64>` 等）として復元されます。型が混在する配列・空配列は推定できないためスキップされます。
- 数学型（`{"x": ..., "y": ...}` 形式）として保存した値を `dictionary` へ読み込むと、判別できないためネスト辞書として復元されます。数学型として取り出したい場合は `GetVector3` 等を使用してください。
- 自己参照などによる無限再帰を防ぐため、変換は16段までに制限されています。

## コンポーネント型

全てのコンポーネント型は参照型（ハンドル `T@` で保持）です。以下の共通メソッドを持ちます。

| 共通メソッド | 説明 |
|---|---|
| `bool IsActive() const` | アクティブ状態を取得する |
| `void SetActive(bool)` | アクティブ状態を設定する |
| `const string &GetComponentType() const` | コンポーネントの種類名を取得する |
| `void SetTag(const string &in)` | タグを設定する（[詳細](#tagタグ)） |
| `Tag GetTag() const` | タグを取得する（比較用） |
| `const string &GetTagName() const` | タグの文字列を取得する |

### Transform

| メソッド | 説明 |
|---|---|
| `void SetTranslate(const Vector3 &in)` / `const Vector3 &GetTranslate() const` | 座標の設定/取得 |
| `void SetRotate(const Vector3 &in)` / `const Vector3 &GetRotate() const` | 回転の設定/取得（オイラー角・ラジアン） |
| `void SetRotateQuaternion(const Quaternion &in)` / `const Quaternion &GetRotateQuaternion() const` | 回転の設定/取得（クォータニオン） |
| `void SetScale(const Vector3 &in)` / `const Vector3 &GetScale() const` | スケールの設定/取得 |
| `const Matrix4x4 &GetWorldMatrix()` | ワールド行列を取得する（必要なら再計算される） |
| `Vector3 GetWorldPosition()` | 親を合成したワールド座標を取得する |
| `Vector3 GetWorldRotate() const` | 親を合成したワールド回転を取得する（オイラー角・ラジアン） |
| `Quaternion GetWorldRotateQuaternion() const` | 親を合成したワールド回転を取得する（クォータニオン） |
| `Vector3 GetWorldScale()` | 親を合成したワールドスケールを取得する |

### Velocity

| メソッド | 説明 |
|---|---|
| `void SetVelocity(const Vector3 &in)` / `const Vector3 &GetVelocity() const` | 速度の設定/取得（単位/秒） |
| `void SetAcceleration(const Vector3 &in)` / `const Vector3 &GetAcceleration() const` | 加速度の設定/取得（単位/秒²） |
| `void AddVelocity(const Vector3 &in)` | 速度に加算する |

### Rotation

`Velocity` の回転版。毎フレーム、角加速度を角速度へ、角速度を `Transform` の回転（オイラー角・ラジアン）へ自動的に適用する。

| メソッド | 説明 |
|---|---|
| `void SetAngularVelocity(const Vector3 &in)` / `const Vector3 &GetAngularVelocity() const` | 角速度の設定/取得（ラジアン/秒） |
| `void SetAngularAcceleration(const Vector3 &in)` / `const Vector3 &GetAngularAcceleration() const` | 角加速度の設定/取得（ラジアン/秒²） |
| `void AddAngularVelocity(const Vector3 &in)` | 角速度に加算する |

### PreTransform

1フレーム前のTransform情報を保持するコンポーネント。オブジェクトに追加すると、シーン側で毎フレーム最後（全オブジェクト・全シーンコンポーネントの更新後）に自動で値が更新される。ローカル/ワールドそれぞれの位置・回転・スケールを取得できるため、補間や速度計算、モーションブラー等に使用できる。

| メソッド | 説明 |
|---|---|
| `const Vector3 &GetPreviousTranslate() const` | 前フレームのローカル座標を取得する |
| `const Vector3 &GetPreviousRotate() const` | 前フレームのローカル回転を取得する（オイラー角・ラジアン） |
| `const Quaternion &GetPreviousRotateQuaternion() const` | 前フレームのローカル回転を取得する（クォータニオン） |
| `const Vector3 &GetPreviousScale() const` | 前フレームのローカルスケールを取得する |
| `const Matrix4x4 &GetPreviousWorldMatrix() const` | 前フレームのワールド行列を取得する（モーションブラー等のGPU用途） |
| `const Vector3 &GetPreviousWorldPosition() const` | 前フレームのワールド座標を取得する |
| `const Vector3 &GetPreviousWorldRotate() const` | 前フレームのワールド回転を取得する（オイラー角・ラジアン） |
| `const Quaternion &GetPreviousWorldRotateQuaternion() const` | 前フレームのワールド回転を取得する（クォータニオン） |
| `const Vector3 &GetPreviousWorldScale() const` | 前フレームのワールドスケールを取得する |

### ParticleSystem2D / ParticleSystem3D

指定した生成方法（自身の子/他オブジェクトの子/自身のワールド座標/指定座標のいずれか）で、一定間隔でパーティクル用オブジェクトを生成するコンポーネント。生成した各パーティクルには`Velocity`（初速・加速度）、`Rotation`（初期回転速度・回転加速度。度数法で指定した値はラジアンへ変換して適用される）と描画コンポーネント（2Dは`MeshFilter`+`SpriteRenderer`、3Dは`MeshFilter`+`MeshRenderer`）が自動で追加され、寿命に応じてスケールが変化しながら消える。寿命・初速・加速度・開始/終了スケール・初期回転・回転速度・回転加速度・一回のスポーン数は、パラメータごとに「固定値」か「Min〜Maxのランダム範囲」かを選べる。Loopを無効にして再生した場合、Total Spawn Countで指定した数だけスポーンすると自動的に停止する。スポーン範囲の形状（Point/Box/Sphere/Capsule、2DはRect/Circle）を含め、詳細なパラメータはインスペクターから設定する。

| メソッド | 説明 |
|---|---|
| `void Play()` / `void Stop()` | パーティクルの生成を開始/停止する（生成済みのパーティクルはStopしても寿命まで残る。停止中にPlayを呼ぶと総発生数カウントがリセットされる） |
| `bool IsPlaying() const` | 生成中かどうか |
| `void Clear()` | 生存中の全パーティクルを即座に削除する |
| `void SetEmissionRate(float)` / `float GetEmissionRate() const` | 1秒あたりの生成数の設定/取得 |
| `void SetMaxParticles(int)` / `int GetMaxParticles() const` | 同時に存在できる最大パーティクル数の設定/取得 |
| `void SetBillboard(bool)` / `bool IsBillboard() const` | ビルボード化（常に指定オブジェクトの方を向く）の有効/無効の設定/取得 |
| `void SetBillboardTarget(Object@)` / `Object@ GetBillboardTarget() const` | ビルボードの向き先オブジェクトの設定/取得（未設定の場合はシーン内のカメラを自動で使う） |
| `void SetBillboardRotationMode(TargetLookAtMode)` / `TargetLookAtMode GetBillboardRotationMode() const` | ビルボードの向き方の設定/取得（`TargetLookAtMode::SyncRotation` / `TargetLookAtMode::LookAt`。[詳細](#targetlookat)） |

### AudioSource

| メソッド | 説明 |
|---|---|
| `uint Play()` | 再生する（再生中なら止めて再生し直す）。戻り値は再生ハンドル |
| `void Stop()` / `bool Pause()` / `bool Resume()` | 停止 / 一時停止 / 再開 |
| `bool IsPlaying() const` / `bool IsPaused() const` | 再生中 / 一時停止中かどうか |
| `void SetSoundName(const string &in)` / `const string &GetSoundName() const` | 使用する音声の設定/取得 |
| `void SetVolume(float)` / `float GetVolume() const` | ボリューム（0.0～1.0） |
| `void SetPitch(float)` / `float GetPitch() const` | ピッチ（半音単位） |
| `void SetLoop(bool)` / `bool GetLoop() const` | ループ再生 |

### AudioListener

| メソッド | 説明 |
|---|---|
| `void SetUsed(bool)` / `bool GetUsed() const` | 使用中フラグ（シーン内で使用中にできるのは1つだけ） |
| `Vector3 GetWorldPosition() const` | ワールド座標を取得する |

### Camera3D

| メソッド | 説明 |
|---|---|
| `void SetFovY(float)` / `float GetFovY() const` | 画角Y |
| `void SetNearClip(float)` / `float GetNearClip() const` | 近クリップ距離 |
| `void SetFarClip(float)` / `float GetFarClip() const` | 遠クリップ距離 |
| `void SetAspectRatio(float)` / `float GetAspectRatio() const` | アスペクト比 |
| `void SetOrthographic(bool)` / `bool IsOrthographic() const` | 平行投影かどうか |
| `void SetOrthoSize(float)` / `float GetOrthoSize() const` | 平行投影サイズ |

### SpriteRenderer

| メソッド | 説明 |
|---|---|
| `void SetAnchor(const Vector2 &in)` / `const Vector2 &GetAnchor() const` | アンカー |
| `void SetPivot(const Vector2 &in)` / `const Vector2 &GetPivot() const` | ピボット |
| `void SetPipelineName(const string &in)` | 使用パイプライン名の設定 |
| `void SetMaterialName(const string &in)` | 使用マテリアル名の設定 |

### ScriptComponent

| メソッド | 説明 |
|---|---|
| `void SetScriptPath(const string &in)` / `const string &GetScriptPath() const` | スクリプトパスの設定/取得 |
| `bool Reload()` | スクリプトを再コンパイルする |
| `bool GetVariable(const string &in, ?&out) const` | 指定名の `[SerializeField]` 変数の現在値を取得する（[詳細](#scriptcomponent経由のスクリプト間データ受け渡し)） |
| `bool SetVariable(const string &in, ?&in)` | 指定名の `[SerializeField]` 変数へ値を設定する（[詳細](#scriptcomponent経由のスクリプト間データ受け渡し)） |

### MeshFilter

| メソッド | 説明 |
|---|---|
| `void SetMeshHandle(uint)` / `uint GetMeshHandle() const` | 使用するメッシュのハンドルの設定/取得 |
| `bool HasMesh() const` | メッシュが設定されているかどうか |

### Animator

| メソッド | 説明 |
|---|---|
| `void SetAnimationName(const string &in)` / `const string &GetAnimationName() const` | 再生するアニメーション名の設定/取得 |
| `void SetPlayOnStart(bool)` / `bool GetPlayOnStart() const` | 開始時に自動再生するかどうか |

### KeyFrameAnimator

キーフレームアニメーション（json）のfloat値を、同オブジェクトの他コンポーネントのパラメータへ毎フレーム適用するコンポーネントです。

- アニメーションのjsonはエディターツールの **Keyframe Animation Editor**（メニューバーの Tools > Keyframe Animation Editor）で作成し、インスペクターの各アニメーションエントリの `Json Path` へ指定します。
- 複数のアニメーションを登録でき、それぞれに再生速度・再生時オフセット（評価時刻へ加算・秒）・スケール（評価値にかける倍率）・デフォルト値オフセット（評価値へ加算）・ループ・自動再生を設定できます。適用値は `評価値 × Value Scale + Value Offset` です。
- 適用先（Bindings・複数指定可）はインスペクターのコンボから選択します。候補は同オブジェクトの各コンポーネントのfloat系パラメータ（Vector型は成分単位）と、`ScriptComponent` の `[SerializeField]` 付き `float` 変数です。ImGuiのインスペクターで編集できるパラメータは全コンポーネントで候補として登録されています。
- アニメーションはエントリの `Name` で識別します。スクリプトからは以下のメソッドで制御できます。

| メソッド | 説明 |
|---|---|
| `bool Play(const string &in name)` | 指定した名前のアニメーションを先頭から再生する |
| `bool Stop(const string &in name)` | 指定した名前のアニメーションを停止する（再生位置は保持） |
| `void PlayAll()` / `void StopAll()` | 全アニメーションの再生/停止 |
| `bool IsPlaying(const string &in name) const` | 再生中かどうか |
| `bool SetPlaybackSpeed(const string &in name, float speed)` / `float GetPlaybackSpeed(const string &in name) const` | 再生速度倍率（負の値で逆再生） |
| `bool TryGetValue(const string &in name, float &out value) const` | 最後に評価された値（値オフセット適用後）を取得する |
| `uint GetAnimationCount() const` | 登録されているアニメーション数 |

```angelscript
KeyFrameAnimator@ animator;
if (GetComponent(@animator)) {
    animator.Play("DoorOpen");
    float value;
    if (animator.TryGetValue("DoorOpen", value)) {
        // 再生中の評価値を独自の処理にも利用できる
    }
}
```

### InputCommandApplier

InputCommandの入力を、同オブジェクトの他コンポーネントのパラメータへ適用するコンポーネントです。1つのオブジェクトへ複数アタッチでき、コマンド・条件ごとに別々の適用が行えます。

- 毎フレーム `Command Name` の入力コマンドを評価し、**条件を満たしたフレームだけ**値をバインド先へ書き込みます。
- 条件（Condition）: コマンドがtrue / コマンドがfalse / 入力値が閾値以上 / 入力値が閾値以下 / 入力値が閾値と等しい（誤差1e-6以内）
- 書き込む値（Value Source）: 入力コマンドの評価値 or 固定値。適用値は `値 × Value Scale + Value Offset` です。
- 適用先（Bindings・複数指定可）は `KeyFrameAnimator` と同じ仕組みで、同オブジェクトのコンポーネントのfloat系パラメータまたは `ScriptComponent` の `[SerializeField]` 付き `float` 変数をコンボから選択します。

| メソッド | 説明 |
|---|---|
| `void SetCommandName(const string &in)` / `const string &GetCommandName() const` | 評価する入力コマンド名 |
| `void SetThreshold(float)` / `float GetThreshold() const` | 入力値と比較する閾値 |
| `void SetFixedValue(float)` / `float GetFixedValue() const` | 固定値モードで書き込む値 |
| `void SetValueScale(float)` / `float GetValueScale() const` | 書き込む値にかけるスケール |
| `void SetValueOffset(float)` / `float GetValueOffset() const` | 書き込む値へ加算するオフセット |
| `bool WasApplied() const` | 前回のUpdateで条件を満たして値を適用したか |
| `float GetLastValue() const` | 前回適用した値（Scale/Offset適用後） |

### Text

| メソッド | 説明 |
|---|---|
| `void SetText(const string &in)` / `const string &GetText() const` | 表示文字列の設定/取得 |
| `void SetColor(const Vector4 &in)` / `const Vector4 &GetColor() const` | 色の設定/取得 |

### ComputeShaderProcessing

| メソッド | 説明 |
|---|---|
| `void SetPipelineName(const string &in)` / `const string &GetPipelineName() const` | 使用するComputeパイプライン名の設定/取得 |
| `void SetGroupCounts(uint, uint, uint)` | ディスパッチするスレッドグループ数(x, y, z)を設定する |
| `void GetGroupCounts(uint &out, uint &out, uint &out) const` | スレッドグループ数(x, y, z)を取得する |

### RigidBody2D / RigidBody3D

| メソッド | RigidBody2D | RigidBody3D | 説明 |
|---|:-:|:-:|---|
| `void SetMass(float)` / `float GetMass() const` | o | o | 質量 |
| `void SetUseGravity(bool)` / `bool IsGravityEnabled() const` | o | o | 重力の有効/無効 |
| `void SetVelocity(const Vector2 &in)` / `const Vector2 &GetVelocity() const` | o | - | 速度 |
| `void SetBodyType(int)` / `int GetBodyType() const` | - | o | 物理ボディ種別（0:Static 1:Kinematic 2:Dynamic） |
| `void SetInterpolate(bool)` / `bool IsInterpolateEnabled() const` | - | o | 補間の有効/無効 |
| `void SyncFromTransform()` | - | o | 現在のTransformの位置・回転を物理ボディへ反映する（Play開始時の同期用） |

### MeshRenderer / SkinnedMeshRenderer

| メソッド | MeshRenderer | SkinnedMeshRenderer | 説明 |
|---|:-:|:-:|---|
| `void SetPipelineName(const string &in)` / `const string &GetPipelineName() const` | o | o | 使用パイプライン名 |
| `void SetMaterialName(const string &in)` / `const string &GetMaterialName() const` | o | o | 使用マテリアル名 |
| `void SetTargetObject(Object@)` / `Object@ GetTargetObject() const` | o | - | 描画先オブジェクト |
| `void SetAnimationClipName(const string &in)` / `const string &GetAnimationClipName() const` | - | o | 再生するアニメーションクリップ名 |
| `void SetPlayOnStart(bool)` / `bool GetPlayOnStart() const` | - | o | 開始時に自動再生するかどうか |
| `void SetLoop(bool)` / `bool GetLoop() const` | - | o | ループ再生 |
| `void SetPlaybackSpeed(float)` / `float GetPlaybackSpeed() const` | - | o | 再生速度倍率 |
| `void Play()` / `void Stop()` / `bool IsPlaying() const` | - | o | 再生制御 |
| `void SetBlendShapeWeight(const string &in, float)` / `float GetBlendShapeWeight(const string &in) const` | - | o | BlendShapeウェイト（0～100）の設定/取得 |

### Camera2D

| メソッド | 説明 |
|---|---|
| `void SetSize(float width, float height)` | 表示範囲サイズを設定する |
| `void SetNearClip(float)` / `float GetNearClip() const` | 近クリップ距離 |
| `void SetFarClip(float)` / `float GetFarClip() const` | 遠クリップ距離 |
| `float GetWidth() const` / `float GetHeight() const` | 表示範囲サイズを取得する |

### CameraRenderer

| メソッド | 説明 |
|---|---|
| `void SetPipelineName(const string &in)` / `const string &GetPipelineName() const` | 使用パイプライン名 |
| `Vector3 GetWorldPosition() const` | カメラのワールド座標を取得する |
| `const Matrix4x4 &GetViewProjectionMatrix() const` | 直近アップロードしたビュー射影行列を取得する |
| `float GetNearClip() const` / `float GetFarClip() const` | 直近使用したニア/ファークリップ距離を取得する |

### CameraController

同一オブジェクトの `Camera3D` を、複数の追従先オブジェクトへ滑らかに追従させるコンポーネント。

| メソッド | 説明 |
|---|---|
| `bool IsControllable() const` | 同オブジェクトに `Camera3D` があるかどうか |
| `void AddFollowTarget(Object@)` | 追従先オブジェクトを追加する |
| `void RemoveFollowTarget(uint index)` | 追従先オブジェクトをインデックス指定で削除する |
| `void SetPositionOffset(const Vector3 &in)` / `const Vector3 &GetPositionOffset() const` | 位置オフセット |
| `void SetRotationOffset(const Vector3 &in)` / `const Vector3 &GetRotationOffset() const` | 回転オフセット（オイラー角、ラジアン） |
| `void SetTargetFovY(float)` / `float GetTargetFovY() const` | 目標画角Y |
| `void SetMoveStrength(float)` / `float GetMoveStrength() const` | 移動追従の強さ（0.0～1.0） |
| `void SetRotateStrength(float)` / `float GetRotateStrength() const` | 回転追従の強さ（0.0～1.0） |
| `void SetFovLerpFactor(float)` / `float GetFovLerpFactor() const` | 画角遷移の強さ（0.0～1.0） |

### TargetLookAt

指定オブジェクトへ向き続ける（ビルボードのような動作をする）コンポーネント。回転の決め方は `TargetLookAtMode` 列挙型で2種類から選択できます。

- `TargetLookAtMode::SyncRotation` — ターゲットの**ワールド回転と同期**する。カメラをターゲットにすると常に画面と正対する**ビルボード**になる
- `TargetLookAtMode::LookAt` — 自身の**+Z軸が常にターゲットの方向を向く**よう回転する

どちらのモードでも回転オフセット（オイラー角、ラジアン）を追加で適用できます。ターゲットが未指定・存在しない場合は回転を変更しません。インスペクターではターゲットをヒエラルキーからのD&Dでも指定できます。

| メソッド | 説明 |
|---|---|
| `void SetTargetObject(Object@)` | ターゲットオブジェクトを設定する |
| `Object@ GetTargetObject() const` | ターゲットオブジェクトを取得する（無い場合は `null`） |
| `void SetRotationOffset(const Vector3 &in)` / `const Vector3 &GetRotationOffset() const` | 回転オフセット（オイラー角、ラジアン） |
| `void SetRotationMode(TargetLookAtMode)` / `TargetLookAtMode GetRotationMode() const` | 回転モード |

```angelscript
class Billboard : ScriptComponentBehavior {
    void Start() {
        TargetLookAt@ lookAt;
        if (AddComponent(@lookAt)) {
            lookAt.SetTargetObject(FindObject("MainCamera"));
            lookAt.SetRotationMode(TargetLookAtMode::SyncRotation); // カメラと正対し続ける
        }
    }
}
```

### Light / LightRenderer

`LightType` 列挙型（`Directional` / `Point` / `Spot` / `Rect` / `Sphere` / `Disc` / `Tube`）が使用できます。

`Rect`/`Sphere`/`Disc`/`Tube` は面光源（形状を持つ光源）です。真のLTC（Linearly Transformed Cosines）等の専用ルックアップテーブルは使わず、**形状上の代表点（representative point）法**（Karis "Real Shading in Unreal Engine 4" のsphere/tube近似をdisc/rectへ拡張したもの）で拡散・鏡面を近似します。拡散照明の減衰式自体はPoint/Spotと同じものを流用しており、面としての形状は主に鏡面ハイライトの広がりと影の柔らかさに表れます。

- `Sphere`/`Tube`: 全方向に発光（Pointと同様、`Radius`が減衰範囲）。影はPointと同じキューブ6面（Tubeは中心点からの近似）
- `Disc`/`Rect`: Transformの **+Z方向を法線とした片面発光**（Spotと同様、`Distance`が減衰範囲。コーン角の概念は無い）。影はSpotと同じ広角の単一透視投影で近似する
- `Rect`は+X方向が幅、+Y方向が高さ

| メソッド | Light | LightRenderer | 説明 |
|---|:-:|:-:|---|
| `void SetPipelineName(const string &in)` / `const string &GetPipelineName() const` | - | o | 使用パイプライン名 |
| `void SetType(LightType)` / `LightType GetType() const` | o | - | ライト種別 |
| `LightType GetLightType() const` | - | o | 同オブジェクトの `Light` の種別を取得する |
| `Light@ GetLight() const` | - | o | 同オブジェクトの `Light` コンポーネントを取得する |
| `void SetColor(const Vector4 &in)` / `const Vector4 &GetColor() const` | o | - | 色 |
| `void SetIntensity(float)` / `float GetIntensity() const` | o | - | 強度 |
| `void SetRadius(float)` / `float GetRadius() const` | o | - | 減衰範囲（Point/Sphere/Tube用） |
| `void SetDistance(float)` / `float GetDistance() const` | o | - | 減衰範囲（Spot/Disc/Rect用） |
| `void SetDecay(float)` / `float GetDecay() const` | o | - | 減衰指数（Point/Spot/Sphere/Disc/Rect/Tube共通） |
| `void SetInnerAngle(float)` / `float GetInnerAngle() const` | o | - | 内側角度（Spot用、ラジアン） |
| `void SetOuterAngle(float)` / `float GetOuterAngle() const` | o | - | 外側角度（Spot用、ラジアン） |
| `void SetSourceRadius(float)` / `float GetSourceRadius() const` | o | - | 面光源の物理的な半径（Sphere/Discの半径、Tubeの円柱半径） |
| `void SetSourceWidth(float)` / `float GetSourceWidth() const` | o | - | Rectの幅（+X方向） |
| `void SetSourceHeight(float)` / `float GetSourceHeight() const` | o | - | Rectの高さ（+Y方向） |
| `void SetSourceLength(float)` / `float GetSourceLength() const` | o | - | Tubeの長さ（+X方向） |
| `void SetShadowSoftness(float)` / `float GetShadowSoftness() const` | o | - | Directional/Point/Spotの半影ソフト化（PCSS）に使うワールド単位の光源サイズ（既定0=硬い影） |
| `float GetEffectiveShadowSoftness() const` | o | - | 実際にPCSSへ使われる光源サイズ。Sphere/Disc/Tubeは`SourceRadius`、Rectは`max(Width,Height)/2`が自動的に返る |
| `Vector3 GetWorldPosition() const` | - | o | ワールド座標を取得する |
| `Vector3 GetWorldDirection() const` | - | o | ワールド方向（+Z、発光面の法線）を取得する |
| `Vector3 GetWorldRight() const` | - | o | ワールド右方向（+X、Rectの幅・Tubeの軸方向）を取得する |
| `Vector3 GetWorldUp() const` | - | o | ワールド上方向（+Y、Rectの高さ方向）を取得する |

#### 半影のソフト化（PCSS）

`CastShadows`が有効なライトは、光源サイズに応じたソフトシャドウ（PCSS: Percentage-Closer Soft Shadows）が自動的にかかります。Sphere/Disc/Rect/Tubeは形状の物理サイズ（`SourceRadius`等）がそのまま光源サイズとして使われ、Directional/Point/Spotは既定で硬い影のままです（`SetShadowSoftness`で明示的にワールド単位の光源サイズを設定すると、そのライトにもソフトシャドウがかかります）。

### 描画先コンポーネント（NormalWindowObject / OverlayWindowObject / ScreenBufferObject / ShadowMapObject）

| メソッド | Window系(Normal/Overlay) | Buffer系(ScreenBuffer/ShadowMap) | 説明 |
|---|:-:|:-:|---|
| `void SetTitle(const string &in)` / `const string &GetTitle() const` | o | - | ウィンドウタイトルの設定/取得 |
| `void SetName(const string &in)` / `const string &GetName() const` | - | o | 管理用名前（TextureManagerへの登録名）の設定/取得 |
| `void SetSize(uint width, uint height)` | o | o | サイズの設定（バッファ系は既存バッファを実際にリサイズする） |

Window系コンポーネントは共通の基底型 `WindowObject` を持ち、ウィンドウが受信したメッセージをスクリプトの `OnWindowMessage` へ通知できます（[詳細](#ウィンドウメッセージonwindowmessage)）。また、メッセージ種別ごとにウィンドウの既定処理を中断してゲーム側の処理へ差し替える[メッセージの横取り](#メッセージの横取りインターセプト)が使用できます。

#### WindowObject（基底型）

`NormalWindowObject` / `OverlayWindowObject` の基底となる参照型です。`WindowMessageInfo` の `sourceComponent` として受け取れます。コンポーネント共通メソッド（`IsActive` / `GetComponentType` / `GetTag` 等）に加えて以下が使用できます。

| メソッド | 説明 |
|---|---|
| `void SetTitle(const string &in)` / `const string &GetTitle() const` | ウィンドウタイトル |
| `void SetSize(uint width, uint height)` | ウィンドウサイズを設定する |
| `int GetClientWidth() const` / `int GetClientHeight() const` | クライアント領域のサイズを取得する（ウィンドウ未生成時は0） |
| `bool IsWindowFocused() const` | ウィンドウにフォーカスがあるかどうか |
| `bool IsWindowMinimized() const` | ウィンドウが最小化されているかどうか |
| `bool SetMessageIntercepted(uint msg, bool enabled)` | メッセージを横取りするかを設定する（[詳細](#メッセージの横取りインターセプト)） |
| `bool IsMessageIntercepted(uint msg) const` | メッセージを横取りするかを取得する |
| `void CloseWindow()` | ウィンドウを閉じる（`WM_CLOSE` 横取り時にゲーム側の処理後に閉じるために使う） |

- `NormalWindowObject` / `OverlayWindowObject` のハンドルは `WindowObject@` へ**暗黙的に変換**できます。
- `cast<NormalWindowObject>(windowObject)` のように**具体的な型へダウンキャスト**できます（型が異なる場合は `null` が返ります）。

#### ウィンドウメッセージ（OnWindowMessage）

WindowObject系コンポーネントが付いているオブジェクトに `ScriptComponent` を追加すると、ウィンドウが受信したWindowsメッセージ（`WM_*`）を `OnWindowMessage(const WindowMessageInfo &in)` メソッドで受け取れます（[WindowMessageInfo](#windowmessageinfo)）。

- 通知は毎フレーム、そのフレームにウィンドウが受信したメッセージに対して行われます。**メッセージ種別ごとに最後の1件**が対象で、通知順は不定です。
- 同オブジェクトに複数のウィンドウコンポーネントがある場合は全てが通知対象です。どのウィンドウからの通知かは `sourceComponent` で判別できます。
- 通知はウィンドウの既定処理が実行された後の**事後通知**です。既定処理を実行させずゲーム側の処理へ差し替えたい場合は[メッセージの横取り](#メッセージの横取りインターセプト)を使用してください。
- 主要なメッセージは以下のグローバル定数（`const uint`）として登録されています。これら以外のメッセージも `message` の数値で判定できます。
  - ウィンドウ状態: `WM_ACTIVATE` / `WM_CLOSE` / `WM_DESTROY` / `WM_MOVE` / `WM_SIZE` / `WM_SIZING` / `WM_ENTERSIZEMOVE` / `WM_EXITSIZEMOVE` / `WM_SETFOCUS` / `WM_KILLFOCUS` / `WM_PAINT`
  - キーボード: `WM_KEYDOWN` / `WM_KEYUP` / `WM_SYSKEYDOWN` / `WM_SYSKEYUP` / `WM_CHAR`
  - マウス: `WM_MOUSEMOVE` / `WM_LBUTTONDOWN` / `WM_LBUTTONUP` / `WM_LBUTTONDBLCLK` / `WM_RBUTTONDOWN` / `WM_RBUTTONUP` / `WM_RBUTTONDBLCLK` / `WM_MBUTTONDOWN` / `WM_MBUTTONUP` / `WM_MOUSEWHEEL` / `WM_MOUSEHWHEEL`
  - その他: `WM_DROPFILES`

```angelscript
class WindowWatcher : ScriptComponentBehavior {
    void OnWindowMessage(const WindowMessageInfo &in info) {
        if (info.message == WM_SIZE) {
            Log("リサイズ: " + info.sourceComponent.GetClientWidth() + " x " + info.sourceComponent.GetClientHeight());
        }
        if (info.message == WM_KILLFOCUS) {
            Log("フォーカスを失った: " + info.sourceComponent.GetTitle());
        }

        // 複数のウィンドウコンポーネントがある場合、
        // どのコンポーネントからの通知かをハンドル比較で判別できる
        NormalWindowObject@ mainWindow;
        GetComponent(@mainWindow);
        if (info.sourceComponent is mainWindow) {
            // メインウィンドウのメッセージ
        }
    }
}
```

#### メッセージの横取り（インターセプト）

`SetMessageIntercepted(msg, true)` で登録したメッセージは、**ウィンドウの既定処理（エンジン既定のイベント処理・OSの既定処理）が実行されなくなり**、`OnWindowMessage` への通知だけが行われます。メッセージ種別ごとに「そのままウィンドウの処理を通す」か「ウィンドウの処理を中断してゲーム側の処理に差し替える」かを選択できます。

- 設定はスクリプトの `SetMessageIntercepted(uint msg, bool enabled)`、またはインスペクターの「**Intercepted Messages**」セクション（`WM_CLOSE` 用のチェックボックスと、メッセージ番号指定での追加/削除）から行えます。設定はシーンへ保存され、ウィンドウの再生成時にも引き継がれます。
- **`WM_CLOSE` を横取りすると、Xボタン・Alt+F4でウィンドウが閉じなくなります**（既定の確認ダイアログも出ません）。代わりにスクリプトへ `WM_CLOSE` が通知されるため、セーブや確認UIなどゲーム側の処理を行い、閉じてよくなったら `CloseWindow()` で明示的に閉じてください。
- `WM_KEYDOWN` / `WM_CHAR` / マウス系などの入力メッセージは安全に横取りできます（OSの既定処理を止めたい場合に使用。例: `WM_SYSKEYDOWN` を横取りしてAltキーのメニュー起動やAlt+Enterのフルスクリーン切替を無効化する）。
- **注意**: `WM_NCHITTEST` / `WM_PAINT` / `WM_SETCURSOR` / `WM_SYSCOMMAND` 全体などを横取りすると、ドラッグ移動・リサイズ・最小化・描画といったウィンドウの基本動作が壊れます（Win32の仕様）。ゲーム側で処理したいメッセージにのみ使用してください。
- `WM_DESTROY` / `WM_NCDESTROY` / `WM_QUIT` は横取りできません（`SetMessageIntercepted` が `false` を返します）。
- 通知の仕様は通常の `OnWindowMessage` と同じです（フレーム毎・メッセージ種別ごとに最後の1件・順序不定）。同フレームに同種メッセージが複数来た場合の個別処理はできません。

```angelscript
class GameWindow : ScriptComponentBehavior {
    void Start() {
        NormalWindowObject@ window;
        GetComponent(@window);
        window.SetMessageIntercepted(WM_CLOSE, true); // Xボタンで閉じずにスクリプトへ通知する
    }

    void OnWindowMessage(const WindowMessageInfo &in info) {
        if (info.message == WM_CLOSE) {
            // セーブや終了確認UIなど、ゲーム側の処理をここで行う
            Log("閉じる操作を検知しました");
            // 閉じてよければ明示的に閉じる（呼ばなければウィンドウは開いたまま）
            info.sourceComponent.CloseWindow();
        }
    }
}
```

### コライダー（BoxCollider / SphereCollider / CapsuleCollider / MeshCollider / RayCollider / Box2DCollider / Circle2DCollider / Capsule2DCollider / Ray2DCollider）

共通メソッドに加えて以下を持ちます。

| メソッド | 説明 |
|---|---|
| `bool IsTrigger() const` / `void SetTrigger(bool)` | トリガー（すり抜け）かどうか |
| `bool IsContinuousDetection() const` / `void SetContinuousDetection(bool)` | 連続衝突判定（CCD）が有効かどうか（[詳細](#連続衝突判定ccd)） |
| `bool Is2D() const` | 2D用コライダーかどうか |

#### 連続衝突判定（CCD）

高速に移動するオブジェクトは、1フレームの移動量が形状のサイズを超えると相手のコライダーを「すり抜けて」しまい、衝突イベントが発生しないことがあります。連続衝突判定を有効にすると、前フレーム位置から現在位置までの**移動経路を形状サイズ以下の刻みに分割して中間位置でも判定**するため、すり抜けても `OnCollisionEnter` 等のイベントが正しく発生します。

- インスペクターの「**Continuous Detection**」チェックボックス、またはスクリプトの `SetContinuousDetection(true)` で有効にします（2D/3Dどちらのコライダーにも対応）。
- 移動量が形状の最小半径以下の間はスイープ判定が走らないため、通常の速度では追加コストはほぼありません。高速に動く弾丸や小さいオブジェクトにだけ有効にしてください。
- 経路の分割数には上限（16分割）があるため、1フレームで形状サイズの16倍を超えるような極端な移動では取りこぼす可能性があります。
- **テレポート**（リスポーン等の瞬間移動）でも移動経路上の判定が走る点に注意してください。瞬間移動させる場合は、移動前にCCDを無効にするか、コライダーを一時的に無効化してください。

```angelscript
class Bullet : ScriptComponentBehavior {
    void Start() {
        SphereCollider@ collider;
        if (GetComponent(@collider)) {
            collider.SetContinuousDetection(true); // 高速移動してもすり抜けを検出できるようにする
        }
    }
}
```

#### Collider（基底型）

全コライダー型の基底となる参照型です。`HitInfo` の `selfCollider` / `otherCollider` として受け取れます。コンポーネント共通メソッド（`IsActive` / `GetComponentType` / `GetTag` 等）と、コライダー共通の `IsTrigger` / `SetTrigger` / `IsContinuousDetection` / `SetContinuousDetection` / `Is2D` が使用できます。

- 各コライダー型のハンドルは `Collider@` へ**暗黙的に変換**できます。
- `cast<BoxCollider>(collider)` のように**具体的な型へダウンキャスト**できます（型が異なる場合は `null` が返ります）。

```angelscript
class Player : ScriptComponentBehavior {
    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherCollider is null) return;

        // 相手コライダーのトリガー状態やタグで判別する
        // （タグはオブジェクトとは別に、コライダー単位でも設定できる）
        if (hit.otherCollider.IsTrigger()) return;
        if (hit.otherCollider.GetTag() == Tag("DamageZone")) {
            Log("ダメージゾーンに接触");
        }

        // 種類名での判別
        if (hit.otherCollider.GetComponentType() == "SphereCollider") {
            // 具体的な型へのダウンキャスト
            SphereCollider@ sphere = cast<SphereCollider>(hit.otherCollider);
        }

        // 自オブジェクトに複数のコライダーが付いている場合、
        // どのコライダーで衝突したのかをハンドル比較で判別できる
        BoxCollider@ attackCollider;
        GetComponent(@attackCollider);
        if (hit.selfCollider is attackCollider) {
            Log("攻撃判定がヒット");
        }
    }
}
```

### ポストプロセスエフェクト

`ScreenBufferObject` が付与されたオブジェクトに追加すると、そのスクリーンバッファへエフェクトがかかります。いずれも共通メソッドに加えて以下のGet/Setを持ちます。

| コンポーネント | メソッド |
|---|---|
| `BloomEffect` | `GetThreshold`/`SetThreshold`, `GetSoftKnee`/`SetSoftKnee`, `GetIntensity`/`SetIntensity`, `GetBlurRadius`/`SetBlurRadius`（すべて`float`）, `GetIterations`/`SetIterations`（`uint`、ダウンサンプル段数1～16） |
| `BoxFilterEffect` | `GetIntensity`/`SetIntensity`（`float`）, `GetHalfSizeX`/`GetHalfSizeY`（`int`）, `SetHalfSize(int, int)` |
| `ChromaticAberrationEffect` | `GetDirection`/`SetDirection`（`Vector2`）, `GetStrength`/`SetStrength`（`float`） |
| `ColorAdjustEffect` | `GetBrightness`/`SetBrightness`, `GetContrast`/`SetContrast`, `GetSaturation`/`SetSaturation`, `GetTemperature`/`SetTemperature`（すべて`float`）, `GetColorBalance`/`SetColorBalance`（`Vector3`） |
| `DissolveEffect` | `GetMaskThreshold`/`SetMaskThreshold`, `GetEdgeThickness`/`SetEdgeThickness`（`float`）, `GetBaseTexturePath`/`SetBaseTexturePath`, `GetMaskTexturePath`/`SetMaskTexturePath`（`string`、読み込み済みテクスチャのAssetsルートからの相対パス）, `GetBaseTextureColor`/`SetBaseTextureColor`, `GetEdgeColor`/`SetEdgeColor`（`Vector4`） |
| `DitherEffect` | `GetIntensity`/`SetIntensity`（`float`）, `IsColorDither`/`SetColorDither`（`bool`） |
| `DotMatrixEffect` | `GetDotSpacing`/`SetDotSpacing`, `GetDotRadius`/`SetDotRadius`, `GetThreshold`/`SetThreshold`, `GetIntensity`/`SetIntensity`（`float`）, `IsMonochrome`/`SetMonochrome`（`bool`） |
| `FXAAEffect` | `GetThreshold`/`SetThreshold`, `GetThresholdMin`/`SetThresholdMin`, `GetStrength`/`SetStrength`（すべて`float`） |
| `GaussianFilterEffect` | `GetRadius`/`SetRadius`（`int`）, `GetSigma`/`SetSigma`（`float`） |
| `GrayscaleEffect` | `GetIntensity`/`SetIntensity`（`float`） |
| `OutlineEffect` | `GetThreshold`/`SetThreshold`, `GetThickness`/`SetThickness`（`float`）, `GetColor`/`SetColor`（`Vector4`）, `GetCameraNear`/`SetCameraNear`, `GetCameraFar`/`SetCameraFar`（`float`） |
| `RadialBlurEffect` | `GetIntensity`/`SetIntensity`（`float`）, `GetSampleCount`/`SetSampleCount`（`int`）, `GetCenter`/`SetCenter`（`Vector2`）, `GetStartRadius`/`SetStartRadius`（`float`） |
| `VignetteEffect` | `GetCenter`/`SetCenter`（`Vector2`）, `GetColor`/`SetColor`（`Vector4`）, `GetIntensity`/`SetIntensity`, `GetInnerRadius`/`SetInnerRadius`, `GetSmoothness`/`SetSmoothness`（`float`） |

## 数学型

全て値型です（変数への代入はコピーになります）。

### Vector2 / Vector3 / Vector4

```angelscript
Vector3 v(1.0f, 2.0f, 3.0f);
v.x += 1.0f;
Vector3 doubled = v * 2.0f;
float len = v.Length();
```

| 要素 | Vector2 | Vector3 | Vector4 |
|---|---|---|---|
| コンストラクタ | `(float x, float y)` | `(float x, float y, float z)` | `(float x, float y, float z, float w)` |
| プロパティ | `x` `y` | `x` `y` `z` | `x` `y` `z` `w` |

共通の演算子: 単項`-` / `+` / `-` / `+=` / `-=` / `==` / ベクトル同士の`/` / スカラーとの`*` `/`

| メソッド | Vector2 | Vector3 | Vector4 |
|---|:-:|:-:|:-:|
| `float Dot(const T &in) const` | o | o | - |
| `Cross(const T &in) const` | o（`float`） | o（`Vector3`） | - |
| `float Length() const` | o | o | - |
| `float LengthSquared() const` | o | o | - |
| `T Normalize() const` | o | o | - |
| `float Distance(const T &in) const` | o | o | - |

行列との乗算: `Vector2 * Matrix3x3`、`Vector3 * Matrix4x4`（およびその逆順）、`Vector3 Transform(const Matrix4x4 &in) const`

### Quaternion

```angelscript
Quaternion q = Math::MakeRotateAxisAngle(Vector3(0, 1, 0), 3.14159f * 0.5f);
Vector3 rotated = q.RotateVector(Vector3(1, 0, 0));
```

- コンストラクタ: `(float x, float y, float z, float w)`、プロパティ: `x` `y` `z` `w`
- 演算子: `+` / `-` / `*`（クォータニオン同士） / スカラーとの `*` `/`

| メソッド | 説明 |
|---|---|
| `Quaternion Conjugate() const` | 共役クォータニオンを取得する |
| `float Norm() const` / `float NormSquared() const` | ノルム / ノルムの2乗 |
| `Quaternion Normalize() const` | 正規化したクォータニオンを取得する |
| `Quaternion Inverse() const` | 逆クォータニオンを取得する |
| `Vector3 RotateVector(const Vector3 &in) const` | ベクトルを回転させる |
| `Matrix4x4 MakeRotateMatrix() const` | 回転行列を生成する |

### Matrix3x3 / Matrix4x4

```angelscript
Matrix4x4 mat = Math::IdentityMatrix4x4();
mat.MakeAffine(Vector3(1, 1, 1), Vector3(0, 0, 0), Vector3(0, 5, 0));
float m03 = mat.GetElement(0, 3);
```

- コンストラクタ: 全要素を行優先で指定（Matrix3x3は9個、Matrix4x4は16個のfloat）
- 演算子: `+` / `-` / 行列同士の`*` / スカラーとの`*`

| メソッド | Matrix3x3 | Matrix4x4 | 説明 |
|---|:-:|:-:|---|
| `float GetElement(uint row, uint col) const` | o | o | 要素の取得（範囲外は0を返す） |
| `void SetElement(uint row, uint col, float value)` | o | o | 要素の設定（範囲外は無視） |
| `T Transpose()` | o | o | 転置行列を取得する |
| `float Determinant() const` | o | o | 行列式を計算する |
| `T Inverse() const` | o | o | 逆行列を計算する |
| `void MakeIdentity()` / `MakeTranspose()` / `MakeInverse()` | o | o | 自身を単位/転置/逆行列にする |
| `void MakeTranslate(...)` | o（`Vector2`） | o（`Vector3`） | 平行移動行列を生成する |
| `void MakeScale(...)` | o（`Vector2`） | o（`Vector3`） | 拡大縮小行列を生成する |
| `void MakeRotate(...)` | o（`float`） | o（`Vector3` または `float x3`） | 回転行列を生成する |
| `void MakeRotateX/Y/Z(float)` | - | o | 各軸回転行列を生成する |
| `void MakeAffine(...)` | o | o | アフィン行列を生成する（スケール, 回転, 平行移動） |
| `void MakeViewMatrix(const Vector3 &in eye, const Vector3 &in target, const Vector3 &in up)` | - | o | ビュー行列を生成する |
| `void MakePerspectiveFovMatrix(float fovY, float aspect, float near, float far)` | - | o | 透視投影行列を生成する |
| `void MakeOrthographicMatrix(float l, float t, float r, float b, float near, float far)` | - | o | 正射影行列を生成する |
| `void MakeViewportMatrix(float left, float top, float width, float height, float minD, float maxD)` | - | o | ビューポート行列を生成する |

## Math名前空間

数学系の静的関数は `Math::` 名前空間にまとめられています。

| 関数 | 説明 |
|---|---|
| `Vector2 Math::Lerp(const Vector2 &in, const Vector2 &in, float t)` | 線形補間 |
| `Vector2 Math::Slerp(const Vector2 &in, const Vector2 &in, float t)` | 球面線形補間 |
| `Vector3 Math::Lerp(const Vector3 &in, const Vector3 &in, float t)` | 線形補間 |
| `Vector3 Math::Slerp(const Vector3 &in, const Vector3 &in, float t)` | 球面線形補間 |
| `Vector4 Math::Lerp(const Vector4 &in, const Vector4 &in, float t)` | 線形補間 |
| `Quaternion Math::Slerp(const Quaternion &in, const Quaternion &in, float t)` | 球面線形補間 |
| `Quaternion Math::IdentityQuaternion()` | 単位クォータニオンを取得する |
| `Quaternion Math::MakeRotateEuler(const Vector3 &in euler)` | オイラー角（ラジアン）から回転を生成する |
| `Quaternion Math::MakeRotateAxisAngle(const Vector3 &in axis, float angleRad)` | 軸と角度から回転を生成する |
| `Quaternion Math::MakeFromRotationMatrix(const Matrix4x4 &in)` | 回転行列からクォータニオンを生成する |
| `Matrix3x3 Math::IdentityMatrix3x3()` | 3x3単位行列を取得する |
| `Matrix4x4 Math::IdentityMatrix4x4()` | 4x4単位行列を取得する |

## Easing（イージング）

`Utilities/MathUtils/Easings.h` のイージング関数を `Easing::` 名前空間で使用できます。

```angelscript
class Popup : ScriptComponentBehavior {
    float elapsed = 0.0f;

    void Update() {
        elapsed += GetDeltaTime();
        float t = Easing::Normalize01(elapsed, 0.0f, 1.0f); // 0.0~1.0に正規化
        Vector3 pos = Easing::Eased(Vector3(0, -5, 0), Vector3(0, 0, 0), t, EaseType::EaseOutBack);
        GetTransform().SetTranslate(pos);
    }
}
```

### EaseType

`Linear` と、Quad/Cubic/Quart/Quint/Sine/Expo/Circ/Back/Elastic/Bounce の各カーブに対して `EaseIn` / `EaseOut` / `EaseInOut` / `EaseOutIn` を組み合わせた列挙値が使用できます（例: `EaseType::EaseInOutCubic`、`EaseType::EaseOutBounce`）。

### 関数

| 関数 | 説明 |
|---|---|
| `float/Vector2/Vector3/Vector4 Easing::Normalize01(value, min, max)` | `value` を `min`～`max` の範囲で 0.0～1.0 に正規化する（クランプ済み） |
| `float Easing::Lerp(float start, float end, float t)` | 線形補間（`Vector2`/`Vector3`/`Vector4` の線形補間は [Math名前空間](#math名前空間) を使用） |
| `float Easing::Apply(float t, EaseType type)` | 0.0～1.0の進行度 `t` にイージングカーブを適用した値（0.0～1.0）を返す |
| `float/Vector2/Vector3/Vector4 Easing::Eased(start, end, float t, EaseType type)` | `start`→`end` を `t`（0.0～1.0）とイージングタイプで補間する |
| `float/Vector2/Vector3/Vector4 Easing::EasedGAB(start, end, float t, EaseType goType, EaseType backType)` | `start`→`end`→`start` と行って帰ってくる補間（`t`が0.5未満で行き、0.5以上で帰り） |

## Random（乱数）

`Utilities/RandomValue.h` の乱数関数を `Random::` 名前空間で使用できます。

```angelscript
int enemyType = Random::Int(0, 2);
float spawnDelay = Random::Float(1.0f, 3.0f);
if (Random::Bool(0.1f)) { // 10%の確率でtrue
    Log("レアドロップ発生");
}
```

| 関数 | 説明 |
|---|---|
| `int Random::Int(int min, int max)` | `min`以上`max`以下のランダムな整数を取得する |
| `int64 Random::Int64(int64 min, int64 max)` | `min`以上`max`以下のランダムな64ビット整数を取得する |
| `float Random::Float(float min, float max)` | `min`以上`max`以下のランダムな浮動小数点数を取得する |
| `double Random::Double(double min, double max)` | `min`以上`max`以下のランダムな倍精度浮動小数点数を取得する |
| `bool Random::Bool(float trueProbability = 0.5f)` | `trueProbability`（0.0～1.0）の確率で `true` を返す |

## WaveFunctionCollapse（波動関数崩壊アルゴリズム）

`WaveFunctionCollapse` 型（参照型）を使うと、いわゆる「波動関数崩壊法（WFC）」でタイルベースのマップを自動生成できます。タイルの登録・接続情報の設定・グリッドサイズ/固定タイル/開始位置の指定・崩壊解決（`Solve`）まで一通りスクリプトから行えます。

```angelscript
WaveFunctionCollapse@ wfc = WaveFunctionCollapse();
wfc.SetSeed(12345);
wfc.SetGridSize(10, 1, 10); // 2次元的に使う場合はYを1にする

// タイルを登録し、方向ごとに接続可能なタイルIDを追加する
wfc.RegisterTile("Grass");
wfc.RegisterTile("Water");
wfc.AddTileConnection("Grass", WFCDirection::Right, "Grass");
wfc.AddTileConnection("Grass", WFCDirection::Left, "Grass");
wfc.AddTileConnection("Water", WFCDirection::Right, "Water");
wfc.AddTileConnection("Water", WFCDirection::Left, "Water");
// 接続は片方向の宣言のため、逆方向（WaterのLeftにGrassを許可する 等）も必要なら別途追加する

wfc.FixTile(0, 0, 0, "Grass"); // 座標(0,0,0)を草地で固定する
wfc.SetStartPosition(0, 0, 0);

if (wfc.Solve()) {
    for (uint x = 0; x < wfc.GetGridWidth(); x++) {
        for (uint z = 0; z < wfc.GetGridDepth(); z++) {
            string tileName;
            if (wfc.TryGetResolvedTile(x, 0, z, tileName)) {
                // tileNameに応じてオブジェクトを配置する
            }
        }
    }
} else {
    LogWarning("WFCの解決に失敗しました（矛盾が発生）");
}

// シード・タイル定義・固定タイル等をJSONへ保存/復元できる
Json@ json = wfc.SaveToJson();
SaveJsonFile("SaveData/wfc.json", json);
```

### WFCDirection

タイルの接続方向を表す列挙です。`Up` / `Down` / `Left` / `Right` / `Front` / `Back` の6方向。

### WaveFunctionCollapseのメンバ

| メンバ | 説明 |
|---|---|
| `WaveFunctionCollapse()` | 新規インスタンスを作成する |
| `void SetSeed(uint seed) / uint GetSeed() const` | 乱数シード値の設定・取得 |
| `void SetGridSize(uint width, uint height, uint depth)` | グリッドサイズを設定する（変更すると既存の固定タイル・開始位置はクリアされる） |
| `uint GetGridWidth/GetGridHeight/GetGridDepth() const` | 現在のグリッドサイズを取得する |
| `bool RegisterTile(const string &in name)` | 接続情報を持たない空のタイルを登録する（名前が空、または同名のタイルが登録済みの場合は`false`） |
| `bool RemoveTile(const string &in tileName)` | 登録済みのタイルを削除する（そのタイルで固定・確定済みだったセルは解除される） |
| `bool AddTileConnection(const string &in tileName, WFCDirection direction, const string &in connectedTileName)` | 指定タイルの指定方向へ接続可能なタイル名を1つ追加する（タイルが未登録の場合は`false`） |
| `bool FixTile(uint x, uint y, uint z, const string &in tileName)` | 指定座標のタイルを固定する（`Solve`実行時の確定制約になる） |
| `bool TryGetFixedTile(uint x, uint y, uint z, string &out tileName) const` | 指定座標に固定されているタイル名を取得する（固定されていない場合は`false`） |
| `bool SetStartPosition(uint x, uint y, uint z)` | 崩壊を開始する座標を指定する |
| `bool TryGetStartPosition(uint &out x, uint &out y, uint &out z) const` | 指定済みの開始座標を取得する（未指定の場合は`false`） |
| `bool Solve()` | 波動関数崩壊アルゴリズムを実行し、グリッド全体のタイルを確定させる（矛盾が発生した場合は`false`） |
| `bool TryGetResolvedTile(uint x, uint y, uint z, string &out tileName) const` | `Solve`で確定したタイル名を取得する |
| `Json@ SaveToJson() const` | シード・グリッドサイズ・登録タイル・固定タイル・開始座標をJsonへ保存する |
| `bool LoadFromJson(const Json &in json)` | `SaveToJson`で保存したJsonから状態を復元する |

- 接続方向は片方向の宣言です。「AのRightにBを許可する」からといって「BのLeftにAを許可する」が自動で成立するわけではないため、必要な組み合わせは両方向とも`AddTileConnection`で設定してください。
- `Solve`は矛盾（候補が0件になるセル）が発生した場合、バックトラックせずその時点で`false`を返します。タイルの接続定義やシードによっては失敗することがあるため、戻り値は必ず確認してください。
- `SetStartPosition`を指定していると、`Solve`実行時に最初の1マスをその座標から崩壊させます（以降は残り候補数が最小のセルがランダムに選ばれます）。未指定の場合は最初から残り候補数最小のセルが選ばれます。
- `SaveToJson`/`LoadFromJson`は`Solve`の確定結果（`TryGetResolvedTile`で取得できる値）は保存しません。復元後に再度`Solve`を呼び出してください。

## StageGraphGenerator / StageGridBuilder（ステージの部屋グラフ生成）

「必ずスタートからゴールまで繋がる」ステージを自動生成するための2つの型です。役割が分かれています。

- **StageGraphGenerator**: 抽象的な「部屋グラフ」（スタート→ゴールのメインルートと、そこから枝分かれする行き止まりの部屋）をランダム生成する。座標はあくまで抽象的な部屋グリッド上のもの（タイル座標ではない）。
- **StageGridBuilder**: `StageGraphGenerator` が生成したグラフを、実際のタイルサイズに展開して `WaveFunctionCollapse` へ固定タイルとして書き込む。部屋同士の隙間のうち通路にならない部分は固定されないため、`WaveFunctionCollapse::Solve()` で壁等の装飾として埋められる。

```angelscript
// 1. 部屋グラフを生成する（スタート→ゴールは必ず繋がる）
StageGraphGenerator@ graph = StageGraphGenerator();
graph.SetSeed(12345);
graph.SetGridSize(6, 3, 2); // メインルートの長さ・上下スロット数・奥行スロット数
graph.SetBranchProbability(0.35f);
graph.AddSideRoomType(RoomType::Building, 1.0f);      // 建物入口（重み1.0）
graph.AddSideRoomType(RoomType::GimmickDepth, 1.0f);  // 奥行ギミック（重み1.0）
graph.AddSideRoomType(RoomType::Treasure, 1.5f);      // 収集アイテム部屋（重み1.5、出やすい）
graph.Generate();

// 2. WaveFunctionCollapseへタイルを登録しておく（事前準備。詳細は前節を参照）
WaveFunctionCollapse@ wfc = WaveFunctionCollapse();
// ...RegisterTile / AddTileConnection...

// 3. 部屋グラフを実際のタイルグリッドへ展開する（部屋・通路タイルを固定する）
StageGridBuilder@ builder = StageGridBuilder();
builder.SetRoomSize(3, 2, 3);      // 部屋1つ分のタイルサイズ
builder.SetRoomSpacing(1);         // 部屋同士の隙間（通路にならない部分はWFCの装飾対象）
builder.SetCorridorWidth(1);       // 通路の太さ
builder.SetTileWorldSize(2.0f);    // タイル1個分のワールド座標上のサイズ
builder.SetDefaultRoomTileName("RoomFloor"); // 個別設定していない部屋種別に使う既定タイル
builder.SetRoomTileName(RoomType::Treasure, "TreasureFloor");
builder.SetCorridorTileName("CorridorFloor");

if (builder.Build(graph, wfc)) {
    // 4. 部屋・通路以外の隙間をWaveFunctionCollapseで解決する
    if (wfc.Solve()) {
        // 5. 部屋種別ごとにオブジェクトを配置する例（宝箱部屋にマーカーを置く）
        uint roomCount = graph.GetRoomCount();
        for (uint i = 0; i < roomCount; i++) {
            if (graph.GetRoomType(i) != RoomType::Treasure) continue;
            Vector3 pos;
            if (builder.TryGetRoomWorldCenter(graph, graph.GetRoomID(i), pos)) {
                // posの位置に宝箱オブジェクトを生成する、等
            }
        }
    }
}
```

### RoomType

部屋の意味的な種別を表す列挙です。`Start` / `Goal` / `Normal`（メインルート上の通常の部屋） / `Branch`（種別未指定の寄り道） / `Building`（建物入口） / `GimmickDepth`（奥行ギミック） / `Treasure`（収集アイテム部屋）。

### StageGraphGeneratorのメンバ

| メンバ | 説明 |
|---|---|
| `StageGraphGenerator()` | 新規インスタンスを作成する |
| `void SetSeed(uint seed) / uint GetSeed() const` | 乱数シード値の設定・取得 |
| `void SetGridSize(uint width, uint height, uint depth)` | 抽象部屋グリッドのサイズを設定する（width=メインルートの進行方向、height=上下、depth=奥行） |
| `uint GetGridWidth/GetGridHeight/GetGridDepth() const` | 現在の抽象部屋グリッドサイズを取得する |
| `void SetBranchProbability(float probability) / float GetBranchProbability() const` | メインルートの各部屋から寄り道部屋が生成される確率（0.0～1.0） |
| `bool AddSideRoomType(RoomType type, float weight)` | 寄り道部屋の種別候補を重み付きで登録する（未登録の場合は全て`RoomType::Branch`になる） |
| `void ClearSideRoomTypes()` | 登録済みの寄り道部屋種別候補をすべて削除する |
| `void Generate()` | 部屋グラフを生成する（既存の生成結果は破棄される。グリッド幅2未満、または高さ・奥行が0の場合は何も生成しない） |
| `uint GetRoomCount() const` | 生成された部屋の数を取得する |
| `uint GetRoomID(uint index) const` | 生成順でindex番目の部屋のIDを取得する |
| `RoomType GetRoomType(uint index) const` | index番目の部屋の種別を取得する |
| `uint GetRoomX/GetRoomY/GetRoomZ(uint index) const` | index番目の部屋の抽象グリッド座標を取得する |
| `uint GetRoomConnectionCount(uint index) const` | index番目の部屋が接続している部屋の数を取得する |
| `uint GetRoomConnectedRoomID(uint index, uint connectionIndex) const` | index番目の部屋のconnectionIndex番目の接続先部屋IDを取得する |
| `bool TryGetStartRoomID(uint &out roomID) const / bool TryGetGoalRoomID(uint &out roomID) const` | スタート/ゴールの部屋IDを取得する（未生成の場合は`false`） |

### StageGridBuilderのメンバ

| メンバ | 説明 |
|---|---|
| `StageGridBuilder()` | 新規インスタンスを作成する |
| `void SetRoomSize(uint sizeX, uint sizeY, uint sizeZ)` | 部屋1つ分が占めるタイルサイズを設定する |
| `void SetRoomSpacing(uint spacing)` | 隣接する部屋ブロックの間に空ける隙間（タイル数）を設定する |
| `void SetCorridorWidth(uint width)` | 部屋同士を繋ぐ通路の太さ（タイル数）を設定する（RoomSpacingを超える値は隙間の幅で切り詰められる） |
| `void SetTileWorldSize(float size)` | タイル1個分に対応する実際のワールド座標上のサイズを設定する |
| `void SetRoomTileName(RoomType type, const string &in tileName)` | 部屋種別ごとに固定するタイル名を設定する |
| `void SetDefaultRoomTileName(const string &in tileName)` | `SetRoomTileName` で個別設定されていない部屋種別に使う既定のタイル名（必須） |
| `void SetCorridorTileName(const string &in tileName)` | 部屋同士を繋ぐ通路に固定するタイル名（必須） |
| `bool Build(const StageGraphGenerator &in graph, WaveFunctionCollapse@ wfc)` | 部屋グラフの内容をwfcへ展開する（`wfc.SetGridSize`を内部で呼び出すため、既存の固定タイル等はクリアされる） |
| `bool TryGetRoomGridCenter(const StageGraphGenerator &in graph, uint roomID, uint &out x, uint &out y, uint &out z) const` | 部屋の中心タイル座標を取得する |
| `bool TryGetRoomWorldCenter(const StageGraphGenerator &in graph, uint roomID, Vector3 &out position) const` | 部屋の中心位置をワールド座標として取得する |
| `void GetRequiredGridSize(const StageGraphGenerator &in graph, uint &out width, uint &out height, uint &out depth) const` | Buildで実際に必要となるWaveFunctionCollapseのグリッドサイズを取得する |

- `Build`を呼ぶ前に、使用する全タイル名（部屋種別ごとのタイル・通路タイル）を`wfc.RegisterTile`で登録しておく必要があります（`Build`自体はタイルの登録を行いません）。
- 部屋グラフの辺は、抽象グリッド上で隣接する部屋同士のみを想定しています。それ以外（隣接していない部屋同士の接続）がある場合、`Build`は`false`を返します（`StageGraphGenerator`が生成するグラフは常にこの条件を満たします）。
- `SetRoomSpacing(0)`を指定すると部屋同士が直接接するため、通路タイルは使われません。

## サンプルスクリプト

```angelscript
class Player : ScriptComponentBehavior {
    [SerializeField]
    float moveSpeed = 5.0f;

    [SerializeField]
    string jumpSound = "Sounds/jump.wav";

    Velocity@ velocity;

    void Start() {
        Log("Player start: " + GetOwnerObject().GetName());
        GetComponent(@velocity);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        // 入力コマンドで左右移動
        float moveX = GetCommandValue("MoveX");
        Vector3 pos = tf.GetTranslate();
        pos.x += moveX * moveSpeed * GetDeltaTime();
        tf.SetTranslate(pos);

        // ジャンプ
        if (IsCommandTriggered("Jump") && velocity !is null) {
            velocity.AddVelocity(Vector3(0.0f, 8.0f, 0.0f));
            PlayAudio(jumpSound);
        }
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject !is null && hit.otherObject.GetName() == "Enemy") {
            LogWarning("敵と衝突: penetration=" + hit.penetration);
            GetScene().SetNextSceneName("GameOver");
            GetScene().ChangeToNextScene();
        }
    }

    void End() {
        Log("Player end");
    }
}
```

## 注意事項

- **ハンドルの寿命**: `Object@` やコンポーネントのハンドルはエンジン側が所有する実体への参照（参照カウント無し）です。オブジェクトやコンポーネントが削除された後のハンドルへアクセスすると未定義動作になります。フレームをまたいで保持する場合は削除タイミングに注意してください。
- **Behaviorクラスは1モジュール1つ**: `ScriptComponentBehavior` を実装したクラスが複数ある場合、最初に見つかった1つだけが使用されます。
- **コライダーの追加/削除**: 衝突コールバックのフックは毎フレームのコライダー数チェックで追従しますが、コライダーの追加とほぼ同時に発生した衝突は最初の1フレームだけ通知されないことがあります。
- **RayCollider / Ray2DCollider**: レイキャスト専用のコライダーは常駐形状を持たないため、OnCollisionEnter等の衝突イベントは発生しません。
- **数学型は値型**: `Vector3` 等を変数に代入するとコピーされます。`Transform` の座標を変更する場合は `GetTranslate()` で取得→変更→`SetTranslate()` で書き戻してください。
- **文字列と数値の連結**: `"value=" + 1.0f` のような連結が可能です（scriptstdstringアドオンによる）。
- **ポストプロセスエフェクトの内部Params構造体**: `BloomEffect`等が内部で持つ `Params` 構造体自体はスクリプトへ公開されていません。フィールドごとのGet/Setメソッドを使用してください。
- **`Object@` を要求する引数**: `MeshRenderer::SetTargetObject`や`CameraController::AddFollowTarget`のように `Object@` を引数に取るメソッドへ `null` を渡した場合は何もしません（クラッシュしません）。
- **AddComponentの追加数上限**: コンポーネント種別ごとに1オブジェクトへ追加できる最大数が決まっており（例: `Transform`は1個まで）、上限を超えて`AddComponent`を呼ぶと失敗し`false`が返ります。
- **CloneObjectの複製範囲**: `Scene.CloneObject`は対象オブジェクト自身が持つコンポーネントのみを複製します。子オブジェクトの複製や、親子関係（`Transform`の親設定）の引き継ぎは行われません。
