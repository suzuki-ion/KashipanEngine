class Blood : ScriptComponentBehavior {
    [SerializeField, Tooltip("落下速度(下方向への一定速度)")]
    float fallSpeed = 180.0f;

    [SerializeField, Tooltip("全体の最大生存時間(秒)")]
    float lifeTime = 4.0f;

    [SerializeField, Tooltip("地面に着弾してから消滅するまでの時間(秒)")]
    float destroyDelay = 0.2f;

    [SerializeField, Tooltip("ダメージ量")]
    float damageAmount = 1.0f;

    float timer = 0.0f;
    float groundTimer = 0.0f;
    bool isGrounded = false;
    Vector2 velocity;

    CharacterController2D@ controller;

    void Start() {
        GetComponent(@controller);
        velocity.x = 0.0f;
        velocity.y = -fallSpeed; // 重力加速はせず、常に一定速度で下方向へ飛ばす
    }

    void Update() {
        // 会話中(isDialogueActive)は落下などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        Transform@ tf = GetTransform();
        if (tf is null) return;

        // 最大生存時間を超えたら強制的に非アクティブ化する
        timer += GetDeltaTime();
        if (timer >= lifeTime) {
            Object@ obj = GetOwnerObject();
            if (obj !is null) obj.SetActive(false);
            return;
        }

        if (!isGrounded) {
            if (controller !is null) {
                // CharacterController2Dがある場合はコライダーベースで移動と着地判定を行う
                controller.Move(velocity * GetDeltaTime());
                if (controller.IsGrounded()) {
                    isGrounded = true;
                    velocity.y = 0.0f;
                }
            } else {
                // Controllerがない場合の簡易フォールバック
                Vector3 pos = tf.GetTranslate();
                pos.y += velocity.y * GetDeltaTime();

                if (pos.y <= 0.0f) {
                    pos.y = 0.0f;
                    isGrounded = true;
                    velocity.y = 0.0f;
                }
                tf.SetTranslate(pos);
            }
        } else {
            // 着地(または画面下端到達)後は少し待ってから消滅させる
            groundTimer += GetDeltaTime();
            if (groundTimer >= destroyDelay) {
                Object@ obj = GetOwnerObject();
                if (obj !is null) obj.SetActive(false);
            }
        }
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
