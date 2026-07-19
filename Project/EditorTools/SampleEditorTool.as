// エディターツールのサンプルスクリプト。
// 実行ディレクトリ直下の "EditorTools" フォルダに置いた .as は、エディター起動時に自動で読み込まれる。
//
// - [EditorWindow("ウィンドウ名")] : ImGuiウィンドウが用意される（開くのはスクリプト側から行う）
// - [MenuItem("表示階層", "タグ")] : "MenuBar/" でメニューバー、"Hierarchy/" でヒエラルキーの
//   右クリックメニューへ項目が追加され、選択時に OnItemSelected(タグ) が呼ばれる
[EditorWindow("Sample Tool Window")]
[MenuItem("MenuBar/Tools/Toggle Sample Window", "toggle_window")]
[MenuItem("Hierarchy/Create Object From Tool", "create_object")]
class SampleEditorTool : EditorTool {
    uint updateCount = 0;

    // 読み込み直後に一度だけ呼ばれる
    void InitializeOnLoad() {
        Log("SampleEditorTool: 読み込まれました");
    }

    // [MenuItem]で追加した項目が選択されたときに呼ばれる（tagは有効化された項目のタグ）
    void OnItemSelected(const string &in tag) {
        if (tag == "toggle_window") {
            if (IsEditorWindowOpen("Sample Tool Window")) {
                CloseEditorWindow("Sample Tool Window");
            } else {
                OpenEditorWindow("Sample Tool Window");
            }
        } else if (tag == "create_object") {
            Scene@ scene = GetScene();
            if (scene !is null) {
                scene.CreateObject("CreatedByEditorTool");
                Log("SampleEditorTool: オブジェクトを生成しました");
            }
        }
    }

    // [EditorWindow]で用意したウィンドウが開かれたときに呼ばれる（windowNameはウィンドウの名前）
    void OnWindowEnable(const string &in windowName) {
        Log("SampleEditorTool: ウィンドウが開きました: " + windowName);
    }

    // [EditorWindow]で用意したウィンドウが閉じられたときに呼ばれる
    void OnWindowDisable(const string &in windowName) {
        Log("SampleEditorTool: ウィンドウが閉じました: " + windowName);
    }

    string objectName = "CreatedByEditorTool";
    int createCount = 1;

    // 毎フレーム呼ばれる（ウィンドウが開いている間は、そのウィンドウのBegin/Endの中で呼ばれる。
    // その間に ImGui:: の関数を呼ぶと、このツールのウィンドウへUIが描画される）
    void Update() {
        updateCount++;
        if (!IsEditorWindowOpen("Sample Tool Window")) return;

        ImGui::Text("Update回数: " + updateCount);
        ImGui::Separator();
        ImGui::InputText("Object Name", objectName);
        ImGui::SliderInt("Count", createCount, 1, 10);
        if (ImGui::Button("Create Objects")) {
            Scene@ scene = GetScene();
            if (scene !is null) {
                for (int i = 0; i < createCount; i++) {
                    scene.CreateObject(objectName);
                }
                Log("SampleEditorTool: " + createCount + "個のオブジェクトを生成しました");
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("指定した名前のオブジェクトをシーンへ生成する");
        }
    }
}
