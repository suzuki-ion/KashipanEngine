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

        if (isVisible && currentIndex >= 0) {
            if (isAnimating) {
                UpdateReveal();
            }
            UpdateCharacterShake();

            if (IsCommandTriggered(advanceCommandName)) {
                if (isAnimating) {
                    FinishReveal();
                } else {
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
    }

    // index番目の会話を表示開始する。範囲外なら会話を終了する
    void BeginEntry(int index) {
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
