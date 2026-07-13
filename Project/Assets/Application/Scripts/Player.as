
float Clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

class Player : ScriptComponentBehavior {
    // --- PlayerMovement 相当のパラメータ ---
    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 0.1f;
    [SerializeField, Tooltip("ジャンプ力")]
    float jumpPower = 16.0f;
    [SerializeField, Tooltip("重力")]
    float gravity = 0.2f;
    [SerializeField, Tooltip("横方向の減速")]
    float lateralDeceleration = 0.1f;
    [SerializeField, Tooltip("最小速度")]
    Vector3 minVelocity = Vector3(-8.0f, -16.0f, -8.0f);
    [SerializeField, Tooltip("最大速度")]
    Vector3 maxVelocity = Vector3(8.0f, 16.0f, 8.0f);

    // --- PlayerCollision 相当のパラメータ ---
    // 法線のy成分がこの値以上なら地面と判定する
    [SerializeField, Tooltip("地面との接触判定閾値")]
    float groundedThreshold = 0.4f;

    // --- 実行時状態（保存不要） ---
    Vector3 velocity = Vector3(0.0f, 0.0f, 0.0f);
    bool isGrounded = false;
    bool isCollidingWithEnemy = false;
    Vector3 hitNormal = Vector3(0.0f, 0.0f, 0.0f);

    bool isJumping = false;
    bool wasJumping = false;
    bool isMoveLeft = false;
    bool isMoveRight = false;
    float moveLeftInput = 0.0f;
    float moveRightInput = 0.0f;

    Tag groundColliderTag = Tag("GroundBox");
    Tag enemyColliderTag = Tag("EnemySphere");

    Tag audioSourcePlayerLandingTag = Tag("PlayerLanding");

    void Start() {
        Log("Player start: " + GetOwnerObject().GetName());
    }

    void Update() {
        // --- PlayerInputHandler 相当 ---
        if (IsCommandTriggered("PlayerMoveLeft")) {
            isMoveLeft = true;
            moveLeftInput = GetCommandValue("PlayerMoveLeft");
        } else {
            isMoveLeft = false;
            moveLeftInput = 0.0f;
        }
        if (IsCommandTriggered("PlayerMoveRight")) {
            isMoveRight = true;
            moveRightInput = GetCommandValue("PlayerMoveRight");
        } else {
            isMoveRight = false;
            moveRightInput = 0.0f;
        }
        isJumping = IsCommandTriggered("PlayerJump");

        Transform@ tf = GetTransform();
        if (tf is null) return;

        // --- PlayerEnemyJump 相当（敵の上に乗ったら踏みつけジャンプ） ---
        // isCollidingWithEnemy は「このフレーム中に敵と衝突した」というパルスなので、
        // 使用したら PlayerCollision::Update() 相当としてリセットする
        bool enemyContact = isCollidingWithEnemy;
        isCollidingWithEnemy = false;
        if (enemyContact && hitNormal.y < 0.5f) {
            isJumping = true;
        }

        // --- PlayerMovement 相当 ---
        float dt = GetDeltaTime() * GetGameSpeed();

        Vector3 targetNormal;
        if (isGrounded) {
            Vector3 tangent(hitNormal.y, -hitNormal.x, 0.0f);
            targetNormal = tangent.Normalize();
        } else {
            targetNormal = Vector3(1.0f, 0.0f, 0.0f);
        }

        // 左右移動
        if (isMoveLeft) {
            velocity -= targetNormal * moveSpeed * moveLeftInput;
        } else if (isMoveRight) {
            velocity += targetNormal * moveSpeed * moveRightInput;
        } else {
            velocity.x = Easing::Lerp(velocity.x, 0.0f, lateralDeceleration);
            if (isGrounded && !isJumping && !wasJumping) {
                velocity.y = Easing::Lerp(velocity.y, 0.0f, lateralDeceleration);
            }
        }

        // 重力とジャンプ
        if (!(isGrounded || enemyContact)) {
            velocity.y -= gravity;
        }
        if ((isGrounded || enemyContact) && isJumping && !wasJumping) {
            velocity.y = jumpPower;
        }
        wasJumping = isJumping;

        // 速度の適用
        velocity.x = Clampf(velocity.x, minVelocity.x, maxVelocity.x);
        velocity.y = Clampf(velocity.y, minVelocity.y, maxVelocity.y);
        tf.SetTranslate(tf.GetTranslate() + velocity * dt);
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            array<AudioSource@>@ audioSources;
            if (GetComponents(@audioSources)) {
                for (uint i = 0; i < audioSources.length(); i++) {
                    AudioSource@ audioSource = audioSources[i];
                        audioSource.Play();
                }
            }
        } else if (hit.otherCollider.GetTag() == enemyColliderTag) {
            isCollidingWithEnemy = true;
        }
    }

    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            // 法線が上向きなら地面に接触しているとみなす
            isGrounded = hit.normal.y > groundedThreshold;

            // もし地面にVelocityコンポーネントが付いていたら、そのVelocityをプレイヤーに加算する
            Velocity@ groundVelocity;
            if (hit.otherObject.GetComponent(@groundVelocity)) {
                velocity += groundVelocity.GetVelocity();
            }
            
            // 衝突判定から押し戻しベクトルを計算してプレイヤーを押し戻す
            Transform@ tf = GetTransform();
            if (tf !is null) {
                Vector3 pushBack = hit.normal * hit.penetration;
                pushBack.z = 0.0f;
                // 着地判定がある場合はY方向だけ押し戻す
                if (isGrounded) {
                    pushBack.x = 0.0f;
                    pushBack.z = 0.0f;
                }
                tf.SetTranslate(tf.GetTranslate() + pushBack);
            }
        }
        hitNormal = hit.normal;
    }

    void OnCollisionExit(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            isGrounded = false;
        }
    }

    void End() {
        Log("Player end");
    }
}
