// A normal loading scene: black background with only a spinning icon at bottom right.
class LoadingController : ScriptComponentBehavior {
    [SerializeField] float minimumDisplaySeconds = 0.5f;
    [SerializeField] float iconFadeSeconds = 0.12f;
    float elapsed = 0;
    float fadeElapsed = 0;
    bool finishing = false;
    bool activationRequested = false;
    bool errorLogged = false;

    void Start() {
        GetScene().SetSceneLoadBudgetMilliseconds(2.0f);
        ResizeBackground();
    }

    void ResizeBackground() {
        Scene@ scene = GetScene();
        Camera2D@ camera;
        Object@ cameraObject = scene.GetObject("LoadingCamera");
        Object@ background = scene.GetObject("LoadingBackground");
        if (cameraObject !is null && background !is null && cameraObject.GetComponent(@camera)) {
            background.GetTransform().SetScale(Vector3(camera.GetWidth(), camera.GetHeight(), 1));
        }
    }

    void Update() {
        ResizeBackground();
        float dt = Min(GetDeltaTime(), 0.1f);
        elapsed += dt;
        Scene@ scene = GetScene();
        string error = scene.GetSceneLoadError();
        if (error.length() > 0) {
            if (!errorLogged) { LogError("Loading scene: " + error); errorLogged = true; }
            return;
        }
        if (scene.IsSceneLoadReady() && elapsed >= minimumDisplaySeconds) finishing = true;
        if (!finishing || activationRequested) return;
        fadeElapsed += dt;
        float alpha = iconFadeSeconds > 0 ? Max(0.0f, 1.0f - fadeElapsed / iconFadeSeconds) : 0;
        for (int i = 0; i < 12; ++i) {
            Object@ spoke = scene.GetObject("LoadingSpoke" + i);
            SpriteRenderer@ sprite;
            if (spoke !is null && spoke.GetComponent(@sprite)) {
                sprite.SetInstanceColor(Vector4(1, 1, 1, alpha * (0.2f + 0.8f * float(i) / 11.0f)));
            }
        }
        if (alpha <= 0) activationRequested = scene.ActivateLoadedScene();
    }
}
