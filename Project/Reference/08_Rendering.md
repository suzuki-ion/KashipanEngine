# 描画に必要なオブジェクト（`Renderer` / RenderPass / Pipeline）

KashipanEngine の描画は `Renderer` が中心で、オブジェクトやオフスクリーンが **永続レンダーパス** を登録することでフレームごとに描画されます。

- `GameEngine` → `GraphicsEngine` → `Renderer::RenderFrame(...)`
- `Object2DBase` / `Object3DBase` → `AttachToRenderer(...)` で `Renderer` にパス登録
- `ScreenBuffer` も Renderer に登録し、オフスクリーン描画＋ポストエフェクトを駆動

関連：`KashipanEngine/Graphics/Renderer.h`

---

## 描画をする上で必ず必要になるオブジェクト

エンジン内部（`GameEngine`）で生成・接続され、ユーザーコードは通常「使うだけ」の前提になっている依存関係です。

- `WindowsAPI`：ウィンドウ生成/イベント配送
- `DirectXCommon`：device / command queue / swap chain など DirectX 共通
- `GraphicsEngine`：描画サブシステムの統括
- `Renderer`：フレーム描画（パスのスケジューリング、バッファ確保、Draw 発行）
- `PipelineManager`：`pipelineName` に対応するパイプラインを解決
- `Window`：最終的な出力先（swap chain）
- オフスクリーン：`ScreenBuffer` / `ShadowMapBuffer`

また、`GameEngine` 初期化時に以下が行われます。

- `Object2DBase::SetRenderer(Passkey<GameEngine>, Renderer*)`
- `Object3DBase::SetRenderer(Passkey<GameEngine>, Renderer*)`
- `ScreenBuffer::SetDirectXCommon(Passkey<GameEngine>, DirectXCommon*)`
- `ScreenBuffer::SetRenderer(Passkey<GameEngine>, Renderer*)`
- `ShadowMapBuffer::SetDirectXCommon(Passkey<GameEngine>, DirectXCommon*)`

これにより、利用側は `Object*::AttachToRenderer(...)` や `ScreenBuffer::Create(...)` を呼ぶだけで描画に参加できます。

---

## 公開API（`Renderer`）

### 永続パス登録/解除
- `PersistentPassHandle RegisterPersistentRenderPass(RenderPass&& pass)`
- `bool UnregisterPersistentRenderPass(PersistentPassHandle handle)`

### ScreenBuffer 向け
- `PersistentScreenPassHandle RegisterPersistentScreenPass(ScreenBufferPass&& pass)`
- `bool UnregisterPersistentScreenPass(PersistentScreenPassHandle handle)`

### ScreenBuffer 宛て RenderPass
- `PersistentOffscreenPassHandle RegisterPersistentOffscreenRenderPass(RenderPass&& pass)`
- `bool UnregisterPersistentOffscreenRenderPass(PersistentOffscreenPassHandle handle)`

### ShadowMapBuffer 宛て RenderPass
- `PersistentShadowMapPassHandle RegisterPersistentShadowMapRenderPass(RenderPass&& pass)`
- `bool UnregisterPersistentShadowMapRenderPass(PersistentShadowMapPassHandle handle)`

### フレーム描画
- `void RenderFrame(Passkey<GraphicsEngine>)`

---

## `RenderPass`（オブジェクトが登録する描画単位）

`RenderPass` は内部構造体ですが、`Object*Base` から登録される内容として以下が重要です。

- 描画先：`Window*` / `ScreenBuffer*` / `ShadowMapBuffer*`
- `pipelineName`：使用するパイプライン名
- `renderType`：`RenderType::Standard` or `RenderType::Instancing`
- `batchKey`：インスタンシングのバッチング識別
- 定数/インスタンスバッファ要件
  - `ConstantBufferRequirement { shaderNameKey, byteSize }`
  - `InstanceBufferRequirement { shaderNameKey, elementStride }`
- コールバック
  - `updateConstantBuffersFunction`
  - `submitInstanceFunction`
  - `batchedRenderFunction`（共通バインド）
  - `renderCommandFunction`（DrawCall を生成）

---

## 利用者が普段触る入口：`Object2DBase` / `Object3DBase`

通常、利用者は `Renderer` を直接触るのではなく、
- `Object2DBase::AttachToRenderer(...)`
- `Object3DBase::AttachToRenderer(...)`

でパス登録します。

### 例：2D を Window に描画
```cpp
obj2d->AttachToRenderer(Window::GetWindow("3104_Noisend"), "Object2D.Solid.BlendNormal");
```

### 例：3D を ScreenBuffer に描画
```cpp
auto* sb3d = ScreenBuffer::Create(1280, 720);
obj3d->AttachToRenderer(sb3d, "Object3D.Solid.BlendNormal");
```

---

## ImGui（プロファイル）

`USE_IMGUI` 有効時、`Renderer` は CPU timer 統計を保持し、デバッグウィンドウを表示できます。

- `const CpuTimerStats& GetCpuTimerStats() const noexcept`
- `void SetCpuTimerAverageWindow(std::uint32_t frames) noexcept`
- `void ShowImGuiCpuTimersWindow()`

また `GameEngine` 側でも Update/Draw/FPS のプロファイル UI が表示されます。
