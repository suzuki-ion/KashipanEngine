# ウィンドウの作成/削除

対象コード:

- `KashipanEngine/Core/Window.h/.cpp`
- `KashipanEngine/Core/WindowsAPI.*`（内部実装側）
- `KashipanEngine/Core/GameEngine.*`（生成/更新の呼び出し側）

## 1. ウィンドウの生成

`Window` は静的生成 API を提供します。

- 通常ウィンドウ
  - `Window::CreateNormal(title, width, height, style, iconPath)`
- オーバーレイウィンドウ
  - `Window::CreateOverlay(title, width, height, clickThrough, iconPath)`

引数を省略した場合は、`Window` が保持するデフォルト値が使われます。

デフォルト値は `GameEngine` 側から

- `Window::SetDefaultParams(Passkey<GameEngine>, title, width, height, style, iconPath)`

で設定される想定です。

また、内部利用のためにエンジンから以下が注入されます。

- `Window::SetWindowsAPI(Passkey<GameEngine>, WindowsAPI*)`
- `Window::SetDirectXCommon(Passkey<GameEngine>, DirectXCommon*)`
- `Window::SetPipelineManager(Passkey<GraphicsEngine>, PipelineManager*)`
- `Window::SetRenderer(Passkey<GraphicsEngine>, Renderer*)`

## 2. 毎フレーム更新

エンジンのゲームループ側から、全ウィンドウに対して以下が呼ばれる想定です。

- `Window::Update(Passkey<GameEngine>)`
  - メッセージ処理・状態更新
- `Window::Draw(Passkey<GameEngine>)`
  - ウィンドウの描画（SwapChain 等）

## 3. 削除（破棄）の流れ

`Window` は「破棄要求 → フレーム終端で破棄反映」という設計です。

- 破棄要求
  - `window->DestroyNotify()`
- 破棄反映
  - `Window::CommitDestroy(Passkey<GameEngine>)`

全破棄:

- `Window::AllDestroy(Passkey<GameEngine>)`

## 4. 存在確認と取得

- `Window::GetWindow(HWND hwnd)`
- `Window::GetWindow(const std::string &title)`
- `Window::GetWindows(const std::string &title)`

存在確認:

- `Window::IsExist(HWND hwnd)`
- `Window::IsExist(Window *window)`
- `Window::IsExist(const std::string &title)`

数:

- `Window::GetWindowCount()`

## 5. 親子関係

`Window` は親子関係を保持できます。

- `SetWindowParent(...)` / `ClearWindowParent(...)`
- `SetWindowChild(...)` / `ClearWindowChild(...)`

第2引数 `applyNative` により `SetParent`（WinAPI）を実際に適用するかを制御できます。

## 6. イベント登録

`Window` にはメッセージイベントの登録 API があり、

- 既定イベント（`WindowDefaultEvent::*`）は値として `variant` に保持
- ユーザー拡張イベントは `std::unique_ptr<IWindowEvent>` として保持

します。

主な API:

- `RegisterWindowEvent<TEvent>(...)`
- `UnregisterWindowEvent(UINT msg)`

※ 既定イベント型は `Core/WindowsAPI/WindowEvents/DefaultEvents.h` にあります。
