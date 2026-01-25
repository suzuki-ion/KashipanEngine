# `KashipanEngine/Graphics/ScreenBuffer`

対象コード:

- `KashipanEngine/Graphics/ScreenBuffer.h/.cpp`
- `KashipanEngine/Graphics/Renderer.h/.cpp`

## 1. 概要

`ScreenBuffer` は **オフスクリーンレンダリング用のバッファ** です。

- `IShaderTexture` を実装しており、SRV としてシェーダへ渡せます。
- 内部では **ping-pong（2 枚）** の RenderTarget を持ちます。
  - 今フレーム write 面に描画 → 終了時に SRV 状態へ遷移
  - 次フレームは read 面として参照

## 2. 生成と破棄

### 生成

- `ScreenBuffer::SetDirectXCommon(Passkey<GameEngine>, DirectXCommon*)`
  - 生成前にエンジンが注入する必要がある

- `ScreenBuffer::Create(width, height, colorFormat, depthFormat)`
  - 成功すると `ScreenBuffer*` を返します（所有は内部マップ）

### 破棄

`Window` と同様に「破棄要求 → フレーム終端 commit」の設計です。

- `ScreenBuffer::DestroyNotify(ScreenBuffer *buffer)`
- `ScreenBuffer::CommitDestroy(Passkey<GameEngine>)`

全破棄:

- `ScreenBuffer::AllDestroy(Passkey<GameEngine>)`

存在確認:

- `ScreenBuffer::IsExist(ScreenBuffer *buffer)`
- `ScreenBuffer::IsPendingDestroy(ScreenBuffer *buffer)`

## 3. Renderer との連携（記録開始/終了）

`Renderer` のオフスクリーン描画で、全 `ScreenBuffer` に対してコマンド記録を行います。

- `ScreenBuffer::AllBeginRecord(Passkey<Renderer>)`
  - 各バッファで `AdvanceFrameBufferIndex()` を行い write 面を切替
  - `BeginRecord()` して RT/DS の遷移・クリア・viewport 設定などを実行

- `ScreenBuffer::AllEndRecord(Passkey<Renderer>) -> std::vector<ID3D12CommandList*>`
  - `EndRecord()` で write 面を SRV 参照可能へ遷移
  - `Close()` した command list を返す

破棄予定バッファは記録対象外になります。

## 4. SRV としての利用

`ScreenBuffer` は `IShaderTexture` として

- `GetSrvHandle()`
- `GetWidth()` / `GetHeight()`

を提供します。

また内部リソースアクセスとして

- `GetRenderTarget()`（read 面）
- `GetDepthStencil()`（write 面）
- `GetShaderResource()`（read 面）

があります。

## 5. ポストエフェクト

`ScreenBuffer` は `IPostEffectComponent` を登録できます。

- `RegisterPostEffectComponent(std::unique_ptr<IPostEffectComponent>)`
  - `SetOwnerBuffer(this)`
  - `Initialize()`
  - `GetApplyPriority()` により stable_sort

描画実行（`RenderBatched`）:

1. 各コンポーネントの `BindShaderVariables(&binder)`
2. 各コンポーネントの `Apply()`

## 6. Renderer へのアタッチ

`ScreenBuffer` 自体を「画面合成（screen pass）」として Renderer に登録できます。

- `AttachToRenderer(pipelineName, passName)`
  - `Renderer::RegisterPersistentScreenPass(ScreenBufferPass)`
- `DetachFromRenderer()`

`ScreenBufferPass` は `Renderer.h` に定義されています。

## 7. デバッグ（ImGui）

`USE_IMGUI` 有効時:

- `ScreenBuffer::ShowImGuiScreenBuffersWindow()`
  - 生成済み ScreenBuffer の一覧と SRV のビューアを表示
