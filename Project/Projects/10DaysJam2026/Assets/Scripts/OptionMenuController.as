// 設定パネル(OptionMenu)の項目選択・値変更を処理するスクリプト。
// SelectableUIが管理する行(マスター/BGM/SE音量・フルスクリーン・ウィンドウサイズ・ブラウン管風演出・戻る)のうち
// 現在選択中の行を、左右入力(MoveXコマンド)で値変更する。決定キー(Attack)は「戻る」での
// パネルを閉じる処理と、フルスクリーン/ブラウン管風演出のトグルに使う。
//
// 前提（エディター側で設定が必要）:
//   - selectableUIObject に、下記7つの行を selectableObjects として持つ SelectableUI を指定しておくこと
//   - masterVolumeUIObject/bgmVolumeUIObject/seVolumeUIObject/fullscreenUIObject/resolutionUIObject/
//     crtEffectUIObject/backUIObjectには、selectableUIObject側のselectableObjects配列に
//     渡しているものと同一のオブジェクトをそれぞれ指定しておくこと
//   - mainMenuObjectには、設定画面を閉じたときに再表示するメインメニューのルートオブジェクト(UIObject)を、
//     windowObjectにはWindow Objectを指定しておくこと
//   - finalScreenObjectには、BarrelDistortionEffect/ScanlineEffectを持つ最終合成用ScreenBufferオブジェクト
//     (FinalScreen)を指定しておくこと(ブラウン管風演出のON/OFFがここへ反映される)

class OptionMenuController : ScriptComponentBehavior {
    [Header("参照")]
    [SerializeField, Tooltip("決定状態を監視するSelectableUIを持つオブジェクト")]
    Object@ selectableUIObject;

    [SerializeField, Tooltip("設定パネル自体のルートオブジェクト(OptionMenu)。「戻る」でこれをSetActive(false)する")]
    Object@ optionMenuRootObject;

    [SerializeField, Tooltip("設定画面を閉じたときに再表示するメインメニューのルートオブジェクト(UIObject)")]
    Object@ mainMenuObject;

    [SerializeField, Tooltip("フルスクリーン/ウィンドウサイズ設定の適用先(Window Object)")]
    Object@ windowObject;

    [SerializeField, Tooltip("ブラウン管風演出(BarrelDistortionEffect/ScanlineEffect)を持つ最終合成用ScreenBufferオブジェクト(FinalScreen)")]
    Object@ finalScreenObject;

    [Header("行との対応")]
    [SerializeField, Tooltip("マスター音量の行(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ masterVolumeUIObject;

    [SerializeField, Tooltip("BGM音量の行(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ bgmVolumeUIObject;

    [SerializeField, Tooltip("SE音量の行(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ seVolumeUIObject;

    [SerializeField, Tooltip("フルスクリーン切替の行(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ fullscreenUIObject;

    [SerializeField, Tooltip("ウィンドウサイズ切替の行(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ resolutionUIObject;

    [SerializeField, Tooltip("ブラウン管風演出切替の行(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ crtEffectUIObject;

    [SerializeField, Tooltip("戻るの行(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ backUIObject;

    // --- 参照コンポーネント(保存不要) ---
    ScriptComponent@ selectableUIScript;
    // GetComponent(?&out)はコンポーネントの実際の登録型(NormalWindowObject)としてしか取得できず、
    // 基底のWindowObject型では直接取得できない。NormalWindowObject@で取得してから
    // opImplCastでWindowObject@へ変換して使うこと(WindowObject側にSetFullscreen等を定義しているため)
    NormalWindowObject@ normalWindowObject;
    WindowObject@ windowComponent;

    // --- 実行時状態(保存不要) ---
    float masterVolume = 1.0f;
    float bgmVolume = 1.0f;
    float seVolume = 1.0f;
    bool fullscreen = false;
    int resolutionIndex = 0;
    bool crtEffect = false;
    float lastMoveX = 0.0f;

    void Start() {
        if (selectableUIObject !is null) {
            selectableUIObject.GetComponent(@selectableUIScript);
        }
        if (windowObject !is null) {
            windowObject.GetComponent(@normalWindowObject);
            if (normalWindowObject !is null) {
                @windowComponent = normalWindowObject;
            }
        }
        LoadSettings();
        ApplyCrtEffect();
        RefreshAllTexts();
    }

    void Update() {
        if (selectableUIScript is null) return;

        // 左右入力(MoveX)の立ち上がりで1ステップだけ値を変化させる
        // (継続入力の値をそのまま使うと1フレームごとに変化してしまうため、エッジ検出する)
        float moveX = GetCommandValue("MoveX");
        int step = 0;
        if (moveX > 0.5f && lastMoveX <= 0.5f) {
            step = 1;
        } else if (moveX < -0.5f && lastMoveX >= -0.5f) {
            step = -1;
        }
        lastMoveX = moveX;

        if (step != 0) {
            Object@ selectedObject;
            if (selectableUIScript.GetVariable("selectedObject", @selectedObject) && selectedObject !is null) {
                AdjustValue(selectedObject, step);
            }
        }

        // 決定キー/クリックによる決定(「戻る」を閉じる、フルスクリーン/ブラウン管風演出はトグル)
        bool isDecided = false;
        if (!selectableUIScript.GetVariable("isDecided", isDecided) || !isDecided) return;

        Object@ decidedObject;
        if (!selectableUIScript.GetVariable("decidedObject", @decidedObject) || decidedObject is null) return;

        selectableUIScript.CallMethod("ConsumeDecision");

        if (backUIObject !is null && decidedObject.GetUUID() == backUIObject.GetUUID()) {
            CloseOptionMenu();
        } else if (fullscreenUIObject !is null && decidedObject.GetUUID() == fullscreenUIObject.GetUUID()) {
            AdjustValue(decidedObject, 1);
        } else if (crtEffectUIObject !is null && decidedObject.GetUUID() == crtEffectUIObject.GetUUID()) {
            AdjustValue(decidedObject, 1);
        } else {
            // 値を左右に動かす行は、クリックした側を画面上の< / >操作として扱う。
            // キーボード/ゲームパッドの決定入力では従来どおり値を変えない。
            int mouseStep = 0;
            if (TryGetMouseAdjustment(decidedObject, mouseStep)) {
                AdjustValue(decidedObject, mouseStep);
            }
        }
    }

    // UIButtonのクリック位置は(0,0)=左下～(1,1)=右上。
    // 行の左半分を減少、右半分を増加として扱う。
    bool TryGetMouseAdjustment(Object@ target, int &out step) {
        step = 0;
        if (target is null) return false;

        UIButton@ button;
        if (!target.GetComponent(@button) || !button.IsClicked()) return false;

        Vector2 localPosition;
        if (!button.TryGetLocalHoverPosition(localPosition)) return false;
        step = localPosition.x < 0.5f ? -1 : 1;
        return true;
    }

    // 現在保存されている設定値を読み込む(表示専用のキャッシュ。実際の適用はTitleMenuController.Start()側で
    // 既に行われているため、ここでは再適用せず値の取得のみ行う)
    void LoadSettings() {
        masterVolume = GetSettingFloat("audio.masterVolume", 1.0f);
        bgmVolume = GetSettingFloat("audio.category.BGM", 1.0f);
        seVolume = GetSettingFloat("audio.category.SE", 1.0f);
        fullscreen = GetSettingBool("video.fullscreen", false);
        resolutionIndex = GetSettingInt("video.resolutionIndex", 0);
        crtEffect = GetSettingBool("video.crtEffect", false);
    }

    // targetの行に対応する値をdelta方向へ1ステップ変化させ、即座に適用・永続化・表示更新する
    void AdjustValue(Object@ target, int delta) {
        if (masterVolumeUIObject !is null && target.GetUUID() == masterVolumeUIObject.GetUUID()) {
            masterVolume = ClampVolume(masterVolume + float(delta) * 0.1f);
            SetMasterVolume(masterVolume);
            SetSettingFloat("audio.masterVolume", masterVolume);
        } else if (bgmVolumeUIObject !is null && target.GetUUID() == bgmVolumeUIObject.GetUUID()) {
            bgmVolume = ClampVolume(bgmVolume + float(delta) * 0.1f);
            SetCategoryVolume("BGM", bgmVolume);
            SetSettingFloat("audio.category.BGM", bgmVolume);
        } else if (seVolumeUIObject !is null && target.GetUUID() == seVolumeUIObject.GetUUID()) {
            seVolume = ClampVolume(seVolume + float(delta) * 0.1f);
            SetCategoryVolume("SE", seVolume);
            SetSettingFloat("audio.category.SE", seVolume);
        } else if (fullscreenUIObject !is null && target.GetUUID() == fullscreenUIObject.GetUUID()) {
            fullscreen = !fullscreen;
            if (windowComponent !is null) windowComponent.SetFullscreen(fullscreen);
            SetSettingBool("video.fullscreen", fullscreen);
        } else if (resolutionUIObject !is null && target.GetUUID() == resolutionUIObject.GetUUID()) {
            resolutionIndex = (resolutionIndex + delta + kResolutionCount) % kResolutionCount;
            if (windowComponent !is null) {
                Vector2 resolution = GetResolutionCandidate(resolutionIndex);
                windowComponent.SetWindowResolution(int(resolution.x), int(resolution.y));
            }
            SetSettingInt("video.resolutionIndex", resolutionIndex);
        } else if (crtEffectUIObject !is null && target.GetUUID() == crtEffectUIObject.GetUUID()) {
            crtEffect = !crtEffect;
            SetSettingBool("video.crtEffect", crtEffect);
            ApplyCrtEffect();
        } else {
            return;
        }
        RefreshAllTexts();
    }

    // finalScreenObjectのBarrelDistortionEffect/ScanlineEffectへ現在のcrtEffectを反映する
    void ApplyCrtEffect() {
        if (finalScreenObject is null) return;
        BarrelDistortionEffect@ barrel;
        if (finalScreenObject.GetComponent(@barrel)) barrel.SetActive(crtEffect);
        ScanlineEffect@ scanline;
        if (finalScreenObject.GetComponent(@scanline)) scanline.SetActive(crtEffect);
    }

    float ClampVolume(float value) {
        if (value < 0.0f) return 0.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    }

    // ウィンドウサイズ候補(TitleMenuController側と同じ並び)
    int kResolutionCount = 3;
    Vector2 GetResolutionCandidate(int index) {
        if (index == 1) return Vector2(1600.0f, 900.0f);
        if (index == 2) return Vector2(1920.0f, 1080.0f);
        return Vector2(1280.0f, 720.0f);
    }

    // 各行の表示テキストを現在値に合わせて更新する
    void RefreshAllTexts() {
        SetRowText(masterVolumeUIObject, "マスター音量  " + FormatPercentRow(masterVolume));
        SetRowText(bgmVolumeUIObject, "BGM音量  " + FormatPercentRow(bgmVolume));
        SetRowText(seVolumeUIObject, "SE音量  " + FormatPercentRow(seVolume));
        SetRowText(fullscreenUIObject, "フルスクリーン  " + FormatToggleRow(fullscreen));
        SetRowText(resolutionUIObject, "ウィンドウサイズ  " + FormatResolutionRow(resolutionIndex));
        SetRowText(crtEffectUIObject, "ブラウン管風演出  " + FormatToggleRow(crtEffect));
        SetRowText(backUIObject, "戻る");
    }

    void SetRowText(Object@ target, const string &in text) {
        if (target is null) return;
        BitmapTextRenderer@ bitmapText;
        if (target.GetComponent(@bitmapText)) {
            bitmapText.SetText(text);
        }
    }

    string FormatPercentRow(float value) {
        int percent = int(value * 100.0f + 0.5f);
        return "<  " + percent + "%  >";
    }

    string FormatToggleRow(bool value) {
        return value ? "<  ON  >" : "<  OFF  >";
    }

    string FormatResolutionRow(int index) {
        Vector2 resolution = GetResolutionCandidate(index);
        return "<  " + int(resolution.x) + "x" + int(resolution.y) + "  >";
    }

    // 設定画面を閉じ、メインメニューを再表示する
    // (GetOwnerObject()はこのスクリプトが乗っているOptionMenuController自身を指し、
    //  パネル全体のルート(OptionMenu)ではないため使わないこと。誤って使うと、
    //  このスクリプト自身が非アクティブ化されてUpdate()が二度と呼ばれなくなり、
    //  パネルの見た目も残ったまま「戻る」が反応しなくなる)
    void CloseOptionMenu() {
        if (mainMenuObject !is null) {
            mainMenuObject.SetActive(true);
        }
        if (optionMenuRootObject !is null) {
            optionMenuRootObject.SetActive(false);
        }
    }
}
