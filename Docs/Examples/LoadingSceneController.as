// Copy to your project's Assets/Application/Scripts/LoadingSceneController.as.
// Attach as ScriptComponent in a normal scene registered as "LoadingScene".
// Request from the source scene: GetScene().ChangeSceneWithLoading("Stage1", "LoadingScene");
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
        // Start your loading animation, audio and other scene-specific processing here.
    }

    void Update() {
        elapsed += GetDeltaTime();
        // Obtain Scene@ for each operation rather than retaining a scene across transitions.
        Scene@ scene = GetScene();
        progress = scene.GetSceneLoadProgress();
        error = scene.GetSceneLoadError();
        ready = scene.IsSceneLoadReady();
        if (error.length() > 0) return; // Keep the loading scene visible; show error/retry/back UI.

        if (ready && elapsed >= minimumDisplaySeconds && !waitForManualFinish && !finishing) FinishLoading();
        if (finishing && ready && !activationRequested) {
            finishElapsed += GetDeltaTime();
            if (finishElapsed >= finishAnimationSeconds) activationRequested = scene.ActivateLoadedScene();
        }
    }

    // Call after a waiting animation reaches its cycle boundary, or after a user confirmation.
    void FinishLoading() {
        if (!GetScene().IsSceneLoadReady() || finishing) return;
        finishing = true;
        finishElapsed = 0.0f;
        // Start your outro/fade with KeyFrameAnimator or other custom processing here.
    }

    void CancelLoading() {
        GetScene().CancelSceneLoad();
        finishing = false;
        // Change to a registered menu scene using the existing scene transition API if needed.
    }
}
