# KashipanEngine

C++ / DirectX12 ベースの自作ゲームエンジン。エディター内蔵、AngelScriptによるスクリプト拡張、単一の実行ファイルが「エディター」と「各プロジェクトのランタイム」を兼ねる構成になっている（`KashipanEngine.exe --project <名前>` で起動プロジェクトを切り替える）。

## ソリューション構成 (`KashipanEngine.sln`)

| プロジェクト | ファイル | 種別 | 役割 |
|---|---|---|---|
| `KashipanEngine` | `KashipanEngine.vcxproj` | Application | `main.cpp` + `KashipanEngine/` 全体。エンジン兼エディター本体のexe |
| `KashipanHub` | `KashipanHub.vcxproj` | Application | プロジェクト一覧/作成/起動だけを行う別exe。`Launcher/`が実体 |
| `DirectXTex` | `Externals/DirectXTex/...` | 静的ライブラリ | ベンダーライブラリ |
| `angelscript` | `Externals/angelscript/...` | 静的ライブラリ | ベンダーライブラリ |

`KashipanEngine.vcxproj`は`ConfigurationType=Application`であり、静的/動的ライブラリとしてゲーム側exeにリンクする構成ではない。ゲーム固有コードは`Application/AppInitialize.h`とAngelScriptスクリプトで差し込む。

## トップレベルディレクトリ

**エンジン本体・ツール（コード）**
- `KashipanEngine/` — エンジンのソースツリー本体。詳細は下記。
- `Launcher/` — `KashipanHub.exe`のソース。WebView2ベースのプロジェクトランチャーUI。
- `EditorTools/` — C++コードではなく、実行中のエディターが自動読み込みするAngelScript(`.as`)拡張スクリプト集（`[EditorWindow]`/`[MenuItem]`属性でツールウィンドウ/メニューを追加する仕組み）。
- `Tools/` — リポジトリ整備用PowerShellスクリプト（`Externals`配下のベンダーバイナリ取得など）。実行時には使われない。
- `MyStd/` — ヘッダオンリーの自作コンテナ/ユーティリティ（`AnyVector`, `NameMap`, `VectorMap`等）。エンジン全体からインクルードされる。
- `Externals/` — サードパーティ/ベンダー依存（Assimp, DirectXTex, ReactPhysics3D, WebView2, angelscript, imgui, nlohmann-json, utf8等）。`Externals/Generated/`はビルド出力先。

**アセット・テンプレート・プロジェクト**
- `AssetsTemplate/` — 新規プロジェクト作成時にコピーされるテンプレート（`Default/`, `Empty/`等）。**詳細と重要な注意点は後述**。
- `Projects/` — 実際のゲームプロジェクト群（例: `GravityRunner`, `JobHuntingGame`, `Test`）。各プロジェクトは`Projects/<名前>/Assets/`配下に独自コピーの資産一式を持つ。
- `Locales/` — エンジングローバルの翻訳JSON（プロジェクトを開く前に読み込まれる）。

**ドキュメント**
- `Reference/` — 手書きの静的HTMLドキュメントサイト（生成物ではない）。`Reference/Editor/Components/*.html`はコンポーネントヘッダと対応させて手動更新する運用。

**実行時生成物・一時ファイル（コード探索時は基本無視してよい）**
- `Dumps/`（クラッシュダンプ）, `Logs/`（ログ）, `SceneBackups/`（自動保存/クラッシュ復元）, `UserSettings/`（エディター個人設定）, `AS_DEBUG/`（AngelScriptデバッグ出力）, `.vs/`

## `KashipanEngine/` 内部構成

- **`Core/`** — プラットフォーム/起動層。`WindowsAPI`, `Window`, `DirectXCommon`, `GameEngine`（トップレベルエンジンクラス）, `ProjectManager`/`ProjectPaths`（マルチプロジェクト管理）, `UserSettings`。
  - `Core/DirectX/` — D3D12低レベルラッパー（`DX12Device`, `DX12CommandQueue`, `DX12SwapChain`, `DescriptorHeaps`等）。
- **`Graphics/`** — レンダラー。
  - `Graphics/Pipeline/` — JSON駆動のパイプライン/シェーダーバリアントシステム。`Pipeline/JsonParser/`（BlendState/RasterizerState/RootSignature等の型付きパーサー、`AssetsTemplate`側のPipeline JSONスキーマと対応）、`Pipeline/System/`（`PipelineCreator`, `PipelineVariantResolver`, `ShaderCompiler`等）。
  - `Graphics/Renderer/` — 毎フレームの描画処理（`RendererDraw/Compute/Lighting/PostProcess/Shadow`に分割）。
  - `Graphics/Resources/` — GPUリソースラッパー（`ConstantBufferResource`, `VertexBufferResource`等、`IGraphicsResource`実装）。
  - 直下: `GraphicsEngine`, `PipelineManager`（パイプラインJSONアセットのロード/保持）, `ScreenBuffer`, `ShadowMapBuffer`等。
- **`Objects/`** — ECS風のゲームオブジェクト/コンポーネントシステム。
  - `Objects/Components/` — 各種コンポーネント本体。`Collider/`（Box/Sphere/Capsule/Mesh/RigidBody2D3D）、`Compute/`（コンピュートシェーダー処理）、`PostProcessing/`（Bloom/AO/DoF/FXAA/MotionBlur等のポストエフェクト実装）、`Render/`（Camera2D/3D, MeshRenderer, SpriteRenderer, TextRenderer, SkinnedMeshRenderer, Light等 — `Reference/Editor/Components/*`とほぼ1:1対応）。
  - `Objects/Collision/` — 狭域衝突判定の数学的処理（ReactPhysics3Dベースの`RigidBody`コンポーネントとは別）。
  - 直下: `EmptyObject`（基底オブジェクト型）, `IObjectComponent`, `ComponentPool`/`ChunkedPool`（コンポーネント格納/プーリング）。
- **`Scene/`** — シーングラフ、シリアライズ、エディターUI。
  - `Scene/Components/` — シーン単位のコンポーネント。`Script/`にAngelScript統合（`SceneScriptEngine`, `EditorToolManager`等、`EditorTools/*.as`の読み込み元）。
  - `Scene/Editor/` — ImGuiベースのシーンエディターUI全般（階層/インスペクター/アセットウィンドウ/プレハブシステム/`SceneCrashRecovery`等）。
  - 直下: `Scene`, `SceneManager`, `SceneFileIO`（シーンJSON永続化）。
- **`Math/`** — コア数学型（`Vector2/3/4`, `Matrix3x3/4x4`, `Quaternion`, `Color`）。
  - 注意: `Utilities/MathUtils/`にも類似のVector/Matrix型が別途存在する（重複あり、用途により使い分け）。
- **`Input/`** — `Keyboard`/`Mouse`/`Controller`/`InputCommand`（`Assets/InputCommand.json`で定義するバインド可能な入力アクション）。
- **`Assets/`** — アセット種別ごとのマネージャー（`TextureManager`, `ModelManager`, `MaterialManager`, `AudioManager`, `VideoManager`等）。
- **`Debug/`** — `Logger`（`Logs/`へ出力）, `CrashHandler`（`Dumps/`へダンプ出力、`SceneBackups/`へクラッシュ復元用シーン出力）, `ImGuiManager`。
- **`EngineSettings/`** — `EngineSettings.json`の各セクション読み込み（Window/Rendering/Limits/Translations）。
- **`ComponentSerialize/`** — `ComponentRegistry`（コンポーネント名↔型の登録、シーン/プレハブJSONやAddComponentメニューで使用）。
- **`Utilities/`** — 汎用ヘルパー群（`FileIO`, `Conversion`, `Dialogs`, `Plugin`, `MathUtils`, `Translation`, `RandomValue`, 手続き生成系(`WaveFunctionCollapse`等)）。
- **`Splash/`** — 起動時スプラッシュ画面（WebView2レンダリング、`Launcher/`と同系統のUI実装パターン）。

## AssetsTemplate と Projects の関係（重要な運用ルール）

`ProjectManager`のドキュメントコメントの通り、新規プロジェクト作成時に`AssetsTemplate/<テンプレート名>/`の中身がまるごと`Projects/<プロジェクト名>/Assets/`へコピーされる。これにより各プロジェクトは、パイプライン定義(`Pipeline/Pipelines/*.json`)・マテリアル・シーン・フォント等の**独立したコピー**を持つ。

**⚠️ `AssetsTemplate/`を編集しても、既存プロジェクト（`Projects/`配下）には自動反映されない。** 新規作成されるプロジェクトにのみ反映される。

そのため、**パイプライン関連（`AssetsTemplate/Default/KashipanEngine/Pipeline/`配下のJSON、シェーダー等）に変更を加えた場合は、`Projects/`配下の既存プロジェクトが持つ対応ファイルにも同じ変更を反映する必要がある。** パイプライン変更を行うタスクでは、`AssetsTemplate`側の修正だけで完了とせず、`Projects/*/Assets/KashipanEngine/Pipeline/`配下の対応ファイルも探して同期すること。

## Launcher / EditorTools / Tools の違い（紛らわしいので明記）

- **`Launcher/`＋`KashipanHub.vcxproj`** — 別exe（`KashipanHub.exe`）。プロジェクトの一覧表示・新規作成・起動のみを行い、選択後は`KashipanEngine.exe --project <名前>`を呼び出す。
- **`EditorTools/`** — exeではない。実行中のエディターが自動読み込みするAngelScript拡張スクリプト（C++再コンパイル無しでエディターツールウィンドウ/メニューを追加する仕組み）。
- **`Tools/`** — 実行時には無関係。リポジトリ/開発環境整備用のPowerShellスクリプト（ベンダーライブラリのダウンロード等）。
