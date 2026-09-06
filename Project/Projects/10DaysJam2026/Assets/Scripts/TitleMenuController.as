// タイトルシーンのメニュー選択結果を処理するスクリプト。
// SelectableUIが決定したオブジェクトがstartUIObject/optionUIObject/exitUIObjectの
// どれと一致するかによって、シーン遷移・設定UI表示・ゲーム終了を振り分ける。
//
// 前提（エディター側で設定が必要）:
//   - selectableUIObject に SelectableUI コンポーネントを持つオブジェクトを指定しておくこと
//   - startUIObject/optionUIObject/exitUIObject には、SelectableUI側のselectableObjects配列に
//     渡しているものと同一のオブジェクトをそれぞれ指定しておくこと
//   - nextSceneName に遷移先のシーン名(GameScene等)を指定しておくこと
//   - 設定用UIはまだ用意されていないため、optionMenuRootObjectは未設定のままでよい
//     (用意でき次第、設定UIのルートオブジェクトを指定すればアクティブ化されるようになる)
//   - 画面フェード演出を入れる場合は、screenFadeObject に ScreenFade を持つオブジェクトを指定しておくこと
//     (遷移直前に画面を瞬時に覆い、遷移先シーン側の同名コンポーネントがフェードインで覆いを解く。
//      フェードインの秒数は遷移先シーンに置いたScreenFade側のfadeInDurationで指定する)

class TitleMenuController : ScriptComponentBehavior {
    [Header("参照")]
    [SerializeField, Tooltip("決定状態を監視するSelectableUIを持つオブジェクト")]
    Object@ selectableUIObject;

    [Header("選択肢との対応")]
    [SerializeField, Tooltip("ゲーム開始用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ startUIObject;

    [SerializeField, Tooltip("設定画面表示用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ optionUIObject;

    [SerializeField, Tooltip("ゲーム終了用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ exitUIObject;

    [Header("ゲーム開始")]
    [SerializeField, Tooltip("ゲーム開始時に遷移するシーン名")]
    string nextSceneName = "GameScene";

    [Header("設定UI")]
    [SerializeField, Tooltip("設定用UIのルートオブジェクト(未実装の間はnullのままでよい)")]
    Object@ optionMenuRootObject;

    [Header("画面フェード")]
    [SerializeField, Tooltip("シーン遷移直前に画面を覆うScreenFadeを持つオブジェクト(未設定なら覆わず即座に遷移する)")]
    Object@ screenFadeObject;

    // --- 参照コンポーネント(保存不要) ---
    ScriptComponent@ selectableUIScript;

    void Start() {
        if (selectableUIObject !is null) {
            selectableUIObject.GetComponent(@selectableUIScript);
        }
    }

    void Update() {
        if (selectableUIScript is null) return;

        bool isDecided = false;
        if (!selectableUIScript.GetVariable("isDecided", isDecided) || !isDecided) return;

        Object@ decidedObject;
        if (!selectableUIScript.GetVariable("decidedObject", decidedObject) || decidedObject is null) return;

        if (startUIObject !is null && decidedObject is startUIObject) {
            GoToGameScene();
        } else if (optionUIObject !is null && decidedObject is optionUIObject) {
            ShowOptionMenu();
        } else if (exitUIObject !is null && decidedObject is exitUIObject) {
            ExitGame();
        }
    }

    // 画面を覆ってからnextSceneNameへシーン遷移する
    void GoToGameScene() {
        if (nextSceneName.length() == 0) return;

        Scene@ scene = GetScene();
        if (scene is null) return;

        CoverScreenBeforeTransition();

        scene.SetNextSceneName(nextSceneName);
        scene.ChangeToNextScene();
    }

    // screenFadeObjectが指定されていれば、画面を瞬時に不透明で覆い遷移の瞬間を隠す
    // (シーン遷移自体は次フレームに反映されるため、このフレームは覆われた状態のまま切り替わる。
    //  遷移先シーン側に置いたScreenFadeがfadeInOnStartで自動的に覆いを解く)
    void CoverScreenBeforeTransition() {
        if (screenFadeObject is null) return;

        ScriptComponent@ fadeScript;
        if (!screenFadeObject.GetComponent(@fadeScript)) return;

        float alpha = 1.0f;
        fadeScript.CallMethod("SetAlphaInstant", alpha);
    }

    // 設定用UIを表示する
    // (現時点では設定用UI自体が未実装のため、optionMenuRootObjectが指定されていれば
    //  それをアクティブ化するのみ。設定用UIが用意でき次第、ここにoptionMenuRootObjectを
    //  割り当てれば表示されるようになる)
    void ShowOptionMenu() {
        if (optionMenuRootObject !is null) {
            optionMenuRootObject.SetActive(true);
        }
    }

    // ゲームを終了する
    void ExitGame() {
        RequestExitGameLoop();
    }
}
