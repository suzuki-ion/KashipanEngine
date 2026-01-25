# シーン

対象コード:

- `KashipanEngine/Scene/SceneBase.h/.cpp`
- `KashipanEngine/Scene/SceneManager.h/.cpp`
- `KashipanEngine/Scene/SceneContext.h/.cpp`
- `KashipanEngine/Core/GameEngine.h/.cpp`

## 1. シーンの基本

シーンは `SceneBase` を継承して作成します。

- `SceneBase(const std::string &sceneName)` を呼び、シーン名を設定します。
- `SceneBase::Update()` がシーンの 1 フレーム分の更新処理。
  - `objects2D_` / `objects3D_` の `Update()`
  - `sceneComponents_` を `GetUpdatePriority()` で安定ソートして `Update()`
  - 最後に `OnUpdate()`（派生クラス側の追加更新）

## 2. 派生シーン側で実装する主な関数

`SceneBase` の仮想関数:

- `Initialize()`
  - `SceneManager` から呼ばれる想定の初期化
- `Finalize()`
  - `SceneManager` から呼ばれる想定の終了
- `OnUpdate()`
  - 毎フレーム更新の拡張ポイント

※ `SceneBase::~SceneBase()` では

- `ClearSceneComponents()`
- `ClearObjects2D()`
- `ClearObjects3D()`

が実行されます。

## 3. シーンの登録と切り替え（`SceneManager`）

`SceneManager` は `GameEngine` が所有し、シーン生成ファクトリを保持します。

- 登録: `SceneManager::RegisterScene<TScene>(sceneName, args...)`
  - `TScene` は `SceneBase` 派生
  - 生成時に `scene->SetSceneManager(Passkey<SceneManager>{}, sm);` が呼ばれます

- 取得: `SceneManager::GetCurrentScene()`
- 切り替え:
  - `ChangeScene(sceneName)`
  - `CommitPendingSceneChange(Passkey<GameEngine>)`（ゲームループ側から呼ばれる想定）

## 4. シーンが最初から持っている他クラスへのポインタ（Engine pointers）

`SceneBase` は静的ポインタとして、エンジン側から注入される参照を保持します。

`SceneBase::SetEnginePointers(...)` で設定されるもの（`SceneBase.h/.cpp`）:

- `AudioManager*`（`sAudioManager`）
- `ModelManager*`（`sModelManager`）
- `SamplerManager*`（`sSamplerManager`）
- `TextureManager*`（`sTextureManager`）
- `Input*`（`sInput`）
- `InputCommand*`（`sInputCommand`）

派生シーンからは protected static getter で取得します。

- `GetAudioManager()`
- `GetModelManager()`
- `GetSamplerManager()`
- `GetTextureManager()`
- `GetInput()`
- `GetInputCommand()`

## 5. シーン変数（Scene variables）

`SceneManager` は `std::any` ベースの変数マップを保持します。

- `SceneBase::AddSceneVariable(key, value)` → `SceneManager::AddSceneVariable`
- `SceneBase::GetSceneVariables()` → `SceneManager::GetSceneVariables()`

`SceneBase` には便利関数があります。

- `TryGetSceneVariable<T>(key, out)`
- `GetSceneVariableOr<T>(key, defaultValue)`

## 6. オブジェクトの保持と操作

`SceneBase` は以下を保持します。

- `std::vector<std::unique_ptr<Object2DBase>> objects2D_`
- `std::vector<std::unique_ptr<Object3DBase>> objects3D_`

追加・削除 API:

- `AddObject2D(std::unique_ptr<Object2DBase> obj)`
- `AddObject3D(std::unique_ptr<Object3DBase> obj)`
- `RemoveObject2D(Object2DBase *obj)`
- `RemoveObject3D(Object3DBase *obj)`

## 7. 次シーン遷移（Scene 内部から）

- `SetNextSceneName(next)`
- `ChangeToNextScene()`
  - `sceneManager_` が設定済みかつ `nextSceneName_` が空でない場合に `sceneManager_->ChangeScene(nextSceneName_)` を呼びます。
- `ClearNextSceneName()`

## 8. SceneContext（コンポーネント用）

`SceneBase` は内部に `std::unique_ptr<SceneContext> sceneContext_` を保持します。

`sceneContext_` は `SceneComponent` の OwnerContext として渡され、

- コンポーネント取得
- オブジェクト追加/削除

などの API を提供します（詳細は「シーンコンポーネント」ドキュメント参照）。
