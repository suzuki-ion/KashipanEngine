# KashipanEngine スクリプトリファレンス

KashipanEngineに組み込まれたAngelScriptの利用方法と、スクリプトから使用できるAPIのリファレンスです。

- スクリプト言語: [AngelScript](https://www.angelcode.com/angelscript/)
- 対象コンポーネント: `ScriptComponent`（オブジェクトコンポーネント） / `SceneScriptEngine`（シーンコンポーネント）

---

## 目次

1. [基本的な使い方](#基本的な使い方)
2. [ファイルの分割（#include）](#ファイルの分割include)
3. [ScriptComponentBehavior（ライフサイクル）](#scriptcomponentbehaviorライフサイクル)
4. [SerializeField（変数のインスペクター編集・保存）](#serializefield変数のインスペクター編集保存)
5. [コンポーネントの取得（GetComponent / GetComponents）](#コンポーネントの取得getcomponent--getcomponents)
6. [グローバル関数](#グローバル関数)
7. [オブジェクト・シーン型](#オブジェクトシーン型)
8. [シーン変数（スクリプト間の値の受け渡し）](#シーン変数スクリプト間の値の受け渡し)
9. [コンポーネント型](#コンポーネント型)
10. [数学型](#数学型)
11. [Math名前空間](#math名前空間)
12. [Easing（イージング）](#easingイージング)
13. [サンプルスクリプト](#サンプルスクリプト)
14. [注意事項](#注意事項)

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
- 標準アドオンの `string`（文字列）と `array`（配列）が使用できます。

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
    void Start() {}                                  // 初期化時に一度だけ
    void Update() {}                                 // 毎フレーム
    void End() {}                                    // 終了時
    void OnCollisionEnter(const HitInfo &in hit) {}  // 衝突開始時
    void OnCollisionStay(const HitInfo &in hit) {}   // 衝突継続中（毎フレーム）
    void OnCollisionExit(const HitInfo &in hit) {}   // 衝突終了時
}
```

| メソッド | 呼び出しタイミング |
|---|---|
| `void Start()` | コンポーネントの初期化時（スクリプトのコンパイル成功後）、およびReload成功後に一度 |
| `void Update()` | 毎フレーム |
| `void End()` | コンポーネントの削除・非アクティブ化・Reload時 |
| `void OnCollisionEnter(const HitInfo &in)` | 同オブジェクトのコライダーが他のコライダーと衝突を開始したとき |
| `void OnCollisionStay(const HitInfo &in)` | 衝突が継続している間、毎フレーム |
| `void OnCollisionExit(const HitInfo &in)` | 衝突が終了したとき |

衝突イベントは同オブジェクトに付いている全ての `ICollider` 派生コンポーネント（2D/3D両方）が対象です。C++側で既に衝突コールバックが設定されている場合、そのコールバックが先に呼ばれた後にスクリプト側が呼ばれます。

### HitInfo

衝突メソッドへ渡される衝突情報です。

| プロパティ | 型 | 内容 |
|---|---|---|
| `normal` | `Vector3` | 衝突面の法線 |
| `penetration` | `float` | めり込み量 |
| `selfObject` | `Object@` | 自身のオブジェクト |
| `otherObject` | `Object@` | 衝突相手のオブジェクト |

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

上記以外の型に付けた場合、インスペクターには `(unsupported type)` と表示され、保存対象になりません。

## コンポーネントの取得（GetComponent / GetComponents）

取得したいコンポーネント型のハンドル変数（または配列）を引数に渡すと、型に応じたコンポーネントが取得できます。

```angelscript
// 単体取得: 最初に見つかったコンポーネントを取得
Velocity@ vel;
if (GetOwnerObject().GetComponent(vel)) {
    vel.AddVelocity(Vector3(0.0f, 5.0f, 0.0f));
}

// 全件取得: array<T@> を渡す
array<AudioSource@> sources;
GetOwnerObject().GetComponents(sources);
for (uint i = 0; i < sources.length(); i++) {
    sources[i].Stop();
}

// グローバル版は自身のオブジェクトが対象（obj.GetComponent(...) の省略形）
Transform@ tf;
GetComponent(tf);
```

| 関数 | 戻り値 | 説明 |
|---|---|---|
| `bool Object::GetComponent(?&out)` | 見つかった場合 `true` | 型に一致する最初のコンポーネントをハンドルへ格納 |
| `bool Object::GetComponents(?&out)` | 成功した場合 `true` | 型に一致する全コンポーネントを `array<T@>` へ格納（0個でも配列は生成される） |
| `bool GetComponent(?&out)` | 同上 | 自身のオブジェクトを対象にした省略形 |
| `bool GetComponents(?&out)` | 同上 | 自身のオブジェクトを対象にした省略形 |

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
| `Transform@ GetTransform()` | Transformコンポーネントを取得する |
| `bool GetComponent(?&out)` | コンポーネントを取得する（[詳細](#コンポーネントの取得getcomponent--getcomponents)） |
| `bool GetComponents(?&out)` | コンポーネントを全件取得する |

### Scene（シーン）

| メソッド | 説明 |
|---|---|
| `const string &GetName() const` | シーン名を取得する |
| `Object@ GetObject(const string &in name) const` | 名前が一致する最初のオブジェクトを取得する |
| `array<Object@>@ GetObjects(const string &in name) const` | 名前が一致する**全ての**オブジェクトを取得する（0件でも配列は返る） |
| `void SetNextSceneName(const string &in)` | 次のシーン名を設定する |
| `bool ChangeToNextScene()` | 次のシーンへ切り替える |
| `bool HasNextSceneName() const` | 次のシーン名が設定されているかを取得する |
| `void ClearNextSceneName()` | 次のシーン名をクリアする |
| `bool SetVariable(const string &in key, ?&in value)` | シーン変数を設定する（[詳細](#シーン変数スクリプト間の値の受け渡し)） |
| `bool GetVariable(const string &in key, ?&out value)` | シーン変数を取得する |
| `bool HasVariable(const string &in key)` | シーン変数が存在するかどうか |
| `bool RemoveVariable(const string &in key)` | シーン変数を削除する |
| `bool SetGlobalVariable(const string &in key, ?&in value)` | グローバルシーン変数を設定する（シーンを跨いで保持される） |
| `bool GetGlobalVariable(const string &in key, ?&out value)` | グローバルシーン変数を取得する |
| `bool HasGlobalVariable(const string &in key)` | グローバルシーン変数が存在するかどうか |
| `bool RemoveGlobalVariable(const string &in key)` | グローバルシーン変数を削除する |

## シーン変数（スクリプト間の値の受け渡し）

`ScriptComponent` は1つにつき独立したスクリプトモジュールとしてコンパイルされるため、あるスクリプトのグローバル変数やクラスのメンバー変数に、別のスクリプトから直接アクセスすることはできません。異なるスクリプト間で値をやり取りしたい場合は、`Scene` のシーン変数を経由します。

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

## コンポーネント型

全てのコンポーネント型は参照型（ハンドル `T@` で保持）です。以下の共通メソッドを持ちます。

| 共通メソッド | 説明 |
|---|---|
| `bool IsActive() const` | アクティブ状態を取得する |
| `void SetActive(bool)` | アクティブ状態を設定する |
| `const string &GetComponentType() const` | コンポーネントの種類名を取得する |

### Transform

| メソッド | 説明 |
|---|---|
| `void SetTranslate(const Vector3 &in)` / `const Vector3 &GetTranslate() const` | 座標の設定/取得 |
| `void SetRotate(const Vector3 &in)` / `const Vector3 &GetRotate() const` | 回転の設定/取得（オイラー角・ラジアン） |
| `void SetRotateQuaternion(const Quaternion &in)` / `const Quaternion &GetRotateQuaternion() const` | 回転の設定/取得（クォータニオン） |
| `void SetScale(const Vector3 &in)` / `const Vector3 &GetScale() const` | スケールの設定/取得 |
| `const Matrix4x4 &GetWorldMatrix()` | ワールド行列を取得する（必要なら再計算される） |

### Velocity

| メソッド | 説明 |
|---|---|
| `void SetVelocity(const Vector3 &in)` / `const Vector3 &GetVelocity() const` | 速度の設定/取得（単位/秒） |
| `void SetAcceleration(const Vector3 &in)` / `const Vector3 &GetAcceleration() const` | 加速度の設定/取得（単位/秒²） |
| `void AddVelocity(const Vector3 &in)` | 速度に加算する |

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

### Light / LightRenderer

`LightType` 列挙型（`Directional` / `Point` / `Spot`）が使用できます。

| メソッド | Light | LightRenderer | 説明 |
|---|:-:|:-:|---|
| `void SetPipelineName(const string &in)` / `const string &GetPipelineName() const` | - | o | 使用パイプライン名 |
| `void SetType(LightType)` / `LightType GetType() const` | o | - | ライト種別 |
| `LightType GetLightType() const` | - | o | 同オブジェクトの `Light` の種別を取得する |
| `Light@ GetLight() const` | - | o | 同オブジェクトの `Light` コンポーネントを取得する |
| `void SetColor(const Vector4 &in)` / `const Vector4 &GetColor() const` | o | - | 色 |
| `void SetIntensity(float)` / `float GetIntensity() const` | o | - | 強度 |
| `void SetRadius(float)` / `float GetRadius() const` | o | - | 半径（Point用） |
| `void SetDistance(float)` / `float GetDistance() const` | o | - | 距離（Spot用） |
| `void SetDecay(float)` / `float GetDecay() const` | o | - | 減衰（Point/Spot共通） |
| `void SetInnerAngle(float)` / `float GetInnerAngle() const` | o | - | 内側角度（Spot用、ラジアン） |
| `void SetOuterAngle(float)` / `float GetOuterAngle() const` | o | - | 外側角度（Spot用、ラジアン） |
| `Vector3 GetWorldPosition() const` | - | o | ワールド座標を取得する |
| `Vector3 GetWorldDirection() const` | - | o | ワールド方向（+Z）を取得する |

### 描画先コンポーネント（NormalWindowObject / OverlayWindowObject / ScreenBufferObject / ShadowMapObject）

| メソッド | Window系(Normal/Overlay) | Buffer系(ScreenBuffer/ShadowMap) | 説明 |
|---|:-:|:-:|---|
| `void SetTitle(const string &in)` | o | - | ウィンドウタイトルの設定 |
| `void SetName(const string &in)` / `const string &GetName() const` | - | o | 管理用名前（TextureManagerへの登録名）の設定/取得 |
| `void SetSize(uint width, uint height)` | o | o | サイズの設定（バッファ系は既存バッファを実際にリサイズする） |

### コライダー（BoxCollider / SphereCollider / CapsuleCollider / MeshCollider / RayCollider / Box2DCollider / Circle2DCollider / Capsule2DCollider / Ray2DCollider）

共通メソッドに加えて以下を持ちます。

| メソッド | 説明 |
|---|---|
| `bool IsTrigger() const` / `void SetTrigger(bool)` | トリガー（すり抜け）かどうか |
| `bool Is2D() const` | 2D用コライダーかどうか |

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
        GetComponent(velocity);
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
