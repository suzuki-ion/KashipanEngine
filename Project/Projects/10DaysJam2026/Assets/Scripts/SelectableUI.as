// 選択UI用コンポーネント。UIButtonが付与されている前提のオブジェクトを配列で受け取り、
// 指定した入力コマンドで選択項目を前後に移動・決定できるようにする。
// 選択中/決定時のオブジェクトはselectedObject/decidedObjectとしてGetVariableから取得できる。
class SelectableUI : ScriptComponentBehavior {
    [Header("選択肢")]
    [SerializeField, Tooltip("UIButtonが付与された選択可能オブジェクトの一覧")]
    array<Object@>@ selectableObjects = null;

    [Header("色設定")]
    [SerializeField, ColorPicker, Tooltip("ホバー(選択中)時に適用する色")]
    Vector4 hoverColor = Vector4(1.0f, 1.0f, 0.6f, 1.0f);

    [SerializeField, ColorPicker, Tooltip("プレス時に適用する色")]
    Vector4 pressedColor = Vector4(1.0f, 0.6f, 0.6f, 1.0f);

    [SerializeField, ColorPicker, Tooltip("クリック/決定時に適用する色")]
    Vector4 clickedColor = Vector4(0.6f, 1.0f, 0.6f, 1.0f);

    [Header("入力")]
    [SerializeField, Tooltip("前の選択肢に移動する入力コマンド名")]
    string prevCommandName = "SelectPrev";

    [SerializeField, Tooltip("次の選択肢に移動する入力コマンド名")]
    string nextCommandName = "SelectNext";

    [SerializeField, Tooltip("選択中の項目を決定する入力コマンド名")]
    string decideCommandName = "Decide";

    [SerializeField, Tooltip("末尾/先頭まで移動したとき反対側へループするか")]
    bool isLoop = true;

    // --- 外部から取得可能な状態 ---
    // GetVariable/SetVariableで読み書きするには[SerializeField]が必須のため付与している
    // (値自体はStart()/Update()で毎回上書きされるため、シーンへ保存されても問題ない)
    [SerializeField, Tooltip("現在選択中のインデックス(-1は未選択)")]
    int selectedIndex = -1;
    [SerializeField, Tooltip("現在選択中のオブジェクト")]
    Object@ selectedObject = null;
    [SerializeField, Tooltip("決定された瞬間のフレームだけ非nullになるオブジェクト")]
    Object@ decidedObject = null;
    [SerializeField, Tooltip("決定された瞬間のフレームだけtrueになるフラグ")]
    bool isDecided = false;

    // --- 実行時状態(保存不要) ---
    array<Vector4> originalColors;

    void Start() {
        CacheOriginalColors();
        selectedIndex = (selectableObjects !is null && selectableObjects.length() > 0) ? 0 : -1;
        UpdateSelectedObject();
    }

    void Update() {
        isDecided = false;
        @decidedObject = null;

        if (selectableObjects is null || selectableObjects.length() == 0) {
            return;
        }

        // マウスホバーでも選択項目を切り替えられるようにする
        for (uint i = 0; i < selectableObjects.length(); ++i) {
            UIButton@ button;
            if (!TryGetButton(i, @button)) continue;
            if (button.IsHovered()) {
                selectedIndex = int(i);
            }
        }

        if (IsCommandTriggered(prevCommandName)) {
            MoveSelection(-1);
        } else if (IsCommandTriggered(nextCommandName)) {
            MoveSelection(1);
        }
        UpdateSelectedObject();

        // マウスクリックによる決定
        for (uint i = 0; i < selectableObjects.length(); ++i) {
            UIButton@ button;
            if (!TryGetButton(i, @button)) continue;
            if (button.IsClicked()) {
                selectedIndex = int(i);
                UpdateSelectedObject();
                Decide();
                break;
            }
        }

        // 入力コマンドによる決定
        if (!isDecided && IsCommandTriggered(decideCommandName)) {
            Decide();
        }

        ApplyColors();
    }

    // 選択項目をdelta分だけ移動する(isLoopがtrueなら反対側へ折り返す)
    void MoveSelection(int delta) {
        int count = int(selectableObjects.length());
        if (count == 0) return;

        if (selectedIndex < 0) {
            selectedIndex = 0;
            return;
        }

        int newIndex = selectedIndex + delta;
        if (isLoop) {
            newIndex = ((newIndex % count) + count) % count;
        } else {
            if (newIndex < 0) newIndex = 0;
            if (newIndex >= count) newIndex = count - 1;
        }
        selectedIndex = newIndex;
    }

    // 現在の選択項目を決定状態にする
    void Decide() {
        if (selectedObject is null) return;
        @decidedObject = selectedObject;
        isDecided = true;
    }

    // selectedIndexに対応するオブジェクトをselectedObjectへ反映する
    void UpdateSelectedObject() {
        if (selectableObjects is null || selectedIndex < 0 || uint(selectedIndex) >= selectableObjects.length()) {
            @selectedObject = null;
            return;
        }
        @selectedObject = selectableObjects[selectedIndex];
    }

    // 各選択肢の表示色を、状態(クリック>プレス>選択中/ホバー>通常)に応じて反映する
    void ApplyColors() {
        for (uint i = 0; i < selectableObjects.length(); ++i) {
            Object@ obj = selectableObjects[i];
            if (obj is null) continue;

            UIButton@ button;
            bool hasButton = obj.GetComponent(@button);

            Vector4 color = (i < originalColors.length()) ? originalColors[i] : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            if (hasButton && button.IsPressed()) {
                color = pressedColor;
            } else if (int(i) == selectedIndex) {
                color = (isDecided && obj is decidedObject) ? clickedColor : hoverColor;
            } else if (hasButton && button.IsHovered()) {
                color = hoverColor;
            }

            SetObjectColor(obj, color);
        }
    }

    // obj上のUIButtonを取得する
    bool TryGetButton(uint index, UIButton@ &out outButton) {
        Object@ obj = selectableObjects[index];
        if (obj is null) return false;
        return obj.GetComponent(@outButton);
    }

    // 開始時点の各オブジェクトの表示色を、復帰用に控えておく
    void CacheOriginalColors() {
        originalColors.resize(0);
        if (selectableObjects is null) return;

        for (uint i = 0; i < selectableObjects.length(); ++i) {
            Vector4 color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            Object@ obj = selectableObjects[i];
            if (obj !is null) {
                TryGetObjectColor(obj, color);
            }
            originalColors.insertLast(color);
        }
    }

    // SpriteRenderer/TextRenderer/BitmapTextRendererのいずれかからInstanceColorを取得する
    bool TryGetObjectColor(Object@ obj, Vector4 &out outColor) {
        SpriteRenderer@ sprite;
        if (obj.GetComponent(@sprite)) {
            outColor = sprite.GetInstanceColor();
            return true;
        }
        TextRenderer@ text;
        if (obj.GetComponent(@text)) {
            outColor = text.GetInstanceColor();
            return true;
        }
        BitmapTextRenderer@ bitmapText;
        if (obj.GetComponent(@bitmapText)) {
            outColor = bitmapText.GetInstanceColor();
            return true;
        }
        return false;
    }

    // SpriteRenderer/TextRenderer/BitmapTextRendererのいずれかにInstanceColorを設定する
    void SetObjectColor(Object@ obj, const Vector4 &in color) {
        SpriteRenderer@ sprite;
        if (obj.GetComponent(@sprite)) {
            sprite.SetInstanceColor(color);
            return;
        }
        TextRenderer@ text;
        if (obj.GetComponent(@text)) {
            text.SetInstanceColor(color);
            return;
        }
        BitmapTextRenderer@ bitmapText;
        if (obj.GetComponent(@bitmapText)) {
            bitmapText.SetInstanceColor(color);
        }
    }
}
