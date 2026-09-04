class SpriteAnimator : ScriptComponentBehavior {
    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    Vector2 uvStep = Vector2(0.25f, 1.0f);

    [SerializeField, Tooltip("アニメーションの総コマ数")]
    int frameCount = 1;

    [SerializeField, Tooltip("初期の行(Y位置)")]
    int startRow = 0;

    [SerializeField, Tooltip("ゲーム開始時から自動再生するか(false=何かのきっかけで再生開始するまで静止)")]
    bool playOnStart = true;

    [SerializeField, Tooltip("ループ再生")]
    bool isLoop = true;

    SpriteRenderer@ sprite;
    float animTimer = 0.0f;
    bool isPlaying = true;
    int currentRow = 0;

    void Start() {
        GetComponent(@sprite);
        currentRow = startRow;
        isPlaying = playOnStart;
    }

    void Update() {
        if (!isPlaying || sprite is null || frameCount <= 0) return;

        animTimer += GetDeltaTime();

        // フレーム計算
        int frame = int(animTimer / frameInterval);

        if (!isLoop && frame >= frameCount) {
            frame = frameCount - 1; // 最終フレームで停止
            isPlaying = false;
        } else {
            frame = frame % frameCount; // ループ
        }

        // UVの適用（Xにコマ数、Yに行数を掛ける）
        Vector2 currentUv = Vector2(frame * uvStep.x, currentRow * uvStep.y);
        sprite.SetInstanceUvTranslate(currentUv);
    }

    // 指定した行(Y位置)のアニメーションを再生
    void PlayRow(int row) {
        if (currentRow == row && isPlaying) return;
        currentRow = row;
        animTimer = 0.0f;
        isPlaying = true;
    }

    // 強制的に最初から再生
    void PlayRowForce(int row) {
        currentRow = row;
        animTimer = 0.0f;
        isPlaying = true;
    }

    // コマ数(X方向)の変更
    void SetFrameCount(int count) {
        frameCount = count;
    }

    // 切り替え速度の変更
    void SetFrameInterval(float interval) {
        frameInterval = interval;
    }

    // 停止
    void Stop() {
        isPlaying = false;
    }

    // 再開
    void Resume() {
        isPlaying = true;
    }
}