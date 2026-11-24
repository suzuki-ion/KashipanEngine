// プロジェクトの Assets/Application/Scripts/LoadingSceneController.as にコピーする。
// 「LoadingScene」として登録した通常のシーンに ScriptComponent として取り付ける。
// 遷移元のシーンで GetScene().ChangeSceneWithLoading("Stage1", "LoadingScene"); を呼び出す。
class LoadingSceneController : ScriptComponentBehavior {
    [SerializeField] float minimumDisplaySeconds = 0.5f;
    [SerializeField] float finishAnimationSeconds = 0.25f;
    [SerializeField] bool waitForManualFinish = false;

    float progress = 0.0f;
    string error;
    bool ready = false;
    bool finishing = false;
    bool activationRequested = false;
    float elapsed = 0.0f;
    float finishElapsed = 0.0f;

    void Start() {
        GetScene().SetSceneLoadBudgetMilliseconds(2.0f);
        // ここでロード中のアニメーション、音声、その他のシーン固有の処理を開始する。
    }

    void Update() {
        elapsed += GetDeltaTime();
        // シーンを遷移後まで保持せず、操作ごとに Scene@ を取得する。
        Scene@ scene = GetScene();
        progress = scene.GetSceneLoadProgress();
        error = scene.GetSceneLoadError();
        ready = scene.IsSceneLoadReady();
        if (error.length() > 0) return; // ロード画面の表示を続け、エラーと再試行・戻るための UI を表示する。

        if (ready && elapsed >= minimumDisplaySeconds && !waitForManualFinish && !finishing) FinishLoading();
        if (finishing && ready && !activationRequested) {
            finishElapsed += GetDeltaTime();
            if (finishElapsed >= finishAnimationSeconds) activationRequested = scene.ActivateLoadedScene();
        }
    }

    // 待機アニメーションの周期が終わったときや、ユーザーが確認した後に呼び出す。
    void FinishLoading() {
        if (!GetScene().IsSceneLoadReady() || finishing) return;
        finishing = true;
        finishElapsed = 0.0f;
        // ここで KeyFrameAnimator や独自処理を使い、終了演出やフェードを開始する。
    }

    void CancelLoading() {
        GetScene().CancelSceneLoad();
        finishing = false;
        // 必要に応じて、既存のシーン遷移 API で登録済みのメニューシーンへ移動する。
    }
}
