// 選択肢1件分の内容(DialogueEntry.choices配列の要素として編集する)
[System.Serializable]
class DialogueChoice {
    [TextArea(1, 3), Tooltip("選択肢に表示するテキスト")]
    string text;
    [Tooltip("選ばれたときに進む会話のインデックス(dialogues配列の添字)。-1なら会話を終了する")]
    int nextIndex = -1;
}

// 会話ボックスに表示する1件分の内容(Inspectorのdialogues配列の要素として編集する)
// (MoveGroundPathPoint.asと同様、プリミティブ/Vector/enumのみをフラットに持つ構成にしている。
//  [System.Serializable]クラスをさらにネストしてハンドル無しでメンバに持たせる書き方は、実機で
//  検証した結果AngelScript側がそのメンバを一切初期化せずクラッシュの原因になったため使用禁止
//  [エンジン側のScriptComponent::ValidateGCValueDeclarationsがこのパターンをビルド時に検出する])
[System.Serializable]
class DialogueEntry {
    [Header("キャラクターアイコン")]
    [TexturePath, Tooltip("アイコンに使用するテクスチャのアセットパス")]
    string iconTextureName;
    [Tooltip("アイコンのUV位置(アトラス内オフセット)")]
    Vector2 iconUvPosition = Vector2(0.0f, 0.0f);
    [Tooltip("アイコンのUVスケール")]
    Vector2 iconUvScale = Vector2(1.0f, 1.0f);

    [Header("テキスト")]
    [TextArea(2, 6), Tooltip("表示するテキスト")]
    string text;
    [Tooltip("1文字表示されるまでの秒数")]
    float characterInterval = 0.05f;

    [Header("シェイク")]
    [Tooltip("文字のシェイクを有効にするか")]
    bool shakeEnabled = false;
    [Tooltip("揺れの振れ幅(ピクセル、XY)")]
    Vector2 shakeAmplitude = Vector2(2.0f, 2.0f);
    [Tooltip("目標値を選び直す速度(1秒あたりの回数)")]
    float shakeSpeed = 20.0f;
    [Tooltip("目標値へ向かう際に使うイージング")]
    EaseType shakeEaseType = EaseType::Linear;

    [Header("選択肢")]
    [Tooltip("このテキストの後に選択肢を挟む場合に設定する。空(要素数0)なら通常通りadvanceCommandNameで次へ進む")]
    array<DialogueChoice@>@ choices = null;
}

// 会話ボックス。isVisibleが立っている間だけ背景/テキスト/アイコンを表示し、
// dialoguesを先頭から順に、1文字ずつのタイプライター表示で送っていく。
class DialogueBox : ScriptComponentBehavior {
    [Header("表示制御")]
    [SerializeField, Tooltip("会話ボックスの表示フラグ。falseの間は関連オブジェクトを非アクティブ化する")]
    bool isVisible = false;

    [Header("参照オブジェクト")]
    [SerializeField, Tooltip("テキストボックスの背景オブジェクト")]
    Object@ backgroundObject;

    [SerializeField, Tooltip("テキストボックスのテキスト用オブジェクト(TextRendererが付いていること)")]
    Object@ textObject;

    [SerializeField, Tooltip("テキストボックスのキャラクターアイコン用オブジェクト(SpriteRenderer・TextureSourceが付いていること)")]
    Object@ iconObject;

    [Header("会話内容")]
    [SerializeField, Tooltip("表示する会話の一覧。先頭から順に表示する")]
    array<DialogueEntry@>@ dialogues = null;

    [Header("入力")]
    [SerializeField, Tooltip("次のテキストへ進めるための入力コマンド名")]
    string advanceCommandName = "Attack";

    [Header("選択肢UI")]
    [SerializeField, Tooltip("選択肢UIのオブジェクト(SelectableUIがアタッチされていること)。選択肢を使わないなら未設定でよい")]
    Object@ selectionUIObject;

    [SerializeField, Tooltip("選択肢の表示に使うオブジェクトの一覧。selectionUIObject側のSelectableUI.selectableObjectsと同じ並び・同じオブジェクトを指定すること(BitmapTextRenderer・TextRendererのいずれかが付いていること)")]
    array<Object@>@ choiceSlotObjects = null;

    // --- 参照コンポーネント(保存不要) ---
    BitmapTextRenderer@ textRenderer;
    SpriteRenderer@ iconSprite;
    TextureSource@ iconTexture;

    // --- 実行時状態(保存不要) ---
    bool previousVisible = false;
    int currentIndex = -1;
    bool isAnimating = false;
    float revealTimer = 0.0f;
    uint visibleCharCount = 0;
    uint totalCharCount = 0;
    bool isShowingChoices = false;

    // 1文字ごとのシェイク実行状態
    array<float> charShakeTimer;
    array<Vector2> charShakePrevious;
    array<Vector2> charShakeTarget;

    void Start() {
        if (textObject !is null) {
            textObject.GetComponent(@textRenderer);
        }
        if (iconObject !is null) {
            iconObject.GetComponent(@iconSprite);
            iconObject.GetComponent(@iconTexture);
        }
        ApplyActive(isVisible);
    }

    void Update() {
        bool justShown = isVisible && !previousVisible;
        ApplyActive(isVisible);

        if (justShown) {
            BeginEntry(0);
        }

        if (isVisible && currentIndex >= 0 && dialogues !is null && uint(currentIndex) < dialogues.length()) {
            DialogueEntry@ entry = dialogues[currentIndex];

            if (isAnimating) {
                UpdateReveal();
                UpdateCharacterShake();
                if (IsCommandTriggered(advanceCommandName)) {
                    FinishReveal();
                }
            } else {
                UpdateCharacterShake();

                bool hasChoices = entry !is null && entry.choices !is null && entry.choices.length() > 0;
                if (hasChoices) {
                    if (!isShowingChoices) {
                        BeginChoices(entry);
                    }
                    UpdateChoices(entry);
                } else if (IsCommandTriggered(advanceCommandName)) {
                    BeginEntry(currentIndex + 1);
                }
            }
        }

        previousVisible = isVisible;
    }

    // 背景/テキスト/アイコンオブジェクトのアクティブ状態をまとめて切り替える
    void ApplyActive(bool active) {
        if (backgroundObject !is null) backgroundObject.SetActive(active);
        if (textObject !is null) textObject.SetActive(active);
        if (iconObject !is null) iconObject.SetActive(active);
        if (!active) EndChoices();
    }

    // index番目の会話を表示開始する。範囲外なら会話を終了する
    void BeginEntry(int index) {
        EndChoices();

        if (dialogues is null || index < 0 || uint(index) >= dialogues.length() || dialogues[index] is null) {
            EndDialogue();
            return;
        }

        currentIndex = index;
        DialogueEntry@ entry = dialogues[index];

        totalCharCount = GetUtf8CharacterCount(entry.text);
        revealTimer = 0.0f;
        visibleCharCount = 0;
        isAnimating = totalCharCount > 0;

        ResetCharacterShakeState(totalCharCount);

        if (textRenderer !is null) {
            textRenderer.SetText("");
        }

        if (iconTexture !is null && entry.iconTextureName.length() > 0) {
            iconTexture.SetTextureAssetPath(entry.iconTextureName);
        }
        if (iconSprite !is null) {
            iconSprite.SetInstanceUvTranslate(entry.iconUvPosition);
            iconSprite.SetInstanceUvScale(entry.iconUvScale);
        }

        if (!isAnimating) {
            ApplyVisibleText();
        }
    }

    // 選択肢UIを表示し、entry.choicesの内容を各選択肢スロットへ反映する
    void BeginChoices(DialogueEntry@ entry) {
        isShowingChoices = true;

        if (selectionUIObject !is null) {
            selectionUIObject.SetActive(true);

            // SelectableUI側の選択状態を、今回の選択肢用にリセットしておく
            // (前回の選択肢が決定済みのまま残っていると、表示した瞬間に誤って決定扱いになってしまうため)
            ScriptComponent@ sc;
            if (selectionUIObject.GetComponent(@sc)) {
                sc.SetVariable("selectedIndex", 0);
                sc.SetVariable("isDecided", false);
            }
        }

        uint choiceCount = entry.choices.length();
        if (choiceSlotObjects !is null) {
            for (uint i = 0; i < choiceSlotObjects.length(); ++i) {
                Object@ slot = choiceSlotObjects[i];
                if (slot is null) continue;

                bool active = i < choiceCount;
                slot.SetActive(active);
                if (active) {
                    SetSlotText(slot, entry.choices[i].text);
                }
            }
        }
    }

    // 選択UI(SelectableUI)の決定状態を監視し、決定された選択肢のnextIndexへ進む
    void UpdateChoices(DialogueEntry@ entry) {
        if (selectionUIObject is null || choiceSlotObjects is null) return;

        ScriptComponent@ sc;
        if (!selectionUIObject.GetComponent(@sc)) return;

        bool decided = false;
        if (!sc.GetVariable("isDecided", decided) || !decided) return;

        Object@ decidedObj;
        if (!sc.GetVariable("decidedObject", @decidedObj) || decidedObj is null) return;

        uint choiceCount = entry.choices.length();
        for (uint i = 0; i < choiceSlotObjects.length() && i < choiceCount; ++i) {
            if (choiceSlotObjects[i] is decidedObj) {
                BeginEntry(entry.choices[i].nextIndex);
                return;
            }
        }
    }

    // 選択肢UIを非表示に戻す(選択肢を使わない通常の会話進行時・会話終了時にも呼ばれる)
    void EndChoices() {
        if (!isShowingChoices) return;
        isShowingChoices = false;

        if (selectionUIObject !is null) selectionUIObject.SetActive(false);
        if (choiceSlotObjects !is null) {
            for (uint i = 0; i < choiceSlotObjects.length(); ++i) {
                if (choiceSlotObjects[i] !is null) choiceSlotObjects[i].SetActive(false);
            }
        }
    }

    // BitmapTextRenderer・TextRendererのいずれかへテキストを設定する(選択肢スロットの表示更新用)
    void SetSlotText(Object@ obj, const string &in text) {
        BitmapTextRenderer@ bitmapText;
        if (obj.GetComponent(@bitmapText)) {
            bitmapText.SetText(text);
            return;
        }
        TextRenderer@ normalText;
        if (obj.GetComponent(@normalText)) {
            normalText.SetText(text);
        }
    }

    // 表示中のテキストアニメーションを進める
    void UpdateReveal() {
        if (currentIndex < 0 || dialogues is null || uint(currentIndex) >= dialogues.length()) return;
        DialogueEntry@ entry = dialogues[currentIndex];
        if (entry is null) return;

        if (entry.characterInterval <= 0.0f) {
            visibleCharCount = totalCharCount;
        } else {
            revealTimer += GetDeltaTime();
            visibleCharCount = uint(revealTimer / entry.characterInterval);
            if (visibleCharCount > totalCharCount) visibleCharCount = totalCharCount;
        }

        ApplyVisibleText();

        if (visibleCharCount >= totalCharCount) {
            isAnimating = false;
        }
    }

    // アニメーションを即座に完了させ、テキストを全文表示にする
    void FinishReveal() {
        visibleCharCount = totalCharCount;
        ApplyVisibleText();
        isAnimating = false;
    }

    // 現在のvisibleCharCountぶんだけテキストを反映する
    void ApplyVisibleText() {
        if (textRenderer is null || currentIndex < 0 || dialogues is null || uint(currentIndex) >= dialogues.length()) return;
        DialogueEntry@ entry = dialogues[currentIndex];
        if (entry is null) return;

        uint byteLength = GetUtf8ByteOffsetForCharCount(entry.text, visibleCharCount);
        textRenderer.SetText(entry.text.substr(0, byteLength));
    }

    // 表示中の文字を1文字ごとにシェイクさせる(既存Shakeコンポーネントと同様、ランダムな
    // 目標値へイージングで追従し続けることで揺れを表現する)
    void UpdateCharacterShake() {
        if (textRenderer is null || currentIndex < 0 || dialogues is null || uint(currentIndex) >= dialogues.length()) return;
        DialogueEntry@ entry = dialogues[currentIndex];
        if (entry is null || !entry.shakeEnabled || visibleCharCount == 0) return;

        float speed = entry.shakeSpeed;
        if (speed <= 0.0f) return;

        float dt = GetDeltaTime();
        float stepDuration = 1.0f / speed;
        Vector2 amplitude = entry.shakeAmplitude;
        EaseType easeType = entry.shakeEaseType;

        for (uint i = 0; i < visibleCharCount; ++i) {
            charShakeTimer[i] += dt;
            if (charShakeTimer[i] >= stepDuration) {
                charShakeTimer[i] = charShakeTimer[i] % stepDuration;
                charShakePrevious[i] = charShakeTarget[i];
                charShakeTarget[i] = Vector2(Random::Float(-amplitude.x, amplitude.x), Random::Float(-amplitude.y, amplitude.y));
            }

            float t = Clamp(charShakeTimer[i] / stepDuration, 0.0f, 1.0f);
            textRenderer.SetCharacterOffset(i, Easing::Eased(charShakePrevious[i], charShakeTarget[i], t, easeType));
        }
    }

    // 新しい会話に入る際、文字数ぶんのシェイク実行状態を作り直し、前の会話で付いていた
    // 文字オフセットを消しておく
    void ResetCharacterShakeState(uint count) {
        if (textRenderer !is null) {
            uint64 previousCount = textRenderer.GetCharacterCount();
            for (uint64 i = 0; i < previousCount; ++i) {
                textRenderer.SetCharacterOffset(i, Vector2(0.0f, 0.0f));
            }
        }

        charShakeTimer.resize(count);
        charShakePrevious.resize(count);
        charShakeTarget.resize(count);
        for (uint i = 0; i < count; ++i) {
            charShakeTimer[i] = 0.0f;
            charShakePrevious[i] = Vector2(0.0f, 0.0f);
            charShakeTarget[i] = Vector2(0.0f, 0.0f);
        }
    }

    // 会話をすべて表示し終えたときの終了処理。関連オブジェクトを非アクティブ化する
    void EndDialogue() {
        currentIndex = -1;
        isAnimating = false;
        isVisible = false;
        ApplyActive(false);
        if (textRenderer !is null) {
            textRenderer.SetText("");
        }
    }

    // text中のUTF-8文字数(コードポイント数)を数える。TextRendererのGetCharacterCount()と
    // 対応させるための処理(AngelScriptのstring.length()はバイト数を返すため)
    uint GetUtf8CharacterCount(const string &in text) const {
        uint length = text.length();
        uint count = 0;
        for (uint i = 0; i < length; ++i) {
            if ((uint(text[i]) & 0xC0) != 0x80) {
                count++;
            }
        }
        return count;
    }

    // text中の先頭からcharCount文字(コードポイント数)ぶんに対応するバイト長を求める
    // (マルチバイト文字の途中でsubstrしてしまわないようにするための処理)
    uint GetUtf8ByteOffsetForCharCount(const string &in text, uint charCount) const {
        if (charCount == 0) return 0;

        uint length = text.length();
        uint seen = 0;
        for (uint i = 0; i < length; ++i) {
            if ((uint(text[i]) & 0xC0) != 0x80) {
                if (seen == charCount) return i;
                seen++;
            }
        }
        return length;
    }
}
