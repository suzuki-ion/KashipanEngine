// タイトルシーンのメニュー選択結果を処理するスクリプト。
// SelectableUIが決定したオブジェクトがstartUIObject/continueUIObject/optionUIObject/exitUIObjectの
// どれと一致するかによって、新規ゲーム開始・セーブデータからの再開・設定UI表示・ゲーム終了を振り分ける。
//
// 前提（エディター側で設定が必要）:
//   - selectableUIObject に SelectableUI コンポーネントを持つオブジェクトを指定しておくこと
//   - startUIObject/continueUIObject/optionUIObject/exitUIObjectには、SelectableUI側の
//     selectableObjects配列に渡しているものと同一のオブジェクトをそれぞれ指定しておくこと
//   - continueUIObjectは、セーブデータが存在しない場合も表示したまま、SelectableUIの
//     disabledFlagsによって灰色表示・選択不可になる
//   - nextSceneName に「はじめから」選択時に遷移する新規ゲーム用シーン名(GameScene等)を指定しておくこと
//   - optionMenuRootObjectには設定パネル(OptionMenu)のルートオブジェクトを、mainMenuObjectには
//     設定画面表示中に隠すメインメニューのルートオブジェクト(selectableUIObjectと同一)を指定しておくこと
//   - windowObjectにはWindow Objectを、finalScreenObjectにはBarrelDistortionEffect/ScanlineEffectを
//     持つ最終合成用ScreenBufferオブジェクト(FinalScreen)を指定しておくこと(起動時に保存済みの
//     フルスクリーン/ウィンドウサイズ/ブラウン管風演出設定を復元するため。マスター/BGM/SE音量もStart()時に自動で復元される)
//   - 画面フェード演出を入れる場合は、ditherFadeObject に DitherFade を持つオブジェクトを指定しておくこと
//     (DitherFadeのフェードアウト完了後にシーンを切り替える)

class TitleMenuController : ScriptComponentBehavior {
    [Header("参照")]
    [SerializeField, Tooltip("決定状態を監視するSelectableUIを持つオブジェクト")]
    Object@ selectableUIObject;

    [Header("選択肢との対応")]
    [SerializeField, Tooltip("新規ゲーム開始用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ startUIObject;

    [SerializeField, Tooltip("セーブデータから再開する用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)。セーブデータが無い間は自動的に非表示になる")]
    Object@ continueUIObject;

    [SerializeField, Tooltip("設定画面表示用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ optionUIObject;

    [SerializeField, Tooltip("ゲーム終了用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ exitUIObject;

    [Header("ゲーム開始")]
    [SerializeField, Tooltip("「はじめから」選択時に遷移する新規ゲーム用シーン名")]
    string nextSceneName = "GameScene";

    [Header("設定UI")]
    [SerializeField, Tooltip("設定用UIのルートオブジェクト(OptionMenu)")]
    Object@ optionMenuRootObject;

    [SerializeField, Tooltip("設定画面を開いている間、非表示にするメインメニューのルートオブジェクト(UIObject)")]
    Object@ mainMenuObject;

    [SerializeField, Tooltip("フルスクリーン/ウィンドウサイズ設定の適用先(Window Object)")]
    Object@ windowObject;

    [SerializeField, Tooltip("ブラウン管風演出(BarrelDistortionEffect/ScanlineEffect)を持つ最終合成用ScreenBufferオブジェクト(FinalScreen)")]
    Object@ finalScreenObject;

    [Header("画面フェード")]
    [SerializeField, Tooltip("シーン遷移直前に画面を覆うDitherFadeを持つオブジェクト(未設定なら覆わず即座に遷移する)")]
    Object@ ditherFadeObject;

    // --- 参照コンポーネント(保存不要) ---
    ScriptComponent@ selectableUIScript;
    // GetComponent(?&out)はコンポーネントの実際の登録型(NormalWindowObject)としてしか取得できず、
    // 基底のWindowObject型では直接取得できない。NormalWindowObject@で取得してから
    // opImplCastでWindowObject@へ変換して使うこと(WindowObject側にSetFullscreen等を定義しているため)
    NormalWindowObject@ normalWindowObject;
    WindowObject@ windowComponent;

    // --- 実行時状態(保存不要) ---
    bool hasSaveData = false;
    ScriptComponent@ ditherFadeScript;
    bool isSceneTransitionPending = false;
    int sceneTransitionWaitFrames = 0;
    string pendingSceneName = "";

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
        if (ditherFadeObject !is null) {
            ditherFadeObject.GetComponent(@ditherFadeScript);
        }
        ApplySavedSettings();
        CheckSaveData();
    }

    void Update() {
        if (isSceneTransitionPending) {
            UpdateSceneTransition();
            return;
        }

        if (selectableUIScript is null) return;

        bool isDecided = false;
        if (!selectableUIScript.GetVariable("isDecided", isDecided) || !isDecided) return;

        Object@ decidedObject;
        if (!selectableUIScript.GetVariable("decidedObject", @decidedObject) || decidedObject is null) return;

        // SelectableUI側のholdDecisionUntilConsumedがtrueの場合、消費しないと
        // isDecided/decidedObjectが保持され続け、毎フレーム同じ決定処理が走ってしまうため必ず消費する
        selectableUIScript.CallMethod("ConsumeDecision");

        if (startUIObject !is null && decidedObject.GetUUID() == startUIObject.GetUUID()) {
            StartNewGame();
        } else if (continueUIObject !is null && decidedObject.GetUUID() == continueUIObject.GetUUID()) {
            ContinueGame();
        } else if (optionUIObject !is null && decidedObject.GetUUID() == optionUIObject.GetUUID()) {
            ShowOptionMenu();
        } else if (exitUIObject !is null && decidedObject.GetUUID() == exitUIObject.GetUUID()) {
            ExitGame();
        }
    }

    // 現在のシーン実行状態でセーブデータの読み書きを行ってよいかどうか
    // (エディターでPlayを押さずにタイトルシーンを編集しているだけの間は、実ファイルを触らない)
    bool ShouldPersistProgress() {
        return GetScene().IsPlaying() || !IsEditorBuild();
    }

    // セーブデータの有無を確認し、continueUIObjectの選択可否に反映する。
    // ここで読み込んだ内容は、そのまま「つづきから」選択時の遷移先シーン名判定にも使い回す
    // (Player.as等と同じく、ディスクからの読み込みはセッション中1回だけに留める)
    void CheckSaveData() {
        if (!ShouldPersistProgress()) return;

        bool hasLoadedSaveThisSession = false;
        GetScene().GetGlobalVariable("hasLoadedSaveThisSession", hasLoadedSaveThisSession);
        if (!hasLoadedSaveThisSession) {
            GetScene().LoadGlobalVariables();
            GetScene().SetGlobalVariable("hasLoadedSaveThisSession", true);
        }

        string savedSceneName;
        hasSaveData = GetScene().GetGlobalVariable("save_sceneName", savedSceneName) && savedSceneName.length() > 0;

        SetContinueEnabled(hasSaveData);
    }

    // continueUIObjectは常に表示し、対応するSelectableUIのdisabledFlagsだけを更新する。
    // selectableObjectsの並びを変更しても壊れないよう、UUIDから対象インデックスを探す。
    void SetContinueEnabled(bool enabled) {
        if (continueUIObject is null || selectableUIScript is null) return;
        continueUIObject.SetActive(true);

        array<Object@>@ selectableObjects;
        array<bool>@ disabledFlags;
        if (!selectableUIScript.GetVariable("selectableObjects", @selectableObjects) || selectableObjects is null) return;
        if (!selectableUIScript.GetVariable("disabledFlags", @disabledFlags) || disabledFlags is null) return;

        if (disabledFlags.length() < selectableObjects.length()) {
            disabledFlags.resize(selectableObjects.length());
        }
        for (uint i = 0; i < selectableObjects.length(); ++i) {
            Object@ selectableObject = selectableObjects[i];
            if (selectableObject !is null && selectableObject.GetUUID() == continueUIObject.GetUUID()) {
                disabledFlags[i] = !enabled;
                selectableUIScript.CallMethod("ApplyColors");
                return;
            }
        }
    }

    // 新規ゲームを開始する。CheckSaveData()で確認用に読み込んでいた既存セーブ内容(メモリ上)を
    // 破棄してからnextSceneNameへ遷移する(破棄しないと、ゲーム開始後のPlayer.LoadProgress()が
    // 古いセーブ内容を引き継いでしまう)。ファイルへは反映しないため、ディスク上の既存セーブ
    // データは実際に上書きセーブされるまでそのまま残る
    void StartNewGame() {
        if (nextSceneName.length() == 0) return;

        Scene@ scene = GetScene();
        if (scene is null) return;

        if (ShouldPersistProgress()) {
            scene.ClearGlobalVariables();
            scene.SetGlobalVariable("hasLoadedSaveThisSession", true);
        }

        ChangeSceneWithFade(scene, nextSceneName);
    }

    // セーブデータに記録されているシーンへ遷移し、続きから再開する
    void ContinueGame() {
        if (!hasSaveData) return;

        Scene@ scene = GetScene();
        if (scene is null) return;

        string savedSceneName;
        if (!scene.GetGlobalVariable("save_sceneName", savedSceneName) || savedSceneName.length() == 0) return;

        ChangeSceneWithFade(scene, savedSceneName);
    }

    // DitherFadeで画面を覆い終えてからtargetSceneNameへシーン遷移する(StartNewGame/ContinueGame共通処理)
    void ChangeSceneWithFade(Scene@ scene, const string &in targetSceneName) {
        if (ditherFadeScript !is null && ditherFadeScript.CallMethod("FadeOut")) {
            pendingSceneName = targetSceneName;
            isSceneTransitionPending = true;
            sceneTransitionWaitFrames = 0;
            return;
        }

        scene.SetNextSceneName(targetSceneName);
        scene.ChangeToNextScene();
    }

    // FadeOut()とDitherFade.Update()の実行順に依存しないよう、最低1フレームは待機する。
    // duration=0の場合も2回目の確認で完了できる。
    void UpdateSceneTransition() {
        ++sceneTransitionWaitFrames;
        if (ditherFadeScript is null) {
            CompleteSceneTransition();
            return;
        }

        bool isFading = false;
        if (!ditherFadeScript.CallMethod("IsFading") || !ditherFadeScript.GetLastReturnValue(isFading)) {
            CompleteSceneTransition();
            return;
        }

        if (!isFading && sceneTransitionWaitFrames >= 2) {
            CompleteSceneTransition();
        }
    }

    void CompleteSceneTransition() {
        Scene@ scene = GetScene();
        if (scene is null || pendingSceneName.length() == 0) return;

        scene.SetNextSceneName(pendingSceneName);
        scene.ChangeToNextScene();
    }

    // 設定用UIを表示する(メインメニューは操作を受け付けないよう非表示にする。
    // 閉じる処理はOptionMenuController側(「戻る」選択)が担当し、mainMenuObjectを再表示する)。
    // 非表示にする前に選択状態をリセットしておかないと、「設定」がselectedIndexを保持したまま
    // Updateが止まるため、次にタイトルへ戻ったとき決定時のハイライト色が残ったままになる
    void ShowOptionMenu() {
        if (optionMenuRootObject is null) return;
        if (selectableUIScript !is null) {
            selectableUIScript.CallMethod("ResetSelection");
        }
        if (mainMenuObject !is null) {
            mainMenuObject.SetActive(false);
        }
        optionMenuRootObject.SetActive(true);
    }

    // ゲームを終了する
    void ExitGame() {
        RequestExitGameLoop();
    }

    // 起動時に保存済みのプレイヤー設定(マスター/BGM/SE音量、フルスクリーン、ウィンドウサイズ、ブラウン管風演出)を適用する。
    // 一度も設定画面を開いていなくても前回の設定が反映されるよう、タイトルシーン開始時に必ず1回実行する
    void ApplySavedSettings() {
        SetMasterVolume(GetSettingFloat("audio.masterVolume", 1.0f));
        SetCategoryVolume("BGM", GetSettingFloat("audio.category.BGM", 1.0f));
        SetCategoryVolume("SE", GetSettingFloat("audio.category.SE", 1.0f));

        if (windowComponent !is null) {
            windowComponent.SetFullscreen(GetSettingBool("video.fullscreen", false));
            Vector2 resolution = GetResolutionCandidate(GetSettingInt("video.resolutionIndex", 0));
            windowComponent.SetWindowResolution(int(resolution.x), int(resolution.y));
        }

        if (finalScreenObject !is null) {
            bool crtEffect = GetSettingBool("video.crtEffect", false);
            BarrelDistortionEffect@ barrel;
            if (finalScreenObject.GetComponent(@barrel)) barrel.SetActive(crtEffect);
            ScanlineEffect@ scanline;
            if (finalScreenObject.GetComponent(@scanline)) scanline.SetActive(crtEffect);
        }
    }

    // ウィンドウサイズ候補(OptionMenuController側と同じ並び)からindexに対応するサイズを返す
    Vector2 GetResolutionCandidate(int index) {
        if (index == 1) return Vector2(1600.0f, 900.0f);
        if (index == 2) return Vector2(1920.0f, 1080.0f);
        return Vector2(1280.0f, 720.0f);
    }
}
