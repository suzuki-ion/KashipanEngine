# KashipanEngine EditorTool スクリプトリファレンス

エディター拡張（メニュー項目・専用ウィンドウなど）をAngelScriptで追加するための `EditorTool` スクリプトのリファレンスです。ゲームプレイ用の `ScriptComponent`（[ScriptReference.md](ScriptReference.md)）とは**別のAngelScriptエンジンインスタンス**で動作します。

- スクリプト言語: [AngelScript](https://www.angelcode.com/angelscript/)
- 対象: `EditorToolManager`（エディターのみ・`USE_IMGUI` 定義時のみ有効）
- 配置場所: 実行ディレクトリ（`Assets` フォルダと同じ階層）直下の **`EditorTools` フォルダ**（サブフォルダも再帰的に読み込まれる）

---

## 目次

1. [基本的な使い方](#基本的な使い方)
2. [EditorTool（ライフサイクル）](#editorToolライフサイクル)
3. [クラス属性（`[EditorWindow]` / `[MenuItem]`）](#クラス属性editorwindow--menuitem)
4. [ウィンドウ操作用グローバル関数](#ウィンドウ操作用グローバル関数)
5. [ScriptComponentとのAPIの違い](#scriptcomponentとのapiの違い)
6. [ImGui名前空間](#imgui名前空間)
7. [サンプルスクリプト](#サンプルスクリプト)
8. [注意事項](#注意事項)

---

## 基本的な使い方

1. `EditorTools` フォルダ（無ければ作成する。`Assets` フォルダと同じ階層）に `.as` ファイルを置く
2. スクリプト内で `EditorTool` インターフェースを実装したクラスを定義する
3. エディターを起動すると、`EditorTools` フォルダ以下の `.as` が**起動時に一度だけ**自動で読み込まれ、`EditorTool` を実装した全クラスがそれぞれ1つずつインスタンス化される
4. クラスに付けた `[MenuItem(...)]` からメニュー選択時の処理を、`[EditorWindow(...)]` から専用ウィンドウの中身を実装する

`ScriptComponent` の `Reload` ボタンのようなホットリロード機構は無いため、スクリプトを追加・編集した場合はエディターを再起動して反映する。

### VSCodeでのコード補完（as.predefined）

`EditorTools/as.predefined` が自動生成され、VSCodeの **AngelScript Language Server** 拡張機能はこれを読み込んでコード補完・型チェックを行う。`EditorTools` フォルダをVSCodeで開く（またはワークスペースに含める）と補完が有効になる。

ゲームプレイ用の `.as`（`ScriptComponent` 側）は実行ディレクトリ直下の `as.predefined` を参照するため別ファイルであり、内容も異なる（EditorTool専用エンジンには後述の `ImGui::` 名前空間が追加登録されている一方、後述の理由で `GetOwnerObject()` 等は実行時に無効）。

## EditorTool（ライフサイクル）

```angelscript
[EditorWindow("My Tool")]
[MenuItem("MenuBar/Tools/My Tool", "open_my_tool")]
class MyTool : EditorTool {
    void InitializeOnLoad() {}                       // 読み込み直後に一度だけ
    void Update() {}                                  // 毎フレーム（[EditorWindow]がある場合はウィンドウのBegin/Endの中で呼ばれる）
    void OnItemSelected(const string &in tag) {}      // [MenuItem]の項目がクリックされたとき（第2引数のタグが渡される）
    void OnWindowEnable(const string &in windowName) {}  // [EditorWindow]のウィンドウが開いたとき
    void OnWindowDisable(const string &in windowName) {} // [EditorWindow]のウィンドウが閉じたとき
}
```

| メソッド | 呼び出しタイミング |
|---|---|
| `void InitializeOnLoad()` | 全ツールの読み込みが完了した直後に一度だけ |
| `void Update()` | 毎フレーム。クラスが `[EditorWindow(...)]` を持つ場合、対応するウィンドウが開いている間だけ、その `ImGui::Begin`/`End` の中で呼ばれる（閉じている間は呼ばれない）。`[EditorWindow]` を持たないクラスは常に毎フレーム呼ばれる |
| `void OnItemSelected(const string &in tag)` | `[MenuItem(...)]` で追加した項目がクリックされたとき。タグは属性の第2引数（省略時は項目名） |
| `void OnWindowEnable(const string &in windowName)` | `[EditorWindow(...)]` のウィンドウが閉→開へ変化したとき |
| `void OnWindowDisable(const string &in windowName)` | `[EditorWindow(...)]` のウィンドウが開→閉へ変化したとき（右上の×で閉じた場合も含む） |

全メソッドは省略可能。1つの `.as` ファイルに複数の `EditorTool` 実装クラスを定義してもよく、それぞれ独立にインスタンス化される。

## クラス属性（`[EditorWindow]` / `[MenuItem]`）

クラス宣言の直前に付ける。複数付けたい場合は `[EditorWindow("..."), MenuItem("...", "...")]` のようにまとめて書ける。

| 属性 | 引数 | 説明 |
|---|---|---|
| `[EditorWindow("ウィンドウ名")]` | ウィンドウタイトル | このクラス用の `ImGui` ウィンドウを1つ用意する。表示/非表示は [`OpenEditorWindow`](#ウィンドウ操作用グローバル関数) 等のスクリプト側の呼び出しで制御する（クラスを読み込んだだけでは開かない） |
| `[MenuItem("階層パス", "タグ")]` | 階層パスとクリック時に渡すタグ（タグは省略可、省略時は項目名がそのまま渡る） | メニューに項目を追加する。階層パスの先頭は `"MenuBar/..."`（メインメニューバー）または `"Hierarchy/..."`（ヒエラルキーの右クリックメニュー）のいずれか。`/` 区切りの中間部分はサブメニューになる（例: `"MenuBar/Tools/WFC/Tile Editor"` → `Tools` の中に `WFC` サブメニュー → `Tile Editor` 項目） |

同じクラスに `[MenuItem(...)]` を複数付けることもできる（それぞれ別のタグで `OnItemSelected` が呼ばれる）。

## ウィンドウ操作用グローバル関数

`[EditorWindow]` で用意したウィンドウの開閉を制御する（`ScriptComponent` 側にも同じ関数が登録されているが、エディターが無効なビルドでは何もしない）。

| 関数 | 説明 |
|---|---|
| `void OpenEditorWindow(const string &in name)` | 指定した名前の `[EditorWindow]` を開く |
| `void CloseEditorWindow(const string &in name)` | 指定した名前の `[EditorWindow]` を閉じる |
| `bool IsEditorWindowOpen(const string &in name)` | 指定した名前の `[EditorWindow]` が開いているか |

`Update()` 内でウィンドウの中身を描く前に `IsEditorWindowOpen` で開いているか確認するのが基本パターン（[EditorWindow]を持つクラスでは`Update()`自体がウィンドウが開いている間しか呼ばれないため必須ではないが、`[MenuItem]`から複数ウィンドウを開閉する構成にする場合など、明示的に確認したい場面で使う）。

## ScriptComponentとのAPIの違い

EditorToolのエンジンには [ScriptReference.md](ScriptReference.md) に記載の数学型・コンポーネント型・`Json`・`dictionary`・`Math`/`Easing`/`Random` 名前空間・`WaveFunctionCollapse` 等、ほぼ同じエンジンAPIが登録されている。ただし以下の点が異なる。

- **`ObjectContext` が存在しない**: EditorToolはどのオブジェクトにも属さないため、`GetOwnerObject()` / `GetTransform()` / `GetComponent(?&out)` / `FindObject(...)` の**オブジェクト起点**の関数は正しく動作しない（対象が無い扱いになる）。使えるのは `GetScene()` が返す `Scene@` を経由した操作（`GetScene().CreateObject(...)`、`GetScene().GetObjects(name)` 等）のみ。
- **`GetScene()` はエディターで開いている（編集中の）シーン**を返す。ゲーム実行中かどうかに関わらず、エディターの現在のシーンコンテキストが対象になる。
- **`ImGui::` 名前空間が追加登録されている**（後述）。`ScriptComponent` 側では登録されていないため使用できない。
- **ホットリロード無し**: `ScriptComponent` の `Reload` ボタンに相当する機能は無い。スクリプトの追加・変更はエディター再起動で反映する。

## ImGui名前空間

`ImGui::Xxx(...)` として呼び出す。値を書き換えて返す系の関数（`Checkbox`/`DragFloat`/`InputText`等）は、**戻り値を使わず参照引数（`&inout`）を直接書き換える**AngelScript特有の呼び方をする点に注意（例: `ImGui::DragFloat("Speed", speed, 0.1f);` のように呼び、`speed` が直接更新される）。

| 分類 | 関数 |
|---|---|
| テキスト | `void Text(const string &in)` / `void TextColored(const Vector4 &in color, const string &in)` / `void TextWrapped(const string &in)` / `void TextDisabled(const string &in)` / `void BulletText(const string &in)` |
| ボタン等 | `bool Button(const string &in, float width = 0, float height = 0)` / `bool SmallButton(const string &in)` / `bool Checkbox(const string &in, bool &inout value)` / `bool RadioButton(const string &in, bool active)` / `bool Selectable(const string &in, bool selected = false)` |
| 入力 | `bool InputText(const string &in, string &inout text)` / `bool InputTextMultiline(const string &in, string &inout text, float width = 0, float height = 0)` / `bool DragInt(const string &in, int &inout value, float speed = 1.0f, int min = 0, int max = 0)` / `bool DragFloat(const string &in, float &inout value, float speed = 1.0f, float min = 0, float max = 0)` / `bool SliderInt(const string &in, int &inout value, int min, int max)` / `bool SliderFloat(const string &in, float &inout value, float min, float max)` / `bool DragVector2(const string &in, Vector2 &inout value, float speed = 1.0f)` / `bool DragVector3(const string &in, Vector3 &inout value, float speed = 1.0f)` / `bool DragVector4(const string &in, Vector4 &inout value, float speed = 1.0f)` / `bool ColorEdit3(const string &in, Vector3 &inout color)` / `bool ColorEdit4(const string &in, Vector4 &inout color)` / `bool Combo(const string &in, int &inout currentIndex, const array<string> &in items)` / `void ProgressBar(float fraction, const string &in overlay = "")` |
| レイアウト | `void Separator()` / `void SameLine(float offsetX = 0.0f, float spacing = -1.0f)` / `void NewLine()` / `void Spacing()` / `void Indent(float width = 0.0f)` / `void Unindent(float width = 0.0f)` / `void SetNextItemWidth(float width)` / `void PushID(int id)` / `void PushID(const string &in id)` / `void PopID()` / `void BeginDisabled(bool disabled = true)` / `void EndDisabled()` / `bool BeginChild(const string &in id, float width = 0, float height = 0, bool border = false)` / `void EndChild()` |
| ツリー・折りたたみ | `bool TreeNode(const string &in)` / `void TreePop()` / `bool CollapsingHeader(const string &in)` |
| 状態取得・その他 | `bool IsItemHovered()` / `bool IsItemClicked(int button = 0)` / `bool IsItemActive()` / `void SetTooltip(const string &in)` / `Vector2 GetContentRegionAvail()` |
| 図形描画（現在のウィンドウの描画リストへ直接描く。スクリーン座標系） | `Vector2 GetCursorScreenPos()` / `void Dummy(const Vector2 &in size)` / `void DrawLine(const Vector2 &in p1, const Vector2 &in p2, const Vector4 &in color, float thickness = 1.0f)` / `void DrawRect(const Vector2 &in pMin, const Vector2 &in pMax, const Vector4 &in color, float thickness = 1.0f, float rounding = 0.0f)` / `void DrawRectFilled(const Vector2 &in pMin, const Vector2 &in pMax, const Vector4 &in color, float rounding = 0.0f)` / `void DrawCircleFilled(const Vector2 &in center, float radius, const Vector4 &in color, int segments = 0)` / `void DrawText(const Vector2 &in pos, const Vector4 &in color, const string &in text)` |
| マウス入力（自作の直接描画UIをクリック/ドラッグ操作させる） | `Vector2 GetMousePos()` / `bool InvisibleButton(const string &in id, const Vector2 &in size)` |

`Combo` は選択肢の一覧（`array<string>`）と現在のインデックス（`&inout`）を渡す簡易版（内部で`BeginCombo`/`Selectable`/`EndCombo`を組み立てる）。個別の`BeginCombo`/`EndCombo`は登録されていない。

図形描画系は`ImGui::GetCursorScreenPos()`で描画開始位置（スクリーン座標）を取得し、`DrawLine`等で任意の図形を描いた後、実際に描いた領域のサイズを`ImGui::Dummy(size)`に渡してレイアウト領域を確保するのが基本パターン（`Dummy`を呼ばないと後続のウィジェットが描画内容に重なる）。クリック/ドラッグ操作を受け付けたい場合は`Dummy`の代わりに`InvisibleButton(id, size)`を同じ位置へ置く（レイアウト確保を兼ねる透明なボタンになる）。戻り値はクリックされた瞬間だけ`true`、`ImGui::IsItemActive()`はマウスボタンを押している間ずっと`true`になる（カーソルが領域外へ出てもドラッグ中は`true`のまま）ため、`GetMousePos()`と組み合わせて値をドラッグで書き換えるUIが作れる。グラフ・カーブエディター・ミニマップ等の自作UIに使う。実例は[`EditorTools/KeyframeAnimationEditor.as`](../EditorTools/KeyframeAnimationEditor.as)の`ShowCurveSection()`（キーフレームの点をドラッグして値を編集する）を参照。

## サンプルスクリプト

メニューから開く簡単なカウンターウィンドウの例:

```angelscript
[EditorWindow("Counter Tool")]
[MenuItem("MenuBar/Tools/Counter Tool", "open_counter_tool")]
class CounterTool : EditorTool {
    int count = 0;

    void InitializeOnLoad() {
        Log("CounterTool: 読み込まれました (Tools > Counter Tool)");
    }

    void OnItemSelected(const string &in tag) {
        if (tag == "open_counter_tool") {
            OpenEditorWindow("Counter Tool");
        }
    }

    void Update() {
        ImGui::Text("Count: " + count);
        if (ImGui::Button("+1")) {
            count++;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            count = 0;
        }

        ImGui::Separator();
        // GetScene()経由でシーン内のオブジェクトを直接操作することもできる
        if (ImGui::Button("Create Empty Object")) {
            GetScene().CreateObject("EditorCreated");
        }
    }
}
```

ヒエラルキーの右クリックメニューに項目を追加する例:

```angelscript
class HierarchyTool : EditorTool {
    [MenuItem("Hierarchy/Create Marker", "create_marker")]
    void OnItemSelected(const string &in tag) {
        if (tag == "create_marker") {
            GetScene().CreateObject("Marker");
        }
    }
}
```

より実践的な例として、以下の同梱ツールも参照:

- [`EditorTools/WfcTileEditor.as`](../EditorTools/WfcTileEditor.as) — [WaveFunctionCollapse](ScriptReference.md#wavefunctioncollapse波動関数崩壊アルゴリズム)用タイル定義のグリッドエディター（`Json`の保存/読み込み・動的な接続編集を含む）
- [`EditorTools/KeyframeAnimationEditor.as`](../EditorTools/KeyframeAnimationEditor.as) — `KeyFrameAnimator`コンポーネント用のキーフレームjson作成エディター（イージング選択コンボ・評価値プレビューを含む）

## 注意事項

- `EditorTool` を実装するクラスにはデフォルトコンストラクタ（引数無しで呼べるコンストラクタ）が必要（内部でファクトリ関数経由でインスタンス化するため）。
- 1つの `.as` に複数の `EditorTool` 実装クラスを書ける。それぞれ独立に読み込まれ、`[EditorWindow]`/`[MenuItem]` もクラスごとに個別に登録される。
- コンパイルエラーはログにのみ出力される（`ScriptComponent` のようなインスペクター表示は無い）。エディター起動直後のログを確認すること。
- `EditorTools` フォルダが存在しない場合、エディターツールは一切読み込まれない（エラーにはならず、単に無視される）。
