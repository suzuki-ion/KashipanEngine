
class Player : ScriptComponentBehavior {
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

    [SerializeField, Tooltip("地面との接触判定閾値")]
    float groundedThreshold = 0.4f;
    [SerializeField, Tooltip("敵との接触判定閾値")]
    float enemyCollisionThreshold = 0.4f;

    // --- 実行時状態（保存不要） ---
    Vector3 velocity = Vector3(0.0f, 0.0f, 0.0f);
    bool isGrounded = false;
    bool wasGrounded = false;
    bool isCollidingWithEnemy = false;
    Vector3 groundHitNormal = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 groundVelocity = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 enemyHitNormal = Vector3(0.0f, 0.0f, 0.0f);

    bool isJumping = false;
    bool wasJumping = false;

    float moveDirection = 0.0f;

    Tag groundColliderTag = Tag("GroundBox");
    Tag enemyColliderTag = Tag("EnemySphere");

    Tag audioSourcePlayerLandingTag = Tag("PlayerLanding");

    void Start() {
        Log("Player start: " + GetOwnerObject().GetName());
    }

    void Update() {
        const float dt = GetDeltaTime() * GetGameSpeed();
        Transform@ tf = GetTransform();
        if (tf is null) return;

        // 左右移動入力
        moveDirection = 0.0f;
        if (IsCommandTriggered("PlayerMoveLeft")) {
            moveDirection = GetCommandValue("PlayerMoveLeft");
        }
        if (IsCommandTriggered("PlayerMoveRight")) {
            moveDirection = GetCommandValue("PlayerMoveRight");
        }
        // ジャンプ入力
        isJumping = IsCommandTriggered("PlayerJump");

        // 敵に接触している状態で、かつ法線が上向きならジャンプ状態にする
        if (isCollidingWithEnemy && enemyHitNormal.y > enemyCollisionThreshold) {
            isJumping = true;
        }

        // 地面着地時の着地音再生
        if (isGrounded && !wasGrounded) {
            array<AudioSource@>@ audioSources;
            if (GetComponents(@audioSources)) {
                for (uint i = 0; i < audioSources.length(); i++) {
                    if (audioSources[i].GetTag() == audioSourcePlayerLandingTag) {
                        audioSources[i].Play();
                    }
                }
            }
        }
        wasGrounded = isGrounded;

        // 移動時の法線方向を計算する
        Vector3 targetNormal;
        if (isGrounded) {
            Vector3 tangent(groundHitNormal.y, -groundHitNormal.x, 0.0f);
            targetNormal = tangent.Normalize();
        } else {
            targetNormal = Vector3(1.0f, 0.0f, 0.0f);
        }

        // 左右移動
        if (moveDirection < 0.0f || moveDirection > 0.0f) {
            velocity -= targetNormal * moveSpeed * moveDirection;
        } else {
            velocity.x = Easing::Lerp(velocity.x, 0.0f, lateralDeceleration);
            if (isGrounded && !isJumping && !wasJumping) {
                velocity.y = Easing::Lerp(velocity.y, 0.0f, lateralDeceleration);
            }
        }

        // 重力とジャンプ
        if (!(isGrounded || isCollidingWithEnemy)) {
            velocity.y -= gravity * dt * 60.0f;
        }
        if ((isGrounded || isCollidingWithEnemy) && isJumping && !wasJumping) {
            velocity.y = jumpPower;
        }
        wasJumping = isJumping;

        // 速度の適用
        velocity.x = Clamp(velocity.x, minVelocity.x, maxVelocity.x);
        velocity.y = Clamp(velocity.y, minVelocity.y, maxVelocity.y);
        // 地面の速度は自分のvelocityとは別に、移動量にだけその場で加算する
        tf.SetTranslate(tf.GetTranslate() + velocity * dt + groundVelocity * dt);

        isGrounded = false;
        isCollidingWithEnemy = false;
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
        } else if (hit.otherCollider.GetTag() == enemyColliderTag) {
            if (!isCollidingWithEnemy) {
                isCollidingWithEnemy = true;
                enemyHitNormal = hit.normal;
            }
        }
    }

    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            // 法線が上向きなら地面に接触しているとみなす
            if (!isGrounded) {
                isGrounded = hit.normal.y > groundedThreshold;
                groundHitNormal = hit.normal;
            }

            // もし地面にVelocityコンポーネントが付いていたら、その速度を記録しておき
            // Update()側で移動量に加算する（velocity本体に加算すると接地中に蓄積し続けてしまうため）
            Velocity@ groundVelocityComponent;
            if (hit.otherObject.GetComponent(@groundVelocityComponent)) {
                groundVelocity = groundVelocityComponent.GetVelocity();
            } else {
                groundVelocity = Vector3(0.0f, 0.0f, 0.0f);
            }

            // 衝突判定から押し戻しベクトルを計算してプレイヤーを押し戻す
            Transform@ tf = GetTransform();
            if (tf !is null) {
                Vector3 pushBack = hit.normal * hit.penetration;
                pushBack.z = 0.0f;
                // 着地判定がある場合はY方向だけ押し戻す
                if (hit.normal.y > groundedThreshold) {
                    pushBack.x = 0.0f;
                }
                tf.SetTranslate(tf.GetTranslate() + pushBack);
            }
        }
    }

    void OnCollisionExit(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            isGrounded = false;
            groundVelocity = Vector3(0.0f, 0.0f, 0.0f);
        }
    }

    void End() {
        Log("Player end");
    }
}
