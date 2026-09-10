// エンドクレジットを下から上へスクロールし、表示終了後にタイトルへ戻す。
// シーン開始時のDitherFadeによるフェードインが完了してからスクロールを開始する。
class EndCreditController : ScriptComponentBehavior {
    [Header("クレジット")]
    [SerializeField, Tooltip("BitmapTextRendererを持つクレジット本文オブジェクト")]
    Object@ creditsObject;

    [SerializeField, Tooltip("スクロール開始時のY座標")]
    float startY = -16.0f;

    [SerializeField, Tooltip("本文を流し終えたとみなすY座標")]
    float endY = 480.0f;

    [SerializeField, Tooltip("通常時のスクロール速度(毎秒ピクセル)")]
    float normalScrollSpeed = 16.0f;

    [SerializeField, Tooltip("早送り入力中のスクロール速度(毎秒ピクセル)")]
    float fastScrollSpeed = 64.0f;

    [SerializeField, Tooltip("Attackと同じキーを押している間に反応する入力コマンド名")]
    string fastForwardCommandName = "CreditFastForward";

    [Header("シーン遷移")]
    [SerializeField, Tooltip("フェードイン/アウトを行うDitherFadeを持つオブジェクト")]
    Object@ ditherFadeObject;

    [SerializeField, Tooltip("クレジット終了後に戻るシーン名")]
    string titleSceneName = "TitleScene";

    Transform@ creditsTransform;
    ScriptComponent@ ditherFadeScript;

    bool isWaitingForFadeIn = true;
    bool isTransitionPending = false;
    int transitionWaitFrames = 0;

    void Start() {
        if (creditsObject !is null) {
            @creditsTransform = creditsObject.GetTransform();
        }
        if (creditsTransform !is null) {
            Vector3 pos = creditsTransform.GetTranslate();
            pos.y = startY;
            creditsTransform.SetTranslate(pos);
        }
        if (ditherFadeObject !is null) {
            ditherFadeObject.GetComponent(@ditherFadeScript);
        }

        // DitherFadeが無い構成では即座にスクロールを始める。
        isWaitingForFadeIn = ditherFadeScript !is null;
    }

    void Update() {
        if (isTransitionPending) {
            UpdateTitleTransition();
            return;
        }

        if (isWaitingForFadeIn) {
            bool isFading = false;
            if (!ditherFadeScript.CallMethod("IsFading") || !ditherFadeScript.GetLastReturnValue(isFading) || !isFading) {
                isWaitingForFadeIn = false;
            }
            return;
        }

        if (creditsTransform is null) {
            BeginTitleTransition();
            return;
        }

        Vector3 pos = creditsTransform.GetTranslate();
        float speed = GetCommandValue(fastForwardCommandName) > 0.0f
            ? fastScrollSpeed
            : normalScrollSpeed;
        pos.y += speed * GetDeltaTime();
        creditsTransform.SetTranslate(pos);

        if (pos.y >= endY) {
            BeginTitleTransition();
        }
    }

    void BeginTitleTransition() {
        if (isTransitionPending) return;

        if (ditherFadeScript !is null && ditherFadeScript.CallMethod("FadeOut")) {
            isTransitionPending = true;
            transitionWaitFrames = 0;
            return;
        }

        CompleteTitleTransition();
    }

    // DitherFade.Update()との実行順に依存しないよう、最低1フレーム待って完了を確認する。
    void UpdateTitleTransition() {
        ++transitionWaitFrames;

        if (ditherFadeScript is null) {
            CompleteTitleTransition();
            return;
        }

        bool isFading = false;
        if (!ditherFadeScript.CallMethod("IsFading") || !ditherFadeScript.GetLastReturnValue(isFading)) {
            CompleteTitleTransition();
            return;
        }

        if (!isFading && transitionWaitFrames >= 2) {
            CompleteTitleTransition();
        }
    }

    void CompleteTitleTransition() {
        isTransitionPending = false;

        Scene@ scene = GetScene();
        if (scene is null || titleSceneName.length() == 0) return;

        scene.SetNextSceneName(titleSceneName);
        scene.ChangeToNextScene();
    }
}
