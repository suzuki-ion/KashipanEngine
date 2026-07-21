// キーフレームアニメーション（KeyframeAnimation）のjson作成エディターツール。
//
// メニューバーの Tools > Keyframe Animation Editor からウィンドウを開き、
// 「時刻・値・イージング」のキー列を編集して、KeyframeAnimation::LoadFromJson()
// （= KeyFrameAnimatorコンポーネントのJson Path）がそのまま読み込める形式の
// jsonとして任意のパスへ保存できる。
//
// 使い方:
//   1. "Add Key" でキーを追加し、各キーの Time / Value / Ease を編集する
//      （Easeは「そのキーから次のキーへ遷移する際のイージング」。最後のキーでは未使用）
//   2. Preview のスライダーで時刻を動かすと、その時刻の評価値を確認できる
//   3. Save Path を指定して "Save" で保存（保存時に時刻昇順へソートされる）、"Load" で読み込み
//
// 保存したjsonは、オブジェクトに付けた KeyFrameAnimator コンポーネントの
// アニメーションエントリの Json Path へ指定して使う。

/// @brief キー1つ分の編集データ
class KeyframeDef {
    float time = 0.0f;
    float value = 0.0f;
    int easeIndex = 0; // kEaseNames のインデックス（enum EaseType の値と同順）

    KeyframeDef() {}
    KeyframeDef(float t, float v, int e) { time = t; value = v; easeIndex = e; }
}

[EditorWindow("Keyframe Animation Editor")]
[MenuItem("MenuBar/Tools/Keyframe Animation Editor", "open_keyframe_editor")]
class KeyframeAnimationEditor : EditorTool {
    array<KeyframeDef@> keys;
    string savePath = "Assets/Application/KeyframeAnimation.json";
    string statusMessage = "";
    float previewTime = 0.0f;

    // EaseType 列挙型の名前一覧（C++側の EaseTypeToString/StringToEaseType と同じ表記・同じ順序）
    array<string> easeNames = {
        "Linear",
        "EaseInQuad", "EaseOutQuad", "EaseInOutQuad", "EaseOutInQuad",
        "EaseInCubic", "EaseOutCubic", "EaseInOutCubic", "EaseOutInCubic",
        "EaseInQuart", "EaseOutQuart", "EaseInOutQuart", "EaseOutInQuart",
        "EaseInQuint", "EaseOutQuint", "EaseInOutQuint", "EaseOutInQuint",
        "EaseInSine", "EaseOutSine", "EaseInOutSine", "EaseOutInSine",
        "EaseInExpo", "EaseOutExpo", "EaseInOutExpo", "EaseOutInExpo",
        "EaseInCirc", "EaseOutCirc", "EaseInOutCirc", "EaseOutInCirc",
        "EaseInBack", "EaseOutBack", "EaseInOutBack", "EaseOutInBack",
        "EaseInElastic", "EaseOutElastic", "EaseInOutElastic", "EaseOutInElastic",
        "EaseInBounce", "EaseOutBounce", "EaseInOutBounce", "EaseOutInBounce"
    };

    void InitializeOnLoad() {
        Log("KeyframeAnimationEditor: 読み込まれました (Tools > Keyframe Animation Editor)");
    }

    void OnItemSelected(const string &in tag) {
        if (tag == "open_keyframe_editor") {
            OpenEditorWindow("Keyframe Animation Editor");
        }
    }

    void OnWindowEnable(const string &in windowName) {}
    void OnWindowDisable(const string &in windowName) {}

    void Update() {
        ShowSaveLoadSection();
        ImGui::Separator();
        ShowKeysSection();
        ImGui::Separator();
        ShowPreviewSection();
    }

    //==================================================
    // 保存・読み込み
    //==================================================

    void ShowSaveLoadSection() {
        ImGui::InputText("Save Path", savePath);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("保存/読み込みするjsonのパス（実行ディレクトリ基準の相対パスまたは絶対パス）。\nKeyFrameAnimatorコンポーネントのJson Pathへこのパスを指定して使う");
        }
        if (ImGui::Button("Save")) {
            SaveToFile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            LoadFromFile();
        }
        if (statusMessage != "") {
            ImGui::TextDisabled(statusMessage);
        }
    }

    void SortKeysByTime() {
        // キー数は多くないため単純な挿入ソート（安定）で時刻昇順に並べる
        for (uint i = 1; i < keys.length(); i++) {
            KeyframeDef@ key = keys[i];
            int j = int(i) - 1;
            while (j >= 0 && keys[j].time > key.time) {
                @keys[j + 1] = keys[j];
                j--;
            }
            @keys[j + 1] = key;
        }
    }

    void SaveToFile() {
        SortKeysByTime();
        Json@ json = Json();
        Json@ keysJson = Json();
        for (uint i = 0; i < keys.length(); i++) {
            Json@ keyJson = Json();
            keyJson.SetFloat("time", keys[i].time);
            keyJson.SetFloat("value", keys[i].value);
            keyJson.SetString("ease", easeNames[keys[i].easeIndex]);
            keysJson.PushJson(keyJson);
        }
        json.SetJson("keyframes", keysJson);

        if (SaveJsonFile(savePath, json)) {
            statusMessage = "保存しました: " + savePath;
        } else {
            statusMessage = "保存に失敗しました: " + savePath;
        }
    }

    void LoadFromFile() {
        Json@ json = LoadJsonFile(savePath);
        if (json is null) {
            statusMessage = "読み込みに失敗しました: " + savePath;
            return;
        }

        keys.resize(0);
        Json@ keysJson = json.GetJson("keyframes");
        if (keysJson !is null) {
            for (uint i = 0; i < keysJson.Size(); i++) {
                Json@ keyJson = keysJson.At(i);
                if (keyJson is null) continue;
                KeyframeDef@ key = KeyframeDef();
                key.time = float(keyJson.GetFloat("time", 0.0f));
                key.value = float(keyJson.GetFloat("value", 0.0f));
                key.easeIndex = FindEaseIndex(keyJson.GetString("ease", "Linear"));
                keys.insertLast(key);
            }
        }
        SortKeysByTime();
        statusMessage = "読み込みました: " + savePath + " (キー数: " + keys.length() + ")";
    }

    int FindEaseIndex(const string &in name) {
        for (uint i = 0; i < easeNames.length(); i++) {
            if (easeNames[i] == name) return int(i);
        }
        return 0; // 不明な名前はLinear扱い
    }

    //==================================================
    // キー一覧の編集
    //==================================================

    void ShowKeysSection() {
        ImGui::Text("Keyframes (" + keys.length() + ")");
        ImGui::SameLine();
        if (ImGui::SmallButton("Add Key")) {
            // 最後のキーの1秒後・同じ値を初期値として追加する
            float newTime = 0.0f;
            float newValue = 0.0f;
            if (keys.length() > 0) {
                KeyframeDef@ last = keys[keys.length() - 1];
                newTime = last.time + 1.0f;
                newValue = last.value;
            }
            keys.insertLast(KeyframeDef(newTime, newValue, 0));
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Sort By Time")) {
            SortKeysByTime();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("キーを時刻昇順に並べ替える（保存時にも自動で行われる）");
        }

        int removeIndex = -1;
        for (uint i = 0; i < keys.length(); i++) {
            ImGui::PushID(int(i));
            ImGui::SetNextItemWidth(90);
            ImGui::DragFloat("##time", keys[i].time, 0.01f);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("時刻（秒）");
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90);
            ImGui::DragFloat("##value", keys[i].value, 0.01f);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("値");
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            ImGui::Combo("##ease", keys[i].easeIndex, easeNames);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("このキーから次のキーへ遷移する際のイージング（最後のキーでは未使用）");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                removeIndex = int(i);
            }
            ImGui::PopID();
        }
        if (removeIndex >= 0) {
            keys.removeAt(removeIndex);
        }
    }

    //==================================================
    // プレビュー
    //==================================================

    float GetDuration() {
        float duration = 0.0f;
        for (uint i = 0; i < keys.length(); i++) {
            if (keys[i].time > duration) duration = keys[i].time;
        }
        return duration;
    }

    /// KeyframeAnimation::Evaluateと同じ規則で指定時刻の値を評価する
    float Evaluate(float time) {
        if (keys.length() == 0) return 0.0f;
        SortKeysByTime();
        if (time <= keys[0].time) return keys[0].value;
        KeyframeDef@ lastKey = keys[keys.length() - 1];
        if (time >= lastKey.time) return lastKey.value;
        for (uint i = 1; i < keys.length(); i++) {
            if (time < keys[i].time) {
                KeyframeDef@ from = keys[i - 1];
                KeyframeDef@ to = keys[i];
                float span = to.time - from.time;
                if (span <= 0.0f) return to.value;
                float t = (time - from.time) / span;
                return Easing::Lerp(from.value, to.value, Easing::Apply(t, EaseType(from.easeIndex)));
            }
        }
        return lastKey.value;
    }

    void ShowPreviewSection() {
        ImGui::Text("Preview");
        float duration = GetDuration();
        if (duration <= 0.0f || keys.length() < 2) {
            ImGui::TextDisabled("キーを2つ以上追加するとプレビューできます");
            return;
        }
        if (previewTime > duration) previewTime = duration;
        ImGui::SliderFloat("Time", previewTime, 0.0f, duration);
        ImGui::Text("Value: " + Evaluate(previewTime));
    }
}
