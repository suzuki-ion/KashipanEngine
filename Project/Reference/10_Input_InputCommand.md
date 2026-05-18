# 入力コマンド（`InputCommand`）

`InputCommand` は、アクション名に対して複数の入力バインドを登録し、統一的に評価できる仕組みです。
キーボード・マウス・コントローラーの入力を 1 つのアクション名にまとめて扱えます。

- `AppInitialize` でコマンドを登録し、ゲーム側は `Evaluate("Action")` で参照する設計です。
- `GameEngine` 内部で生成され、`SceneBase::GetInputCommand()` で取得できます。
- コマンド設定を JSON ファイルに保存・読み込みできます。

関連ヘッダ：
- `KashipanEngine/Input/InputCommand.h`

---

## 公開列挙型

### `InputCommand::InputState`

入力の状態を指定します。

- `Down` — 押されている間（毎フレーム true）
- `Trigger` — 押された瞬間（1 フレームだけ true）
- `Release` — 離された瞬間（1 フレームだけ true）

### `InputCommand::ControllerAnalog`

コントローラーのアナログ入力の種別です。

- `LeftTrigger` — 左トリガー（0.0 ～ 1.0）
- `RightTrigger` — 右トリガー（0.0 ～ 1.0）
- `LeftStickX` — 左スティック X（-1.0 ～ 1.0）
- `LeftStickY` — 左スティック Y（-1.0 ～ 1.0）
- `RightStickX` — 右スティック X（-1.0 ～ 1.0）
- `RightStickY` — 右スティック Y（-1.0 ～ 1.0）

### `InputCommand::MouseAxis`

マウスの軸入力の種別です。

- `X` — X座標（スクリーンまたはクライアント）
- `Y` — Y座標（スクリーンまたはクライアント）
- `DeltaX` — X方向移動量（フレーム差分）
- `DeltaY` — Y方向移動量（フレーム差分）
- `Wheel` — ホイール累積値
- `DeltaWheel` — ホイールフレーム差分

### `InputCommand::MouseSpace`

マウス座標系（内部で自動設定されます）。

- `Screen` — スクリーン座標系（hwnd = nullptr の場合）
- `Client` — クライアント座標系（hwnd を指定した場合）

---

## 公開構造体

### `InputCommand::ReturnInfo`

`Evaluate` の戻り値です。

- `bool Triggered() const noexcept` — 入力が評価条件を満たしたか
- `float Value() const noexcept` — 入力の評価値（-1.0 ～ 1.0）

---

## 公開API

### コマンド管理

- `void Clear()` — すべてのコマンド登録をクリア

### コマンド登録（`RegisterCommand` オーバーロード一覧）

#### キーボード

```cpp
void RegisterCommand(
    const std::string& action,
    Key key,
    InputState state,
    bool invertValue = false);
```

- `key` — `Key::Space` などで直接キーを指定
- `state` — 入力状態（Down / Trigger / Release）
- `invertValue` — `true` にすると `Value()` の符号を反転（例：左移動に -1.0 を割り当て）

#### マウスボタン

```cpp
void RegisterCommand(
    const std::string& action,
    MouseButton button,
    InputState state,
    bool invertValue = false);
```

- `button` — `MouseButton::Left` などで直接ボタンを指定

#### マウス軸

```cpp
void RegisterCommand(
    const std::string& action,
    MouseAxis axis,
    void* hwnd = nullptr,
    float threshold = 0.0f,
    bool invertValue = false);
```

- `axis` — 取得する軸（X / Y / DeltaX / DeltaY / Wheel / DeltaWheel）
- `hwnd` — クライアント座標系で扱う場合の HWND（`nullptr` でスクリーン座標系）
- `threshold` — 絶対値がこの閾値を超えたら `Triggered() = true`

#### コントローラーボタン

```cpp
void RegisterCommand(
    const std::string& action,
    ControllerButton button,
    InputState state,
    int controllerIndex = 0,
    bool invertValue = false);
```

- `button` — `ControllerButton::A` などでボタンを指定
- `controllerIndex` — コントローラー番号（0-3）

#### コントローラーアナログ

```cpp
void RegisterCommand(
    const std::string& action,
    ControllerAnalog analog,
    InputState state,
    int controllerIndex = 0,
    float threshold = 0.0f,
    bool invertValue = false);
```

- `analog` — アナログ入力種別
- `threshold` — 閾値。正の値で正方向判定、負の値で負方向判定
  - `InputState::Down` — 値が閾値を超えている間 true
  - `InputState::Trigger` — 前フレームで閾値以下、今フレームで閾値超過の瞬間 true
  - `InputState::Release` — 前フレームで閾値超過、今フレームで閾値以下の瞬間 true

#### コントローラーアナログ差分

```cpp
void RegisterCommand(
    const std::string& action,
    ControllerAnalog analog,
    int controllerIndex = 0,
    float threshold = 0.0f,
    bool invertValue = false);
```

- current - previous の差分を -1.0 ～ 1.0 相当に正規化して返す
- `threshold` — 差分の絶対値がこの閾値を超えたら `Triggered() = true`

### コマンド評価

```cpp
ReturnInfo Evaluate(const std::string& action) const;
```

- 同じアクション名に複数のバインドが登録されている場合、いずれかが条件を満たせば `Triggered() = true`
- `Value()` は条件を満たした全バインドの値を合算し、-1.0 ～ 1.0 にクランプした値

### JSON 保存・読み込み

#### 保存

```cpp
bool SaveToJSON(const std::string& filepath) const;
```

- 現在登録されているすべてのコマンドを JSON ファイルに書き出す
- 成功時 `true`、失敗時 `false` を返す
- マウス軸の `hwnd`（HWND）は保存できないため、読み込み時は Screen 座標系として復元される

#### 読み込み

```cpp
bool LoadFromJSON(const std::string& filepath);
```

- JSON ファイルからコマンドを読み込んで登録する
- **読み込み前に既存のコマンドはすべてクリアされる**
- 成功時 `true`、失敗時 `false` を返す

### 文字列 ↔ 列挙型変換ユーティリティ

JSON の読み書きやデバッグ用途に使える静的メソッドです。

```cpp
static std::string KeyToString(Key key);
static Key StringToKey(const std::string& str);

static std::string MouseButtonToString(MouseButton button);
static MouseButton StringToMouseButton(const std::string& str);

static std::string ControllerButtonToString(ControllerButton button);
static ControllerButton StringToControllerButton(const std::string& str);

static std::string ControllerAnalogToString(ControllerAnalog analog);
static ControllerAnalog StringToControllerAnalog(const std::string& str);

static std::string InputStateToString(InputState state);
static InputState StringToInputState(const std::string& str);

static std::string MouseAxisToString(MouseAxis axis);
static MouseAxis StringToMouseAxis(const std::string& str);
```

---

## 例：コマンド登録（`AppInitialize` 内）

```cpp
auto* ic = context.inputCommand;
ic->Clear();

// 左右移動（キーボード + コントローラー）
ic->RegisterCommand("MoveX", Key::A, InputCommand::InputState::Down, true);
ic->RegisterCommand("MoveX", Key::D, InputCommand::InputState::Down);
ic->RegisterCommand("MoveX", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down);

// 攻撃（キーボード + コントローラートリガー）
ic->RegisterCommand("Attack", Key::Space, InputCommand::InputState::Release);
ic->RegisterCommand("Attack", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Release);

// マウスボタン
ic->RegisterCommand("Fire", MouseButton::Left, InputCommand::InputState::Trigger);

// マウス軸
ic->RegisterCommand("LookX", InputCommand::MouseAxis::DeltaX);
```

---

## 例：ゲーム側での評価

```cpp
auto* ic = KashipanEngine::SceneBase::GetInputCommand();
if (!ic) return;

// 移動
const auto moveX = ic->Evaluate("MoveX");
if (moveX.Triggered()) {
    float axis = moveX.Value(); // -1.0 ～ 1.0 相当
    // 移動処理
}

// 攻撃
const auto attack = ic->Evaluate("Attack");
if (attack.Triggered()) {
    // 攻撃処理
}

// マウス視点操作
const auto lookX = ic->Evaluate("LookX");
float mouseDX = lookX.Value();
```

---

## `invertValue` の使い方

`invertValue = true` を指定すると、`Value()` の符号が反転します。これにより、同じアクション名に対して正方向と負方向のキーを割り当てられます。

```cpp
// A キー → Value = -1.0（左方向）
ic->RegisterCommand("MoveX", Key::A, InputCommand::InputState::Down, true);
// D キー → Value = +1.0（右方向）
ic->RegisterCommand("MoveX", Key::D, InputCommand::InputState::Down, false);
```

---

## マウス座標系（Screen / Client）

`RegisterCommand(action, MouseAxis, hwnd, ...)` で座標系を制御できます。

- `hwnd == nullptr` — スクリーン座標系（`MouseSpace::Screen`）
- `hwnd != nullptr` — クライアント座標系（`MouseSpace::Client`）

クライアント座標系の場合、指定した HWND のクライアント領域に対する座標が返されます。

---

## JSON 形式

`SaveToJSON` で出力される JSON の形式です。各アクション名をキーとし、バインド配列を値とするオブジェクトになります。

```json
{
    "MoveLeft": [
        {
            "device": "Keyboard",
            "key": "A",
            "state": "Trigger",
            "invertValue": false
        },
        {
            "device": "ControllerButton",
            "button": "DPadLeft",
            "state": "Trigger",
            "controllerIndex": 0,
            "invertValue": false
        }
    ],
    "Attack": [
        {
            "device": "MouseButton",
            "button": "Left",
            "state": "Trigger",
            "invertValue": false
        }
    ],
    "LookX": [
        {
            "device": "MouseAxis",
            "axis": "DeltaX",
            "threshold": 0.0,
            "invertValue": false
        }
    ],
    "Accelerate": [
        {
            "device": "ControllerAnalog",
            "analog": "RightTrigger",
            "state": "Down",
            "controllerIndex": 0,
            "threshold": 0.1,
            "invertValue": false
        }
    ],
    "LookDeltaX": [
        {
            "device": "ControllerAnalogDelta",
            "analog": "RightStickX",
            "controllerIndex": 0,
            "threshold": 0.05,
            "invertValue": false
        }
    ]
}
```

### デバイス別フィールド一覧

| device | 必須フィールド | オプション |
|---|---|---|
| `Keyboard` | `key`, `state` | `invertValue` |
| `MouseButton` | `button`, `state` | `invertValue` |
| `MouseAxis` | `axis` | `threshold`, `invertValue` |
| `ControllerButton` | `button`, `state` | `controllerIndex`, `invertValue` |
| `ControllerAnalog` | `analog`, `state` | `controllerIndex`, `threshold`, `invertValue` |
| `ControllerAnalogDelta` | `analog` | `controllerIndex`, `threshold`, `invertValue` |

### 例：JSON からの読み込み

```cpp
auto* ic = context.inputCommand;
if (!ic->LoadFromJSON("Assets/InputBindings.json")) {
    // 読み込み失敗時のフォールバック
    ic->Clear();
    ic->RegisterCommand("Submit", Key::Enter, InputCommand::InputState::Trigger);
}
```

### 例：JSON への保存

```cpp
auto* ic = KashipanEngine::SceneBase::GetInputCommand();
if (ic) {
    ic->SaveToJSON("Assets/InputBindings.json");
}
