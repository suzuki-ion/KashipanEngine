# コントローラー入力（`Controller` / `ControllerButton`）

コントローラー入力は `Controller` クラスで管理されます。`Input::GetController()` で取得できます。
GameInput API を使用しており、XInput 互換のゲームパッドに対応しています。

関連ヘッダ：
- `KashipanEngine/Input/Controller.h`
- `KashipanEngine/Input/ControllerButton.h`

---

## 公開API：`Controller`（`KashipanEngine/Input/Controller.h`）

### 接続状態

- `int GetPadCount() const noexcept` — 把握しているゲームパッド数を取得
- `bool IsConnected(int index) const` — 指定インデックスのコントローラーが接続されているか
- `bool WasConnected(int index) const` — 前フレームで接続されていたか

### ボタン入力

- `bool IsButtonDown(ControllerButton button, int index) const` — 指定ボタンが押されているか
- `bool IsButtonTrigger(ControllerButton button, int index) const` — 指定ボタンが押された瞬間か（トリガー）
- `bool IsButtonRelease(ControllerButton button, int index) const` — 指定ボタンが離された瞬間か（リリース）
- `bool WasButtonDown(ControllerButton button, int index) const` — 前フレームで押されていたか

### アナログ入力（現在フレーム）

- `int GetLeftTrigger(int index) const` — 左トリガー押し込み量（0-255）
- `int GetRightTrigger(int index) const` — 右トリガー押し込み量（0-255）
- `int GetLeftStickX(int index) const` — 左スティックX（-32767 ～ 32767）
- `int GetLeftStickY(int index) const` — 左スティックY（-32767 ～ 32767）
- `int GetRightStickX(int index) const` — 右スティックX（-32767 ～ 32767）
- `int GetRightStickY(int index) const` — 右スティックY（-32767 ～ 32767）

### アナログ入力（前フレーム）

- `int GetPrevLeftTrigger(int index) const`
- `int GetPrevRightTrigger(int index) const`
- `int GetPrevLeftStickX(int index) const`
- `int GetPrevLeftStickY(int index) const`
- `int GetPrevRightStickX(int index) const`
- `int GetPrevRightStickY(int index) const`

### アナログ入力（フレーム差分）

- `int GetDeltaLeftTrigger(int index) const` — 左トリガーのフレーム差分（current - previous）
- `int GetDeltaRightTrigger(int index) const` — 右トリガーのフレーム差分
- `int GetDeltaLeftStickX(int index) const` — 左スティックXのフレーム差分
- `int GetDeltaLeftStickY(int index) const` — 左スティックYのフレーム差分
- `int GetDeltaRightStickX(int index) const` — 右スティックXのフレーム差分
- `int GetDeltaRightStickY(int index) const` — 右スティックYのフレーム差分

### 振動

- `void SetVibration(int index, int leftMotor, int rightMotor)` — 振動を設定（0 ～ 65535）
- `void StopVibration(int index)` — 振動を停止

### パッド状態の直接取得

- `std::span<const PadState> GetPads() const noexcept` — 現在フレームのパッド状態一覧
- `std::span<const PadState> GetPrevPads() const noexcept` — 前フレームのパッド状態一覧

#### `PadState` 構造体

```cpp
struct PadState {
    std::uint16_t buttons = 0;       // XINPUT_GAMEPAD 互換のボタンマスク
    std::uint8_t  leftTrigger = 0;   // 0-255
    std::uint8_t  rightTrigger = 0;  // 0-255
    std::int16_t  leftX = 0;         // -32767 ～ 32767
    std::int16_t  leftY = 0;
    std::int16_t  rightX = 0;
    std::int16_t  rightY = 0;
};
```

---

## `ControllerButton` 列挙型（`KashipanEngine/Input/ControllerButton.h`）

XInput 互換のボタンマスク値を持つ列挙型です。

### 方向パッド

- `ControllerButton::DPadUp` — 十字キー上（0x0001）
- `ControllerButton::DPadDown` — 十字キー下（0x0002）
- `ControllerButton::DPadLeft` — 十字キー左（0x0004）
- `ControllerButton::DPadRight` — 十字キー右（0x0008）

### システムボタン

- `ControllerButton::Start` — スタートボタン（0x0010）
- `ControllerButton::Back` — バックボタン（0x0020）

### スティック押し込み

- `ControllerButton::LeftThumb` — 左スティック押し込み（0x0040）
- `ControllerButton::RightThumb` — 右スティック押し込み（0x0080）

### ショルダーボタン

- `ControllerButton::LeftShoulder` — 左ショルダー / LB（0x0100）
- `ControllerButton::RightShoulder` — 右ショルダー / RB（0x0200）

### フェイスボタン

- `ControllerButton::A` — Aボタン（0x1000）
- `ControllerButton::B` — Bボタン（0x2000）
- `ControllerButton::X` — Xボタン（0x4000）
- `ControllerButton::Y` — Yボタン（0x8000）

### ユーティリティ関数

- `constexpr std::uint16_t ToMask(ControllerButton b) noexcept` — ボタンをマスク値に変換

---

## 例：コントローラー入力の直接取得

```cpp
auto* input = KashipanEngine::SceneBase::GetInput();
if (!input) return;

const auto& ct = input->GetController();

if (!ct.IsConnected(0)) return; // パッド0が未接続

// Aボタンのトリガー判定
if (ct.IsButtonTrigger(ControllerButton::A, 0)) {
    // Aボタンが押された瞬間
}

// 左スティックの値を正規化して取得
float lx = static_cast<float>(ct.GetLeftStickX(0)) / 32767.0f;
float ly = static_cast<float>(ct.GetLeftStickY(0)) / 32767.0f;

// 右トリガーの値を正規化して取得
float rt = static_cast<float>(ct.GetRightTrigger(0)) / 255.0f;

// 振動
ct.SetVibration(0, 30000, 30000);
```

---

## 例：`InputCommand` でのコントローラー登録

```cpp
auto* ic = context.inputCommand;

// ボタン
ic->RegisterCommand("Jump", ControllerButton::A, InputCommand::InputState::Trigger);
ic->RegisterCommand("Start", ControllerButton::Start, InputCommand::InputState::Trigger);

// アナログ（スティック）
ic->RegisterCommand("MoveX",
    InputCommand::ControllerAnalog::LeftStickX,
    InputCommand::InputState::Down);

// アナログ（トリガー）閾値付き
ic->RegisterCommand("Accelerate",
    InputCommand::ControllerAnalog::RightTrigger,
    InputCommand::InputState::Down, 0, 0.1f);

// アナログ差分（スティックの変化量）
ic->RegisterCommand("LookDeltaX",
    InputCommand::ControllerAnalog::RightStickX,
    0, 0.05f);
```

> 詳しくは `10_Input_InputCommand.md` を参照してください。
