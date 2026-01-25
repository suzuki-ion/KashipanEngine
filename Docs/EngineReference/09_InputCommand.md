# `InputCommand` コマンド登録と使い方

対象コード:

- `KashipanEngine/Input/InputCommand.h/.cpp`
- `KashipanEngine/Input/Input.h`（入力状態の提供側）
- `KashipanEngine/Scene/SceneBase.h`（シーンへのポインタ注入）

## 1. 概要

`InputCommand` は「入力デバイス状態 → アクション名（文字列） → 評価結果」に変換する仕組みです。

- アクション名（例: `"Jump"`, `"MoveX"`）に対し複数の Binding を登録できる
- `Evaluate(action)` で `ReturnInfo` を返す
  - `Triggered()` : 条件を満たしたか
  - `Value()` : 入力の値（キー/ボタンは 0/1、アナログは -1～1 など）

## 2. コマンド登録

登録 API（オーバーロード）:

- キーボード
  - `RegisterCommand(action, KeyboardKey key, InputState state, invertValue)`
- マウスボタン
  - `RegisterCommand(action, MouseButton button, InputState state, invertValue)`
- マウス軸
  - `RegisterCommand(action, MouseAxis axis, void* hwnd, float threshold, invertValue)`
  - `hwnd != nullptr` の場合は Client 座標系として扱う設計
- コントローラーボタン
  - `RegisterCommand(action, ControllerButton button, InputState state, controllerIndex, invertValue)`
- コントローラーアナログ
  - `RegisterCommand(action, ControllerAnalog analog, InputState state, controllerIndex, threshold, invertValue)`
- コントローラーアナログ差分（current - previous）
  - `RegisterCommand(action, ControllerAnalog analog, controllerIndex, threshold, invertValue)`

入力状態 `InputState`:

- `Down`
- `Trigger`
- `Release`

登録クリア:

- `Clear()`

## 3. 評価

- `ReturnInfo Evaluate(const std::string &action) const;`

評価は action に紐づく Binding を順に評価して合成して返す設計です（詳細は `.cpp` 実装）。

`ReturnInfo`:

- `bool Triggered() const`
- `float Value() const`

## 4. シーンからの利用

`SceneBase` はエンジン側から

- `InputCommand* sInputCommand`

が注入されます。

派生シーンからは

- `InputCommand* cmd = GetInputCommand();`

で取得して `Evaluate("Action")` する想定です。

## 5. 注意点

- アクション名は文字列キーのため、タイプミス対策はアプリ側で定数化する運用が必要です。
- `threshold` / `invertValue` により、アナログや軸入力の使い勝手を調整できます。
