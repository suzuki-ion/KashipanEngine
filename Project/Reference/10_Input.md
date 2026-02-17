# 入力（キーボード/マウス/コントローラー）

KashipanEngine は `Input` が生のデバイス状態を管理し、`InputCommand` が「アクション名 → 入力バインド」を管理します。

- `GameEngine` 内で `Input::Update()` が毎フレーム呼ばれます。
- アプリ側は `AppInitialize` で `InputCommand` にコマンドを登録し、ゲーム側は `InputCommand::Evaluate("Action")` で参照する想定です。

関連：
- `KashipanEngine/Input/Input.h`
- `KashipanEngine/Input/InputCommand.h`

---

## 公開API：`InputCommand`（抜粋）

- `void Clear()`
- `void RegisterCommand(...)`（複数オーバーロード）
  - キーボード：`RegisterCommand(action, KeyboardKey{Key::...}, InputState, invertValue)`
  - マウスボタン：`RegisterCommand(action, MouseButton{0..7}, InputState, invertValue)`
  - マウス軸：`RegisterCommand(action, MouseAxis, hwnd, threshold, invertValue)`
  - パッドボタン：`RegisterCommand(action, ControllerButton, InputState, controllerIndex, invertValue)`
  - パッドアナログ：`RegisterCommand(action, ControllerAnalog, InputState, controllerIndex, threshold, invertValue)`
  - パッドアナログ差分：`RegisterCommand(action, ControllerAnalog, controllerIndex, threshold, invertValue)`
- `ReturnInfo Evaluate(const std::string& action) const`

### `ReturnInfo`
- `bool Triggered() const noexcept`
- `float Value() const noexcept`

---

## 例：コマンド登録（`Application/AppInitialize.h` の流儀）

```cpp
ic->Clear();

ic->RegisterCommand("MoveX", InputCommand::KeyboardKey{ Key::A }, InputCommand::InputState::Down, true);
ic->RegisterCommand("MoveX", InputCommand::KeyboardKey{ Key::D }, InputCommand::InputState::Down);
ic->RegisterCommand("MoveX", InputCommand::ControllerAnalog::LeftStickX, InputCommand::InputState::Down);

ic->RegisterCommand("Attack", InputCommand::KeyboardKey{ Key::Space }, InputCommand::InputState::Release);
ic->RegisterCommand("Attack", InputCommand::ControllerAnalog::RightTrigger, InputCommand::InputState::Release);
```

---

## 例：ゲーム側で評価

```cpp
auto* ic = KashipanEngine::SceneBase::GetInputCommand();
if (!ic) return;

const auto moveX = ic->Evaluate("MoveX");
if (moveX.Triggered()) {
    float axis = moveX.Value(); // -1..1 相当
    // 移動処理
}

const auto attack = ic->Evaluate("Attack");
if (attack.Triggered()) {
    // 攻撃
}
```

---

## マウス座標（Screen / Client）

`RegisterCommand(action, MouseAxis axis, void* hwnd, ...)` で
- `hwnd == nullptr`：Screen 座標系
- `hwnd != nullptr`：Client 座標系

として扱われます。
