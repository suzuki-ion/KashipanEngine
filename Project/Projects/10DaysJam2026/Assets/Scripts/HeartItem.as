class HeartItem : ScriptComponentBehavior {

    [SerializeField, Tooltip("重力")]
    float gravity = 250.0f;

    [SerializeField, Tooltip("横方向の空気抵抗（ブレーキ）")]
    float dampingX = 0.8f;

    [SerializeField, Tooltip("最低Y座標（これより下へ落ちない）")]
    float minY = -9999.0f;

    [SerializeField, Tooltip("初速（外部からSetVariableで設定）")]
    Vector2 velocity = Vector2(0.0f, 0.0f);

    [SerializeField, Tooltip("着地時のバウンド係数(0でバウンドなし)")]
    float bounceFactor = 0.35f;

    Transform@ tf;
    bool isGrounded = false;

    void Start() {
        @tf = GetTransform();
    }

    void Update() {
        if (tf is null || isGrounded) return;

        Vector3 pos = tf.GetTranslate();

        // 重力適用
        velocity.y -= gravity * GetDeltaTime();

        // 横移動の減速処理
        if (velocity.x > 0.0f) {
            velocity.x = Max(0.0f, velocity.x - dampingX * GetDeltaTime() * 50.0f);
        } else if (velocity.x < 0.0f) {
            velocity.x = Min(0.0f, velocity.x + dampingX * GetDeltaTime() * 50.0f);
        }

        // 位置の更新
        pos.x += velocity.x * GetDeltaTime();
        pos.y += velocity.y * GetDeltaTime();

        // 着地判定およびバウンド処理
        if (pos.y <= minY && velocity.y < 0.0f) {
            pos.y = minY;

            // 勢いが残っていればバウンド、小さくなれば完全停止
            if (Abs(velocity.y) > 30.0f && bounceFactor > 0.0f) {
                velocity.y = -velocity.y * bounceFactor;
                velocity.x *= 0.5f; // 跳ねた時に横移動も少し減速
            } else {
                velocity = Vector2(0.0f, 0.0f);
                isGrounded = true;
            }
        }

        tf.SetTranslate(pos);
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherCollider.GetTag() == "Player") {
            GetOwnerObject().SetActive(false);
        }
    }
}