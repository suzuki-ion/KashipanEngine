enum Direction {
    Right,
    Left
}

class Bat : ScriptComponentBehavior {

    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    float uvStep = 0.5f;

    [SerializeField, Tooltip("プレイヤー")]
    Object @player;

    SpriteRenderer@ sprite;
    float animTimer = 0.0f;

    void Start() {
        GetComponent(@sprite);
    }

    void Update() {
        // アニメーション処理
        animTimer += GetDeltaTime();
        int frame = int(animTimer / frameInterval) % 2;

        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(Vector2(frame * uvStep, 0.0f));
        }
    }

    void End() {
    }
}