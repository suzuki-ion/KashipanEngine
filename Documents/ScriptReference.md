# KashipanEngine スクリプトリファレンス

KashipanEngineに組み込まれたAngelScriptの利用方法と、スクリプトから使用できるAPIのリファレンスです。

- スクリプト言語: [AngelScript](https://www.angelcode.com/angelscript/)
- 対象コンポーネント: `ScriptComponent`（オブジェクトコンポーネント） / `SceneScriptEngine`（シーンコンポーネント）

---

## 目次

1. [基本的な使い方](#基本的な使い方)
2. [ScriptComponentBehavior（ライフサイクル）](#scriptcomponentbehaviorライフサイクル)
3. [SerializeField（変数のインスペクター編集・保存）](#serializefield変数のインスペクター編集保存)
4. [コンポーネントの取得（GetComponent / GetComponents）](#コンポーネントの取得getcomponent--getcomponents)
5. [グローバル関数](#グローバル関数)
6. [オブジェクト・シーン型](#オブジェクトシーン型)
7. [コンポーネント型](#コンポーネント型)
8. [数学型](#数学型)
9. [Math名前空間](#math名前空間)
10. [サンプルスクリプト](#サンプルスクリプト)
11. [注意事項](#注意事項)

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
| `void SetNextSceneName(const string &in)` | 次のシーン名を設定する |
| `bool ChangeToNextScene()` | 次のシーンへ切り替える |
| `bool HasNextSceneName() const` | 次のシーン名が設定されているかを取得する |
| `void ClearNextSceneName()` | 次のシーン名をクリアする |

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

### コライダー（BoxCollider / SphereCollider / CapsuleCollider / MeshCollider / RayCollider / Box2DCollider / Circle2DCollider / Capsule2DCollider / Ray2DCollider）

共通メソッドに加えて以下を持ちます。

| メソッド | 説明 |
|---|---|
| `bool IsTrigger() const` / `void SetTrigger(bool)` | トリガー（すり抜け）かどうか |
| `bool Is2D() const` | 2D用コライダーかどうか |

### その他のコンポーネント

以下の型は共通メソッドのみで登録されています（`GetComponent` での取得・アクティブ切り替えが可能）。

`MeshFilter` / `Animator` / `Text` / `ComputeShaderProcessing` / `RigidBody2D` / `RigidBody3D` / `MeshRenderer` / `SkinnedMeshRenderer` / `Camera2D` / `CameraRenderer` / `CameraController` / `Light` / `LightRenderer` / `NormalWindowObject` / `OverlayWindowObject` / `ScreenBufferObject` / `ShadowMapObject` / `BloomEffect` / `BoxFilterEffect` / `ChromaticAberrationEffect` / `ColorAdjustEffect` / `DissolveEffect` / `DitherEffect` / `DotMatrixEffect` / `FXAAEffect` / `GaussianFilterEffect` / `GrayscaleEffect` / `OutlineEffect` / `RadialBlurEffect` / `VignetteEffect`

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
