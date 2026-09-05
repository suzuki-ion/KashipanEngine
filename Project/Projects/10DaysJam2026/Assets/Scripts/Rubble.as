class Rubble : ScriptComponentBehavior {
    [SerializeField, Tooltip("落下時の重力")]
    float gravity = 200.0f;

    [SerializeField, Tooltip("全体の最大生存時間(秒)")]
    float lifeTime = 5.0f;

    [SerializeField, Tooltip("地面に落下してから消滅するまでの時間(秒)")]
    float destroyDelay = 0.5f;

    [SerializeField, Tooltip("ダメージ量")]
    float damageAmount = 1;

    float timer = 0.0f;
    float groundTimer = 0.0f;
    bool isGrounded = false;
    Vector2 velocity;
    
    CharacterController2D@ controller;

    void Start() {
        GetComponent(@controller);
        velocity.y = 0.0f;
    }

    void Update() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        // 最大生存時間を超えたら強制的に非アクティブ化して削除扱いにする
        timer += GetDeltaTime();
        if (timer >= lifeTime) {
            Object@ obj = GetOwnerObject();
            if (obj !is null) obj.SetActive(false);
            return;
        }

        if (!isGrounded) {
            // 重力を適用して落下
            velocity.y -= gravity * GetDeltaTime();

            if (controller !is null) {
                // CharacterController2Dがある場合はコライダーベースで移動と着地判定を行う
                controller.Move(velocity * GetDeltaTime());
                if (controller.IsGrounded() && velocity.y <= 0.0f) {
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
            // 地面に落ちたあとは少し待ってから消滅させる
            groundTimer += GetDeltaTime();
            if (groundTimer >= destroyDelay) {
                Object@ obj = GetOwnerObject();
                if (obj !is null) obj.SetActive(false);
            }
        }
    }

    // プレイヤーなどの動的オブジェクトとぶつかった際の処理
    void OnCollisionEnter(const HitInfo &in hit) {
        // プレイヤーに直撃した場合は即座に岩を消滅させる
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