# キーボード入力（`Keyboard` / `Key`）

キーボード入力は `Keyboard` クラスで管理されます。`Input::GetKeyboard()` で取得できます。

関連ヘッダ：
- `KashipanEngine/Input/Keyboard.h`
- `KashipanEngine/Input/Key.h`

---

## 公開API：`Keyboard`（`KashipanEngine/Input/Keyboard.h`）

- `bool IsDown(Key key) const` — 指定キーが押されているか
- `bool IsTrigger(Key key) const` — 指定キーが押された瞬間か（トリガー）
- `bool IsRelease(Key key) const` — 指定キーが離された瞬間か（リリース）
- `bool WasDown(Key key) const` — 前フレームで指定キーが押されていたか

---

## `Key` 列挙型（`KashipanEngine/Input/Key.h`）

キーボードのキーを表す列挙型です。

### アルファベット

- `Key::A` ～ `Key::Z`

### 数字

- `Key::D0` ～ `Key::D9`

### 矢印キー

- `Key::Left`, `Key::Right`, `Key::Up`, `Key::Down`

### 修飾キー（個別）

- `Key::LeftShift`, `Key::RightShift`
- `Key::LeftControl`, `Key::RightControl`
- `Key::LeftAlt`, `Key::RightAlt`

### 修飾キー（汎用）

左右を区別しない汎用修飾キーです。

- `Key::Shift`, `Key::Control`, `Key::Alt`

### 共通キー

- `Key::Space`, `Key::Enter`, `Key::Escape`, `Key::Tab`, `Key::Backspace`

### ファンクションキー

- `Key::F1` ～ `Key::F12`

### その他

- `Key::Unknown` — 不明なキー（デフォルト値）

---

## 例：キーボード入力の直接取得

```cpp
auto* input = KashipanEngine::SceneBase::GetInput();
if (!input) return;

const auto& kb = input->GetKeyboard();

if (kb.IsTrigger(Key::Space)) {
    // スペースキーが押された瞬間
}

if (kb.IsDown(Key::W)) {
    // Wキーが押されている間
}
```

---

## 例：`InputCommand` でのキーボード登録

```cpp
auto* ic = context.inputCommand;
ic->RegisterCommand("Jump", Key::Space, InputCommand::InputState::Trigger);
ic->RegisterCommand("MoveUp", Key::W, InputCommand::InputState::Down);
```

> 詳しくは `10_Input_InputCommand.md` を参照してください。
