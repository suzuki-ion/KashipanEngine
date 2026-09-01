enum Direction {
    Right,
    Left
}

class Bat : ScriptComponentBehavior {

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 10.0f;

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
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // アニメーション処理
        animTimer += GetDeltaTime();
        int frame = int(animTimer / frameInterval) % 2;

        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(Vector2(frame * uvStep, 0.0f));
        }

        // プレイヤー追従処理
        if (player is null) return;
        Transform@ playerTf = player.GetTransform();
        if (playerTf is null) return;

        Vector3 currentPos = tf.GetTranslate();
        Vector3 targetPos = playerTf.GetTranslate();

        // プレイヤーへのベクトルと距離を計算
        Vector3 diff = targetPos - currentPos;
        float dist = diff.Length();

        if (dist > 0.01f) {
            // プレイヤーの位置に応じて向きを切り替え
            float rotY = (diff.x < 0.0f) ? 0.0f : 3.14159f;
            tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

            // プレイヤーに向かって移動
            Vector3 dir = diff / dist;
            tf.SetTranslate(currentPos + dir * moveSpeed * GetDeltaTime());
        }
    }

    void End() {
    }
}