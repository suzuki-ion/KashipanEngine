# 入力（Input）

KashipanEngine の入力システムは、キーボード・マウス・コントローラーの 3 種類のデバイスに対応しています。

- `Input` クラスが生のデバイス状態を管理し、`Keyboard` / `Mouse` / `Controller` の各クラスを保持します。
- `InputCommand` クラスが「アクション名 → 入力バインド」を管理し、複数デバイスの入力を統一的に評価できます。
- `GameEngine` 内で `Input::Update()` が毎フレーム自動的に呼ばれます。

---

## カテゴリ別リファレンス

- `10_Input_Keyboard.md` - キーボード入力（`Keyboard` / `Key`）
- `10_Input_Mouse.md` - マウス入力（`Mouse` / `MouseButton`）
- `10_Input_Controller.md` - コントローラー入力（`Controller` / `ControllerButton`）
- `10_Input_InputCommand.md` - 入力コマンド（`InputCommand`）

---

## `Input` クラス（`KashipanEngine/Input/Input.h`）

`Input` はエンジン内部で生成され、各デバイスクラスへのアクセスを提供します。

- `Keyboard& GetKeyboard()` / `const Keyboard& GetKeyboard() const`
- `Mouse& GetMouse()` / `const Mouse& GetMouse() const`
- `Controller& GetController()` / `const Controller& GetController() const`

### 取得方法

シーン内では `SceneBase::GetInput()` で取得できます。

```cpp
auto* input = KashipanEngine::SceneBase::GetInput();
if (!input) return;

const auto& keyboard = input->GetKeyboard();
const auto& mouse = input->GetMouse();
const auto& controller = input->GetController();
```

> 通常のゲームロジックでは `InputCommand` を使用することを推奨します。`Input` を直接使うのは、`InputCommand` では対応できない特殊な用途向けです。
