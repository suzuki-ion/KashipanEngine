# オフスクリーン（`ScreenBuffer` / `ShadowMapBuffer`）

KashipanEngine では、
- `ScreenBuffer`：オフスクリーン用カラー＋深度（ポストエフェクトも対応）
- `ShadowMapBuffer`：シャドウマップ用（深度中心）

を使用します。

---

## 描画の前提（必ず必要になるオブジェクト）

`ScreenBuffer` は内部で DirectX リソースやコマンド記録を行うため、エンジン初期化時に以下が設定されます。

- `ScreenBuffer::SetDirectXCommon(Passkey<GameEngine>, DirectXCommon*)`
- `ScreenBuffer::SetRenderer(Passkey<GameEngine>, Renderer*)`

これは `GameEngine` が行うため、アプリ側は通常 **`Create` して使うだけ** でOKです。

同様に `ShadowMapBuffer` も `GameEngine` が `DirectXCommon` を設定します。

---

## `ScreenBuffer` 公開API（抜粋）

ヘッダ：`KashipanEngine/Graphics/ScreenBuffer.h`

### 生成/破棄
- `static ScreenBuffer* Create(std::uint32_t width, std::uint32_t height, DXGI_FORMAT colorFormat = ..., DXGI_FORMAT depthFormat = ...)`
- `static void DestroyNotify(ScreenBuffer* buffer)`
- `static void CommitDestroy(Passkey<GameEngine>)`
- `static void AllDestroy(Passkey<GameEngine>)`

### 情報取得
- `std::uint32_t GetWidth() const` / `std::uint32_t GetHeight() const`
- `D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const`（カラー SRV）
- `D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSrvHandle() const`（深度 SRV。未作成なら空）
- `RenderTargetResource* GetRenderTarget() const`
- `DepthStencilResource* GetDepthStencil() const`

### ポストエフェクト
- `bool RegisterPostEffectComponent(std::unique_ptr<IPostEffectComponent> component)`
- `const std::vector<std::unique_ptr<IPostEffectComponent>>& GetPostEffectComponents() const`
- `void InvalidatePostEffectPasses()`（キャッシュ無効化）

---

## ポストエフェクト（`IPostEffectComponent`）

ポストエフェクトは `ScreenBuffer` に「コンポーネント」として登録し、
コンポーネントが `PostEffectPass` を生成して描画パイプラインに組み込まれます。

- インターフェース：`KashipanEngine/Graphics/PostEffectComponents/IPostEffectComponent.h`
- 設計：
  - `BuildPostEffectPasses()` が、このコンポーネントが提供する `PostEffectPass` の配列を返す
  - `ScreenBuffer` はそれをキャッシュし、必要に応じて `InvalidatePostEffectPasses()` で再構築する

### `IPostEffectComponent` 公開API（抜粋）
- `const std::string& GetComponentType() const`
- `size_t GetMaxComponentCountPerBuffer() const`
- `virtual std::optional<bool> Initialize()` / `Finalize()`
- `virtual std::vector<PostEffectPass> BuildPostEffectPasses() const`
- `virtual std::unique_ptr<IPostEffectComponent> Clone() const = 0`

エンジン標準のポストエフェクト実装例：
- `BloomEffect`
- `FXAAEffect`
- `MotionBlurEffect`
- `ChromaticAberrationEffect`
- `DitherEffect`
- `DotMatrixEffect`

### 例：ポストエフェクトを追加する

```cpp
#include <KashipanEngine.h>

auto* sb = ScreenBuffer::Create(1280, 720);

// 例：FXAA（実際のコンストラクタ引数は各クラス定義に従ってください）
// sb->RegisterPostEffectComponent(std::make_unique<KashipanEngine::FXAAEffect>());

sb->InvalidatePostEffectPasses();
```

---

## 例：`ScreenBuffer` を作ってオブジェクトの描画先にする

```cpp
// オフスクリーン生成
auto* sb = ScreenBuffer::Create(1280, 720);

// 例：2Dオブジェクトを ScreenBuffer へ描画
obj2d->AttachToRenderer(sb, "Object2D.Solid.BlendNormal");

// 破棄は通知してフレーム終端でコミットされる
ScreenBuffer::DestroyNotify(sb);
```

---

## `ShadowMapBuffer`

`ShadowMapBuffer` は `GameEngine` が `DirectXCommon` を設定し、フレーム終端で `CommitDestroy` される管理方式です。

- 参照：`KashipanEngine/Graphics/ShadowMapBuffer.h` / `.cpp`
- デバッグ表示：`ShadowMapBuffer::ShowImGuiShadowMapBuffersWindow()`（`USE_IMGUI` 時）

`Object3DBase` は `AttachToRenderer(ShadowMapBuffer*, ...)` を持ち、シャドウマップ向けの永続パス登録に対応します。
