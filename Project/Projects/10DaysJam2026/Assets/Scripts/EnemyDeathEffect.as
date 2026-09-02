class EnemyDeathEffect : ScriptComponentBehavior {
    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    Vector2 uvStep = Vector2(0.25f, 1.0f);

    [SerializeField, Tooltip("EnemyDeathEffect")]
    Object@ effect; 

    [SerializeField, Tooltip("座標")]
    Vector3 pos;

    SpriteRenderer@ sprite;
    float animTimer = 0.0f;

    void Start() {
        GetComponent(@sprite);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null)return;

        tf.SetTranslate(pos);

        // タイマー更新
        animTimer += GetDeltaTime();

        // 4コマのアニメーションフレーム計算
        int frame = int(animTimer / frameInterval) % 4;
        Vector2 uvTranslate = Vector2(frame * uvStep.x, 0.0f);

        // UVTranslateの適用
        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(uvTranslate);
        }

        // アニメーションが1周したらオブジェクトを削除
        if (animTimer >= frameInterval * 4.0f) {
            effect.SetActive(false);
        }
    }

    void End() {
    }
}