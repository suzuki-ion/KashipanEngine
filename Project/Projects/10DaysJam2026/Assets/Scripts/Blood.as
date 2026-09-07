class Blood : ScriptComponentBehavior {
    [SerializeField, Tooltip("最大生存時間(秒)。発射されるまでの待機時間も含む")]
    float lifeTime = 4.0f;

    [SerializeField, Tooltip("ダメージ量")]
    float damageAmount = 1.0f;

    Vector2 velocity;
    float timer = 0.0f;

    // Launch()が呼ばれるまでは、その場に留まり円形配置の見た目を保つ
    bool isLaunched = false;

    CharacterController2D@ controller;

    void Start() {
        GetComponent(@controller);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        // 最大生存時間を超えたら強制的に非アクティブ化する
        timer += GetDeltaTime();
        if (timer >= lifeTime) {
            Object@ obj = GetOwnerObject();
            if (obj !is null) obj.SetActive(false);
            return;
        }

        // 発射されるまでは静止したまま
        if (!isLaunched) return;

        if (controller !is null) {
            controller.Move(velocity * GetDeltaTime());
        } else {
            Vector3 pos = tf.GetTranslate();
            pos.x += velocity.x * GetDeltaTime();
            pos.y += velocity.y * GetDeltaTime();
            tf.SetTranslate(pos);
        }
    }

    // Boss3から呼び出され、指定した速度ベクトルで発射
    void Launch(Vector2 vel) {
        velocity = vel;
        isLaunched = true;
    }

    // プレイヤーなどにぶつかった際の処理
    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherCollider.GetTag() == "Player") {
            Object@ obj = GetOwnerObject();
            if (obj !is null) {
                obj.SetActive(false);
            }

            Object@ player = hit.otherObject;
            if (player !is null) {
                ScriptComponent@ sc;
                if (player.GetComponent(@sc)) {
                    sc.CallMethod("Damage", damageAmount);
                }
            }
        }
    }

    void End() {
    }
}
