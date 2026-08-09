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
//   2. Curve に編集中の曲線が線で表示される（白い点＝キー、赤い縦線＝Previewの時刻）。
//      点をクリック&ドラッグするとその点の値を直接編集できる（時刻は変わらない）
//   3. Preview のスライダーで時刻を動かすと、その時刻の評価値を確認できる
//   4. Save Path を指定して "Save" で保存（保存時に時刻昇順へソートされる）、"Load" で読み込み
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
    // Curve上でドラッグ中のキーのインデックス（ドラッグしていない場合は-1）
    int draggingKeyIndex = -1;

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
        if (!IsEditorWindowOpen("Keyframe Animation Editor")) return;
        ShowSaveLoadSection();
        ImGui::Separator();
        ShowKeysSection();
        ImGui::Separator();
        ShowCurveSection();
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

    //==================================================
    // 曲線の可視化
    //==================================================

    /// 時刻・値をキャンバス上のスクリーン座標へ変換する（Yは値が大きいほど上になるよう反転する）
    Vector2 ToCanvas(const Vector2 &in canvasPos, const Vector2 &in canvasSize,
        float minTime, float maxTime, float minValue, float maxValue, float time, float value) {
        float tx = (maxTime > minTime) ? (time - minTime) / (maxTime - minTime) : 0.0f;
        float ty = (maxValue > minValue) ? (value - minValue) / (maxValue - minValue) : 0.5f;
        return Vector2(canvasPos.x + tx * canvasSize.x, canvasPos.y + (1.0f - ty) * canvasSize.y);
    }

    void ShowCurveSection() {
        ImGui::Text("Curve");
        if (keys.length() == 0) {
            ImGui::TextDisabled("キーを追加すると曲線が表示されます");
            return;
        }

        // キャンバス領域を確保する（幅は使用可能領域いっぱい、高さは固定）
        Vector2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.y = 160.0f;
        if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
        Vector2 canvasPos = ImGui::GetCursorScreenPos();
        Vector2 canvasMax = canvasPos + canvasSize;

        ImGui::DrawRectFilled(canvasPos, canvasMax, Vector4(0.1f, 0.1f, 0.1f, 1.0f));
        ImGui::DrawRect(canvasPos, canvasMax, Vector4(0.5f, 0.5f, 0.5f, 1.0f));

        // 値の表示範囲を決定する（少し余白を持たせ、範囲が0の場合は±1の幅を確保する）
        float minValue = keys[0].value;
        float maxValue = keys[0].value;
        for (uint i = 1; i < keys.length(); i++) {
            if (keys[i].value < minValue) minValue = keys[i].value;
            if (keys[i].value > maxValue) maxValue = keys[i].value;
        }
        if (maxValue - minValue < 0.0001f) {
            minValue -= 1.0f;
            maxValue += 1.0f;
        } else {
            float margin = (maxValue - minValue) * 0.1f;
            minValue -= margin;
            maxValue += margin;
        }
        // 時刻は0からGetDuration()まで（KeyFrameAnimatorの評価・再生時と同じ範囲）を表示する
        float maxTime = GetDuration();
        if (maxTime <= 0.0f) maxTime = 1.0f;

        // 値0の基準線（範囲内にある場合のみ）
        if (minValue < 0.0f && maxValue > 0.0f) {
            Vector2 zeroA = ToCanvas(canvasPos, canvasSize, 0.0f, maxTime, minValue, maxValue, 0.0f, 0.0f);
            Vector2 zeroB = ToCanvas(canvasPos, canvasSize, 0.0f, maxTime, minValue, maxValue, maxTime, 0.0f);
            ImGui::DrawLine(zeroA, zeroB, Vector4(0.4f, 0.4f, 0.4f, 1.0f), 1.0f);
        }

        // 曲線（Evaluate()を一定間隔でサンプリングして線分をつなぐ）
        const int sampleCount = 96;
        Vector2 prevPoint(0.0f, 0.0f);
        for (int i = 0; i <= sampleCount; i++) {
            float t = maxTime * (float(i) / float(sampleCount));
            Vector2 p = ToCanvas(canvasPos, canvasSize, 0.0f, maxTime, minValue, maxValue, t, Evaluate(t));
            if (i > 0) {
                ImGui::DrawLine(prevPoint, p, Vector4(0.3f, 0.8f, 1.0f, 1.0f), 2.0f);
            }
            prevPoint = p;
        }

        // ドラッグ操作用の透明ボタンをキャンバス全体に重ねる（レイアウト領域の確保も兼ねる）。
        // クリックした位置に最も近いキーをドラッグ対象にし、ボタンが押されている間は
        // マウスのY座標からその点の値を逆算して直接書き換える（時刻は変更しない）
        bool clicked = ImGui::InvisibleButton("##curveCanvas", canvasSize);
        Vector2 mousePos = ImGui::GetMousePos();
        if (clicked) {
            const float pickRadius = 10.0f;
            float bestDistSq = pickRadius * pickRadius;
            int bestIndex = -1;
            for (uint i = 0; i < keys.length(); i++) {
                Vector2 p = ToCanvas(canvasPos, canvasSize, 0.0f, maxTime, minValue, maxValue, keys[i].time, keys[i].value);
                float dx = mousePos.x - p.x;
                float dy = mousePos.y - p.y;
                float distSq = dx * dx + dy * dy;
                if (distSq <= bestDistSq) {
                    bestDistSq = distSq;
                    bestIndex = int(i);
                }
            }
            draggingKeyIndex = bestIndex;
        }
        if (!ImGui::IsItemActive()) {
            draggingKeyIndex = -1;
        } else if (draggingKeyIndex >= 0 && draggingKeyIndex < int(keys.length())) {
            float ty = 1.0f - Clamp((mousePos.y - canvasPos.y) / canvasSize.y, 0.0f, 1.0f);
            keys[draggingKeyIndex].value = minValue + ty * (maxValue - minValue);
        }
        if (ImGui::IsItemHovered() && draggingKeyIndex < 0) {
            ImGui::SetTooltip("キー上でクリック&ドラッグすると、その点の値を直接編集できます");
        }

        // キー位置のマーカー（ドラッグ中のキーは強調表示する）
        for (uint i = 0; i < keys.length(); i++) {
            Vector2 p = ToCanvas(canvasPos, canvasSize, 0.0f, maxTime, minValue, maxValue, keys[i].time, keys[i].value);
            bool isDragging = (int(i) == draggingKeyIndex);
            Vector4 color = isDragging ? Vector4(1.0f, 0.85f, 0.2f, 1.0f) : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            ImGui::DrawCircleFilled(p, isDragging ? 5.0f : 4.0f, color);
        }

        // 現在のプレビュー時刻の位置
        if (previewTime >= 0.0f && previewTime <= maxTime) {
            Vector2 a = ToCanvas(canvasPos, canvasSize, 0.0f, maxTime, minValue, maxValue, previewTime, minValue);
            Vector2 b = ToCanvas(canvasPos, canvasSize, 0.0f, maxTime, minValue, maxValue, previewTime, maxValue);
            ImGui::DrawLine(a, b, Vector4(1.0f, 0.3f, 0.3f, 1.0f), 1.0f);
        }
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
