// ゲームオーバー画面を制御するコンポーネント。
// Player.as側で死亡が確定し、一定時間が経過するとシーン変数"gameOverRequested"がtrueになる。
// これを監視して、黒背景と「コンティニュー/タイトルにもどる/おわる」の3択メニューを表示する。
// 選択操作自体はSelectableUI.as（TitleMenuControllerと同じ仕組み）に任せ、このスクリプトは
// 決定結果の振り分けとDitherFadeを使ったシーン遷移（またはゲーム終了）だけを担当する。
// Playerへの直接参照は持たない(SavePoint.as等と同じ疎結合の設計方針)。
//
// 前提(エディター側で設定が必要):
//   - gameOverUIObjectに、黒背景と3つの選択肢をぶら下げた親オブジェクトを指定しておくこと
//     (このスクリプト自身はSetActive(true/false)の切り替えのみを行う)
//   - selectableUIObjectに、SelectableUIコンポーネントを持つオブジェクト(gameOverUIObjectと
//     同一で構わない)を指定しておくこと
//   - continueUIObject/titleUIObject/exitUIObjectには、SelectableUI側のselectableObjects配列に
//     渡しているものと同一のオブジェクトをそれぞれ指定しておくこと
//   - titleSceneNameに「タイトルにもどる」選択時に遷移するタイトルシーン名を指定しておくこと
//   - 画面フェード演出を入れる場合は、ditherFadeObjectにDitherFadeを持つオブジェクトを指定して
//     おくこと(DitherFadeのフェードアウト完了後にシーン遷移/ゲーム終了を行う)
class GameOverMenu : ScriptComponentBehavior {
    [Header("表示制御")]
    [SerializeField, Tooltip("ゲームオーバー画面のルートオブジェクト(黒背景/3択メニューをぶら下げた親)")]
    Object@ gameOverUIObject;

    [Header("選択メニュー")]
    [SerializeField, Tooltip("決定状態を監視するSelectableUIを持つオブジェクト(gameOverUIObjectと同一で可)")]
    Object@ selectableUIObject;

    [SerializeField, Tooltip("「コンティニュー」用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ continueUIObject;

    [SerializeField, Tooltip("「タイトルにもどる」用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ titleUIObject;

    [SerializeField, Tooltip("「おわる」用のオブジェクト(SelectableUIのselectableObjectsに含めているものと同一)")]
    Object@ exitUIObject;

    [Header("タイトルへ戻る")]
    [SerializeField, Tooltip("「タイトルにもどる」選択時に遷移するタイトルシーン名")]
    string titleSceneName = "TitleScene";

    [Header("画面フェード")]
    [SerializeField, Tooltip("シーン遷移/ゲーム終了の直前に画面を覆うDitherFadeを持つオブジェクト(未設定なら覆わず即座に実行する)")]
    Object@ ditherFadeObject;

    [Header("入力")]
    [SerializeField, Tooltip("表示直後の誤入力で即決定してしまわないための、決定操作を受け付け始めるまでの遅延秒数")]
    float inputDelay = 0.3f;

    // --- 参照コンポーネント(保存不要) ---
    ScriptComponent@ selectableUIScript;
    ScriptComponent@ ditherFadeScript;

    // --- 実行時状態(保存不要) ---
    bool isShowing = false;
    float shownTimer = 0.0f;

    bool isSceneTransitionPending = false;
    int sceneTransitionWaitFrames = 0;
    string pendingSceneName = "";
    bool pendingIsExit = false;

    void Start() {
        if (gameOverUIObject !is null) {
            gameOverUIObject.SetActive(false);
        }
        if (selectableUIObject !is null) {
            selectableUIObject.GetComponent(@selectableUIScript);
        }
        if (ditherFadeObject !is null) {
            ditherFadeObject.GetComponent(@ditherFadeScript);
        }
    }

    // シーン終了時、表示中のままだった場合に備えてシーン変数を消費しておく
    // (次にこのシーンを読み込んだ際、古いgameOverRequestedが残らないようにする)
    void End() {
        if (isShowing) {
            GetScene().SetVariable("gameOverRequested", false);
        }
    }

    void Update() {
        if (isSceneTransitionPending) {
            UpdateSceneTransition();
            return;
        }

        bool requested = false;
        GetScene().GetVariable("gameOverRequested", requested);

        if (requested && !isShowing) {
            isShowing = true;
            shownTimer = 0.0f;
            if (gameOverUIObject !is null) {
                gameOverUIObject.SetActive(true);
            }
            // 前回表示時の選択状態(ハイライト色など)を持ち越さないようにリセットする
            if (selectableUIScript !is null) {
                selectableUIScript.CallMethod("ResetSelection");
            }
        }

        if (!isShowing) return;

        shownTimer += GetDeltaTime();
        if (shownTimer < inputDelay) return;
        if (selectableUIScript is null) return;

        bool isDecided = false;
        if (!selectableUIScript.GetVariable("isDecided", isDecided) || !isDecided) return;

        Object@ decidedObject;
        if (!selectableUIScript.GetVariable("decidedObject", @decidedObject) || decidedObject is null) return;

        // SelectableUI側のholdDecisionUntilConsumedがtrueの場合、消費しないと
        // isDecided/decidedObjectが保持され続け、毎フレーム同じ決定処理が走ってしまうため必ず消費する
        selectableUIScript.CallMethod("ConsumeDecision");

        if (continueUIObject !is null && decidedObject.GetUUID() == continueUIObject.GetUUID()) {
            Continue();
        } else if (titleUIObject !is null && decidedObject.GetUUID() == titleUIObject.GetUUID()) {
            ReturnToTitle();
        } else if (exitUIObject !is null && decidedObject.GetUUID() == exitUIObject.GetUUID()) {
            ExitGame();
        }
    }

    // セーブ地点があるシーンへ遷移してやり直す(セーブ地点は別シーンの場合もあるため、
    // 常に現在シーンをリロードするのではなく、保存済みのシーン名(save_sceneName)を優先する。
    // 一度もセーブしていない場合はsave_sceneNameが空のため、現在のシーンをリロードする)
    void Continue() {
        string targetSceneName = GetScene().GetName();

        string savedSceneName;
        if (GetScene().GetGlobalVariable("save_sceneName", savedSceneName) && savedSceneName.length() > 0) {
            targetSceneName = savedSceneName;
        }

        BeginTransition(targetSceneName, false);
    }

    // タイトルシーンへ戻る
    void ReturnToTitle() {
        if (titleSceneName.length() == 0) return;
        BeginTransition(titleSceneName, false);
    }

    // ゲームを終了する
    void ExitGame() {
        BeginTransition("", true);
    }

    // DitherFadeで画面を覆い終えてから、targetSceneNameへのシーン遷移(またはゲーム終了)を行う
    void BeginTransition(const string &in targetSceneName, bool isExit) {
        isShowing = false;
        GetScene().SetVariable("gameOverRequested", false);
        if (gameOverUIObject !is null) {
            gameOverUIObject.SetActive(false);
        }

        pendingSceneName = targetSceneName;
        pendingIsExit = isExit;

        if (ditherFadeScript !is null && ditherFadeScript.CallMethod("FadeOut")) {
            isSceneTransitionPending = true;
            sceneTransitionWaitFrames = 0;
            return;
        }

        CompleteTransition();
    }

    // FadeOut()とDitherFade.Update()の実行順に依存しないよう、最低1フレームは待機する。
    // duration=0の場合も2回目の確認で完了できる。
    void UpdateSceneTransition() {
        ++sceneTransitionWaitFrames;
        if (ditherFadeScript is null) {
            CompleteTransition();
            return;
        }

        bool isFading = false;
        if (!ditherFadeScript.CallMethod("IsFading") || !ditherFadeScript.GetLastReturnValue(isFading)) {
            CompleteTransition();
            return;
        }

        if (!isFading && sceneTransitionWaitFrames >= 2) {
            CompleteTransition();
        }
    }

    void CompleteTransition() {
        isSceneTransitionPending = false;

        if (pendingIsExit) {
            RequestExitGameLoop();
            return;
        }

        Scene@ scene = GetScene();
        if (scene is null || pendingSceneName.length() == 0) return;

        scene.SetNextSceneName(pendingSceneName);
        scene.ChangeToNextScene();
    }
}
