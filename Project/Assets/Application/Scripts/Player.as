
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
    // 直前フレームのisGrounded（着地の瞬間＝false→trueを検知するため）
    bool wasGrounded = false;
    // このフレーム中に地面用コライダーのOnCollisionStayが1回でも呼ばれたか。
    // 地面が複数のコライダーに分かれている場合、境目をまたぐ瞬間は
    // 「Aからは離れたがBとはまだ接触している」ことがあり、Stay(B)より後に
    // Exit(A)が呼ばれてisGroundedを誤ってfalseに戻してしまうことがある
    // （Dispatch側は同フレーム内で必ずStay→Exitの順に呼ぶため、このフラグで防止する）
    bool groundedThisPhysicsFrame = false;
    bool isCollidingWithEnemy = false;
    Vector3 hitNormal = Vector3(0.0f, 0.0f, 0.0f);
    // 乗っている地面(Velocityコンポーネント付き)の現在の速度。
    // プレイヤー自身のvelocityには合算せず、移動量にだけ毎フレーム加算することで
    // 地面の速度にそのまま追従させる（velocityに加算すると接地中ずっと蓄積され続けてしまう）
    Vector3 groundVelocity = Vector3(0.0f, 0.0f, 0.0f);

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
        if (enemyContact && hitNormal.y > groundedThreshold) {
            isJumping = true;
        }

        // 着地の瞬間（isGroundedがfalse→trueに変わった時）にのみ着地音を鳴らす。
        // OnCollisionEnterで鳴らすと、地面が複数のコライダーに分かれている場合に
        // セグメントの境目を跨ぐたびに新規接触として扱われ、歩いているだけで
        // 何度も音が鳴ってしまうため、状態遷移で判定する
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
        // 重力はdt（経過秒数）を掛けてフレームレートに依存しないようにする。
        // 元々は「1フレームあたり」の値として調整されていたため、60fps基準で
        // 換算が変わらないよう *60 でスケールしている（フレームレートが
        // 変動するとジャンプの高さや滞空時間が安定しなかった原因の一つ）
        if (!(isGrounded || enemyContact)) {
            velocity.y -= gravity * dt * 60.0f;
        }
        if ((isGrounded || enemyContact) && isJumping && !wasJumping) {
            velocity.y = jumpPower;
        }
        wasJumping = isJumping;

        // 速度の適用
        velocity.x = Clampf(velocity.x, minVelocity.x, maxVelocity.x);
        velocity.y = Clampf(velocity.y, minVelocity.y, maxVelocity.y);
        // 地面の速度は自分のvelocityとは別に、移動量にだけその場で加算する
        tf.SetTranslate(tf.GetTranslate() + velocity * dt + groundVelocity * dt);

        // 次フレームの誤判定防止用フラグをリセットする
        groundedThisPhysicsFrame = false;
        isGrounded = false;
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == enemyColliderTag) {
            isCollidingWithEnemy = true;
        }
    }

    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            // 地面が複数のコライダーに分かれているため、このフレーム中に
            // 少なくとも1回は地面と接触したことを記録しておく
            // （OnCollisionExitでの誤ったisGrounded解除を防ぐため）
            groundedThisPhysicsFrame = true;

            // 法線が上向きなら地面に接触しているとみなす
            if (!isGrounded) {
                isGrounded = hit.normal.y > groundedThreshold;
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
            // 同フレーム中に別の地面コライダーとの接触(OnCollisionStay)が
            // 既に記録されていれば、実際には地面から離れていないので無視する
            // （地面セグメントの境目を跨ぐ瞬間、AのExitがBのStayより後に届くことがある）
            if (!groundedThisPhysicsFrame) {
                isGrounded = false;
                groundVelocity = Vector3(0.0f, 0.0f, 0.0f);
            }
        }
    }

    void End() {
        Log("Player end");
    }
}
