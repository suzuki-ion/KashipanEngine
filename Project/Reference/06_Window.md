# ウィンドウ（`Window`）

`Window` は Win32 ウィンドウと swap chain を管理し、エンジンの更新・描画ループから毎フレーム更新されます。

関連：`KashipanEngine/Core/Window.h`

---

## 公開API（抜粋）

### 生成/破棄
- `static Window* CreateNormal(const std::string& title = "", int32_t width = 0, int32_t height = 0, DWORD style = 0, const std::string& iconPath = ...)`
- `static Window* CreateOverlay(const std::string& title = "", int32_t width = 0, int32_t height = 0, bool clickThrough = false, const std::string& iconPath = ...)`
- `static void AllDestroy(Passkey<GameEngine>)`
- `void DestroyNotify()`（個別）
- `static void CommitDestroy(Passkey<GameEngine>)`

### 検索
- `static Window* GetWindow(HWND hwnd)`
- `static Window* GetWindow(const std::string& title)`
- `static std::vector<Window*> GetWindows(const std::string& title)`
- `static size_t GetWindowCount()`

### 更新/描画（エンジン側が呼ぶ）
- `static void Update(Passkey<GameEngine>)`
- `static void Draw(Passkey<GameEngine>)`

### 状態・設定
- `void SetSizeChangeMode(SizeChangeMode)`
- `void SetWindowMode(WindowMode)`
- `void SetWindowTitle(const std::string&)` / `void SetWindowTitle(const std::wstring&)`
- `void SetWindowSize(int32_t width, int32_t height)`
- `void SetWindowPosition(int32_t x, int32_t y)`
- `void SetWindowVisible(bool visible)`
- 親子
  - `void SetWindowParent(...)` / `void SetWindowChild(...)`
  - `void ClearWindowParent(...)` / `void ClearWindowChild(...)`

### 情報取得
- `WindowType GetWindowType() const`
- `WindowMode GetWindowMode() const`
- `SizeChangeMode GetSizeChangeMode() const`
- `const std::string& GetWindowTitle() const`
- `HWND GetWindowHandle() const`
- `int32_t GetClientWidth() const` / `int32_t GetClientHeight() const`
- `float GetAspectRatio() const`

---

## ウィンドウイベント（`IWindowEvent` / DefaultEvents）

`Window` は Win32 メッセージ（`WM_*`）に対して「イベントハンドラ」を登録できます。

- インターフェース：`KashipanEngine/Core/WindowsAPI/WindowEvents/IWindowEvent.h`
  - `IWindowEvent(UINT targetMessage)`（対象メッセージを指定）
  - `std::optional<LRESULT> OnEvent(UINT msg, WPARAM wparam, LPARAM lparam)`（処理。未処理なら `std::nullopt`）
  - `std::unique_ptr<IWindowEvent> Clone()`（内部複製用）
- 既定イベント群：`KashipanEngine/Core/WindowsAPI/WindowEvents/DefaultEvents.h`
  - `WindowDefaultEvent::ActivateEvent`（`WM_ACTIVATE`）
  - `WindowDefaultEvent::ClickThroughEvent`（`WM_NCHITTEST` で `HTTRANSPARENT` を返す）
  - `WindowDefaultEvent::CloseEvent`（`WM_CLOSE`）
  - `WindowDefaultEvent::DestroyEvent`（`WM_DESTROY`）
  - `WindowDefaultEvent::SizeEvent`（`WM_SIZE`）
  - `WindowDefaultEvent::SizingEvent`（`WM_SIZING`）
  - `WindowDefaultEvent::SysCommandCloseEvent`（`WM_SYSCOMMAND` の `SC_CLOSE` 捕捉）

### 登録API
- 既定イベント（値として保持）
  - `template<class TEvent, class... Args> void RegisterWindowEvent(Args&&... args)`
- 既定イベント（`unique_ptr` をムーブして値保持）
  - `template<class TEvent> void RegisterWindowEvent(std::unique_ptr<TEvent> handler)`
- 拡張イベント（`unique_ptr<IWindowEvent>` として保持）
  - `template<class TEvent> void RegisterWindowEvent(std::unique_ptr<TEvent> handler)`
  - `template<class TEvent, class... Args> void RegisterWindowEvent(Args&&... args)`
- 解除
  - `void UnregisterWindowEvent(UINT msg)`

### 例：クリック透過の切り替え（Overlay Window）

```cpp
auto* w = Window::GetWindow("3104_Noisend");
if (w) {
    // WM_NCHITTEST 用の既定イベントを登録
    w->RegisterWindowEvent<KashipanEngine::WindowDefaultEvent::ClickThroughEvent>(true);
}
```

---

## 例：ウィンドウ作成

```cpp
Window::CreateOverlay("3104_Noisend", 1280, 720, true);
Window::CreateNormal("Tool Window", 640, 480);
```

---

## 例：タイトル変更とフルスクリーン切替

```cpp
auto* w = Window::GetWindow("3104_Noisend");
if (w) {
    w->SetWindowTitle("My Game");
    w->SetWindowMode(WindowMode::FullScreen);
}
