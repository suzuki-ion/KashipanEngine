// 選択UI用コンポーネント。UIButtonが付与されている前提のオブジェクトを配列で受け取り、
// 指定した入力コマンドで選択項目を前後に移動・決定できるようにする。
// 選択中/決定時のオブジェクトはselectedObject/decidedObjectとしてGetVariableから取得できる。
// holdDecisionUntilConsumedをtrueにすると、決定状態はConsumeDecision()が呼ばれるまで保持される。
// disabledFlagsでselectableObjectsの各項目を個別に無効化でき、無効化中はdisabledColorで表示される。
class SelectableUI : ScriptComponentBehavior {
    [Header("選択肢")]
    [SerializeField, Tooltip("UIButtonが付与された選択可能オブジェクトの一覧")]
    array<Object@>@ selectableObjects = null;

    [SerializeField, Tooltip("各選択肢の無効化フラグ(trueにした項目は選択/決定できなくなる。selectableObjectsと同じ並び・要素数で対応させる)")]
    array<bool>@ disabledFlags = null;

    [Header("色設定")]
    [SerializeField, ColorPicker, Tooltip("通常時に適用する色")]
    Vector4 normalColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    [SerializeField, ColorPicker, Tooltip("ホバー(選択中)時に適用する色")]
    Vector4 hoverColor = Vector4(1.0f, 1.0f, 0.6f, 1.0f);

    [SerializeField, ColorPicker, Tooltip("プレス時に適用する色")]
    Vector4 pressedColor = Vector4(1.0f, 0.6f, 0.6f, 1.0f);

    [SerializeField, ColorPicker, Tooltip("クリック/決定時に適用する色")]
    Vector4 clickedColor = Vector4(0.6f, 1.0f, 0.6f, 1.0f);

    [SerializeField, ColorPicker, Tooltip("無効化されている項目に適用する色")]
    Vector4 disabledColor = Vector4(0.5f, 0.5f, 0.5f, 1.0f);

    [Header("入力")]
    [SerializeField, Tooltip("前の選択肢に移動する入力コマンド名")]
    string prevCommandName = "SelectPrev";

    [SerializeField, Tooltip("次の選択肢に移動する入力コマンド名")]
    string nextCommandName = "SelectNext";

    [SerializeField, Tooltip("選択中の項目を決定する入力コマンド名")]
    string decideCommandName = "Decide";

    [SerializeField, Tooltip("末尾/先頭まで移動したとき反対側へループするか")]
    bool isLoop = true;

    [Header("動作設定")]
    [SerializeField, Tooltip("決定された選択肢(isDecided/decidedObject)を、ConsumeDecision()が呼ばれるまで保持し続けるか。falseの場合は決定した次のフレームで自動的にリセットされる")]
    bool holdDecisionUntilConsumed = false;

    // --- 外部から取得可能な状態 ---
    // GetVariable/SetVariableで読み書きするには[SerializeField]が必須のため付与している
    // (値自体はStart()/Update()で毎回上書きされるため、シーンへ保存されても問題ない)
    [SerializeField, Tooltip("現在選択中のインデックス(-1は未選択)")]
    int selectedIndex = -1;
    [SerializeField, Tooltip("現在選択中のオブジェクト")]
    Object@ selectedObject = null;
    [SerializeField, Tooltip("決定時に非nullになるオブジェクト(holdDecisionUntilConsumedがfalseなら次のフレームでnullに戻る。trueならConsumeDecision()を呼ぶまで保持される)")]
    Object@ decidedObject = null;
    [SerializeField, Tooltip("決定時にtrueになるフラグ(holdDecisionUntilConsumedがfalseなら次のフレームでfalseに戻る。trueならConsumeDecision()を呼ぶまで保持される)")]
    bool isDecided = false;

    // --- 実行時状態(保存不要) ---
    array<Vector4> originalColors;

    void Start() {
        CacheOriginalColors();
        selectedIndex = (selectableObjects !is null && selectableObjects.length() > 0) ? FindFirstEnabledIndex() : -1;
        UpdateSelectedObject();
    }

    void Update() {
        // holdDecisionUntilConsumedがtrueの間は、ConsumeDecision()が呼ばれるまでisDecided/decidedObjectを保持する
        if (!holdDecisionUntilConsumed) {
            isDecided = false;
            @decidedObject = null;
        }

        if (selectableObjects is null || selectableObjects.length() == 0) {
            return;
        }

        // マウスホバーでも選択項目を切り替えられるようにする(無効化項目は対象外)
        for (uint i = 0; i < selectableObjects.length(); ++i) {
            if (IsDisabled(i)) continue;
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

        // マウスクリックによる決定(無効化項目は対象外)
        for (uint i = 0; i < selectableObjects.length(); ++i) {
            if (IsDisabled(i)) continue;
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

    // 選択項目をdelta方向(-1または1)へ移動する(isLoopがtrueなら反対側へ折り返す)。無効化された項目は飛ばす
    void MoveSelection(int delta) {
        int count = int(selectableObjects.length());
        if (count == 0) return;

        if (selectedIndex < 0) {
            selectedIndex = FindFirstEnabledIndex();
            return;
        }

        int newIndex = selectedIndex;
        for (int step = 0; step < count; ++step) {
            newIndex += delta;
            if (isLoop) {
                newIndex = ((newIndex % count) + count) % count;
            } else {
                if (newIndex < 0 || newIndex >= count) return;
            }
            if (!IsDisabled(uint(newIndex))) {
                selectedIndex = newIndex;
                return;
            }
        }
        // すべての選択肢が無効化されている場合は移動しない
    }

    // 現在の選択項目を決定状態にする
    void Decide() {
        if (selectedObject is null) return;
        if (selectedIndex >= 0 && IsDisabled(uint(selectedIndex))) return;
        @decidedObject = selectedObject;
        isDecided = true;
    }

    // 保持されている決定状態を消費する(isDecided/decidedObjectをリセットする)。
    // holdDecisionUntilConsumedがtrueのとき、外部スクリプトは処理完了後にこれを呼んで状態をクリアする
    void ConsumeDecision() {
        isDecided = false;
        @decidedObject = null;
        // 呼び出し元がこの直後にメニューの親を非アクティブ化すると、次のUpdate()が走らず
        // clickedColorのまま残る。非表示になる前に現在の選択色へ戻しておく。
        if (selectableObjects !is null && selectableObjects.length() > 0) {
            ApplyColors();
        }
    }

    // 指定インデックスが無効化されているかを返す
    bool IsDisabled(uint index) const {
        return disabledFlags !is null && index < disabledFlags.length() && disabledFlags[index];
    }

    // 無効化されていない最初のインデックスを返す(見つからない場合は-1)
    int FindFirstEnabledIndex() {
        for (uint i = 0; i < selectableObjects.length(); ++i) {
            if (!IsDisabled(i)) return int(i);
        }
        return -1;
    }

    // selectedIndexに対応するオブジェクトをselectedObjectへ反映する
    void UpdateSelectedObject() {
        if (selectableObjects is null || selectedIndex < 0 || uint(selectedIndex) >= selectableObjects.length()) {
            @selectedObject = null;
            return;
        }
        @selectedObject = selectableObjects[selectedIndex];
    }

    // 各選択肢の表示色を、状態(無効化>クリック>プレス>選択中/ホバー>通常)に応じて反映する
    void ApplyColors() {
        for (uint i = 0; i < selectableObjects.length(); ++i) {
            Object@ obj = selectableObjects[i];
            if (obj is null) continue;

            Vector4 color = (i < originalColors.length()) ? originalColors[i] : Vector4(1.0f, 1.0f, 1.0f, 1.0f);

            if (IsDisabled(i)) {
                color = disabledColor;
            } else {
                UIButton@ button;
                bool hasButton = obj.GetComponent(@button);

                if (hasButton && button.IsPressed()) {
                    color = pressedColor;
                } else if (int(i) == selectedIndex) {
                    color = (isDecided && obj is decidedObject) ? clickedColor : hoverColor;
                } else if (hasButton && button.IsHovered()) {
                    color = hoverColor;
                }
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
