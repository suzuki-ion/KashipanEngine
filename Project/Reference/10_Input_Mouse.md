# マウス入力（`Mouse` / `MouseButton`）

マウス入力は `Mouse` クラスで管理されます。`Input::GetMouse()` で取得できます。

関連ヘッダ：
- `KashipanEngine/Input/Mouse.h`
- `KashipanEngine/Input/MouseButton.h`

---

## 公開API：`Mouse`（`KashipanEngine/Input/Mouse.h`）

### ボタン入力

- `bool IsButtonDown(int button) const` — 指定マウスボタンが押されているか
- `bool IsButtonTrigger(int button) const` — 指定マウスボタンが押された瞬間か（トリガー）
- `bool IsButtonRelease(int button) const` — 指定マウスボタンが離された瞬間か（リリース）
- `bool WasButtonDown(int button) const` — 前フレームで指定マウスボタンが押されていたか

`button` は `MouseButton` 列挙型の値を `static_cast<int>` して渡すか、直接整数値（0-7）を指定します。

### 移動量（フレーム差分）

- `int GetDeltaX() const` — X方向の移動量
- `int GetDeltaY() const` — Y方向の移動量
- `int GetPrevDeltaX() const` — 前フレームのX方向移動量
- `int GetPrevDeltaY() const` — 前フレームのY方向移動量

### ホイール

- `int GetWheel() const` — ホイールのフレーム差分（縦方向）
- `int GetWheelValue() const` — ホイール累積値（縦方向）
- `int GetPrevWheel() const` — 前フレームのホイール差分
- `int GetPrevWheelValue() const` — 前フレームのホイール累積値

### カーソル座標

`HWND` を指定するとクライアント座標系、`nullptr` を指定するとスクリーン座標系で取得します。`Window*` を直接渡すオーバーロードもあります。

- `POINT GetPos(HWND hwnd) const` — マウス座標を取得
- `POINT GetPrevPos(HWND hwnd) const` — 前フレームのマウス座標を取得
- `int GetX(HWND hwnd) const` / `int GetY(HWND hwnd) const` — X/Y座標を個別取得
- `int GetPrevX(HWND hwnd) const` / `int GetPrevY(HWND hwnd) const` — 前フレームのX/Y座標を個別取得
- `POINT GetPos(const Window* window) const` — Window指定で座標を取得
- `POINT GetPrevPos(const Window* window) const` — Window指定で前フレーム座標を取得
- `int GetX(const Window* window) const` / `int GetY(const Window* window) const`
- `int GetPrevX(const Window* window) const` / `int GetPrevY(const Window* window) const`

---

## `MouseButton` 列挙型（`KashipanEngine/Input/MouseButton.h`）

マウスボタンを表す列挙型です。GameInput API のボタンビットインデックスに対応しています。

- `MouseButton::Left` — 左ボタン（0）
- `MouseButton::Right` — 右ボタン（1）
- `MouseButton::Middle` — 中ボタン / ホイールクリック（2）
- `MouseButton::Button4` — サイドボタン4（3）
- `MouseButton::Button5` — サイドボタン5（4）
- `MouseButton::Button6` — ボタン6（5）
- `MouseButton::Button7` — ボタン7（6）
- `MouseButton::Button8` — ボタン8（7）

---

## 例：マウス入力の直接取得

```cpp
auto* input = KashipanEngine::SceneBase::GetInput();
if (!input) return;

const auto& mouse = input->GetMouse();

// 左クリック判定
if (mouse.IsButtonTrigger(static_cast<int>(MouseButton::Left))) {
    // 左ボタンが押された瞬間
}

// マウス移動量の取得
int dx = mouse.GetDeltaX();
int dy = mouse.GetDeltaY();

// クライアント座標系でのカーソル位置
POINT pos = mouse.GetPos(windowHandle);
```

---

## 例：`InputCommand` でのマウスボタン登録

```cpp
auto* ic = context.inputCommand;

// 左クリック
ic->RegisterCommand("Attack", MouseButton::Left, InputCommand::InputState::Trigger);

// 右クリック
ic->RegisterCommand("Block", MouseButton::Right, InputCommand::InputState::Down);
```

---

## 例：`InputCommand` でのマウス軸登録

```cpp
auto* ic = context.inputCommand;

// マウス移動量X（Screen座標系）
ic->RegisterCommand("LookX",
    InputCommand::MouseAxis::DeltaX,
    nullptr, 0.0f);

// マウスホイール差分
ic->RegisterCommand("Zoom",
    InputCommand::MouseAxis::DeltaWheel,
    nullptr, 0.0f);

// クライアント座標系で取得（hwnd を指定）
ic->RegisterCommand("CursorX",
    InputCommand::MouseAxis::X,
    hwnd, 0.0f);
```

> 詳しくは `10_Input_InputCommand.md` を参照してください。
