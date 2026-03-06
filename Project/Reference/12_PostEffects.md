# ポストエフェクト（`IPostEffectComponent`）

ポストエフェクトは `ScreenBuffer` に登録する「コンポーネント」です。
コンポーネントが `PostEffectPass`（パイプライン名、定数/インスタンス要件、描画コマンド生成など）を構築し、
`Renderer` がそれらを実行します。

関連：
- `KashipanEngine/Graphics/ScreenBuffer.h`
- `KashipanEngine/Graphics/PostEffectComponents/IPostEffectComponent.h`
- `KashipanEngine/Graphics/PostEffectComponents/*Effect.h`

---

## 公開API：`IPostEffectComponent`（抜粋）

- `const std::string& GetComponentType() const`
- `size_t GetMaxComponentCountPerBuffer() const`
- `virtual std::optional<bool> Initialize()` / `Finalize()`
- `virtual std::vector<PostEffectPass> BuildPostEffectPasses() const`
- `virtual std::unique_ptr<IPostEffectComponent> Clone() const = 0`

---

## エンジン標準の実装

`KashipanEngine/Graphics/PostEffectComponents/` に以下が存在します（代表例）。

- `BloomEffect`
- `FXAAEffect`
- `MotionBlurEffect`
- `ChromaticAberrationEffect`
- `DitherEffect`
- `DotMatrixEffect`

各エフェクトのパラメータや使用する `pipelineName` は各 `*Effect.h/.cpp` を参照してください。

---

## 例：ScreenBuffer に登録する

```cpp
#include <KashipanEngine.h>

auto* sb = KashipanEngine::ScreenBuffer::Create(1280, 720);

// 例：エフェクト追加（実際のコンストラクタ引数は各クラス定義に従う）
// sb->RegisterPostEffectComponent(std::make_unique<KashipanEngine::BloomEffect>());
// sb->RegisterPostEffectComponent(std::make_unique<KashipanEngine::FXAAEffect>());

// パスキャッシュを無効化（動的に構成を変える場合）
sb->InvalidatePostEffectPasses();
```

---

## 補足：パスの実行タイミング

- `Renderer::RenderFrame(...)` の中で
  - offscreen 記録
  - post effect 記録
  - window への persistent pass

の順で処理されます（詳細は `Renderer.cpp` を参照）。
