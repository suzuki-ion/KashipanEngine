
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
    [SerializeField, Tooltip("プレイヤーが滑り始める地面の傾き閾値（接地面の法線Yがこの値未満なら滑る）")]
    float slideThreshold = 0.5f;
    [SerializeField, Tooltip("接地状態を維持する猶予時間（秒）。坂道や動く床で接触が瞬間的に途切れても着地判定が誤爆しないようにする")]
    float groundedGraceTime = 0.1f;
    [SerializeField, Tooltip("接地中に地面へ押し付ける速度。下り坂や下降する床から離れないようにする")]
    float groundStickSpeed = 2.0f;
    [SerializeField, Tooltip("急斜面を滑り落ちる基本加速度（斜面の角度によらず常にかかる分）")]
    float slideAcceleration = 0.5f;
    [SerializeField, Tooltip("斜面の角度に対する滑り加速度の上がり値（sin(斜面角度)にこの値を掛けた分が基本加速度へ加算される。急な斜面ほど速く滑る）")]
    float slideAngleAcceleration = 2.0f;
    [SerializeField, Tooltip("急斜面を滑り落ちる最大速度")]
    float maxSlideSpeed = 10.0f;

    // --- 実行時状態（保存不要） ---
    Vector3 velocity = Vector3(0.0f, 0.0f, 0.0f);
    bool isGrounded = false;       // 猶予時間を考慮した安定した接地状態
    bool wasGrounded = false;
    bool hasGroundContact = false; // このフレームで実際に地面と接触したか（生の値）
    bool isOnSteepSlope = false;   // 接地面が slideThreshold より急かどうか
    float airborneTime = 1000.0f;  // 最後に地面と接触してからの経過時間（秒）
    bool isCollidingWithEnemy = false;
    Vector3 groundHitNormal = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 groundVelocity = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 enemyHitNormal = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 groundSlideVelocity = Vector3(0.0f, 0.0f, 0.0f);
    bool isJumping = false;
    bool wasJumping = false;
    float moveDirection = 0.0f;

    Tag groundColliderTag = Tag("GroundBox");
    Tag enemyColliderTag = Tag("EnemySphere");
    Tag audioSourcePlayerLandingTag = Tag("PlayerLanding");
    Tag particleSystemPlayerLandingTag = Tag("PlayerLanding");

    void Update() {
        const float dt = GetDeltaTime() * GetGameSpeed();

        InputEvent();
        UpdateGroundedState(dt);
        Landing();
        LateralMovement();
        Sliding(dt);
        Jumping(dt);
        ApplyVelocity(dt);

        // 生の接触情報は毎フレームリセットする（次フレームの衝突コールバックで再設定される）
        hasGroundContact = false;
        isCollidingWithEnemy = false;
        // 猶予時間も含めて完全に地面から離れたら、地面由来の情報をリセットする
        if (!isGrounded) {
            groundVelocity = Vector3(0.0f, 0.0f, 0.0f);
            isOnSteepSlope = false;
        }
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
            bool grounded = hit.normal.y > groundedThreshold;
            if (!hasGroundContact && grounded) {
                hasGroundContact = true;
                groundHitNormal = hit.normal;
                // 接地面が閾値より急な斜面なら滑り状態にする
                isOnSteepSlope = hit.normal.y < slideThreshold;
            }

            // もし地面にVelocityコンポーネントが付いていたら、その速度を記録しておき
            // Update()側で移動量に加算する（velocity本体に加算すると接地中に蓄積し続けてしまうため）
            if (grounded) {
                Velocity@ groundVelocityComponent;
                if (hit.otherObject.GetComponent(@groundVelocityComponent)) {
                    groundVelocity = groundVelocityComponent.GetVelocity();
                } else {
                    groundVelocity = Vector3(0.0f, 0.0f, 0.0f);
                }
            }

            // 衝突判定から押し戻しベクトルを計算してプレイヤーを押し戻す
            Transform@ tf = GetTransform();
            if (tf !is null) {
                Vector3 pushBack = hit.normal * hit.penetration;
                pushBack.z = 0.0f;
                // 着地判定がある場合はY方向だけ押し戻す
                if (grounded) {
                    pushBack.x = 0.0f;
                }
                tf.SetTranslate(tf.GetTranslate() + pushBack);
            }
        }
    }

    void OnCollisionExit(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
        }
    }

    //==================================================
    // 内部処理
    //==================================================

    void InputEvent() {
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
    }

    void UpdateGroundedState(const float dt) {
        // 坂道や動く床の上では、押し戻しの結果次のフレームで一瞬だけ接触が途切れることがあり、
        // 生の接触フラグをそのまま接地判定に使うと着地判定が毎フレームのように誤爆してしまう。
        // 「最後に接触してからの経過時間」が猶予時間以内であれば接地扱いとすることで安定させる
        if (hasGroundContact) {
            airborneTime = 0.0f;
        } else {
            airborneTime += dt;
        }
        isGrounded = (airborneTime <= groundedGraceTime);
    }

    void Landing() {
        // 落下中（velocity.y <= 0）に接地した場合のみ着地として扱う
        // （猶予時間で接地状態が安定しているため、坂道・動く床での瞬断では再発火しない）
        if (isGrounded && !wasGrounded && velocity.y <= 0.0f) {
            // 着地時の処理（着地音再生など）
            array<AudioSource@>@ audioSources;
            if (GetComponents(@audioSources)) {
                for (uint i = 0; i < audioSources.length(); i++) {
                    if (audioSources[i].GetTag() == audioSourcePlayerLandingTag) {
                        audioSources[i].Play();
                    }
                }
            }
            array<ParticleSystem3D@>@ particleSystems;
            if (GetComponents(@particleSystems)) {
                for (uint i = 0; i < particleSystems.length(); i++) {
                   if (particleSystems[i].GetTag() == particleSystemPlayerLandingTag) {
                        particleSystems[i].Play();
                    }
                }
            }
        }
        wasGrounded = isGrounded;
    }

    void LateralMovement() {
        // 移動時の法線方向を計算する
        Vector3 targetNormal;
        if (isGrounded) {
            Vector3 tangent(groundHitNormal.y, -groundHitNormal.x, 0.0f);
            targetNormal = tangent.Normalize();
        } else {
            targetNormal = Vector3(1.0f, 0.0f, 0.0f);
        }

        // 左右移動
        // velocityがminとmaxの範囲内であれば加速
        if ((moveDirection != 0.0f)
        && (velocity.x > minVelocity.x && velocity.x < maxVelocity.x)) {
            velocity += targetNormal * moveSpeed * moveDirection;
        } else {
            velocity.x = Easing::Lerp(velocity.x, 0.0f, lateralDeceleration);
            if (isGrounded && !isJumping && !wasJumping) {
                velocity.y = Easing::Lerp(velocity.y, 0.0f, lateralDeceleration);
            }
        }
    }

    void Sliding(const float dt) {
        if (isGrounded && isOnSteepSlope && !isJumping) {
            // 重力を斜面へ投影した方向（法線の向きに関わらず必ず「下り」方向になる）へ加速する
            Vector3 gravityDir(0.0f, -1.0f, 0.0f);
            Vector3 downhill = gravityDir - groundHitNormal * gravityDir.Dot(groundHitNormal);
            downhill.z = 0.0f;
            if (downhill.LengthSquared() > 0.0001f) {
                // 投影ベクトルの長さはちょうど sin(斜面角度) になる（平地で0、垂直壁で1）ため、
                // これを使って「急な斜面ほど強く加速する」ようにする
                float steepness = downhill.Length();
                float acceleration = slideAcceleration + steepness * slideAngleAcceleration;
                groundSlideVelocity += downhill.Normalize() * acceleration * dt * 60.0f;
                // 滑り速度は最大速度でクランプする
                if (groundSlideVelocity.Length() > maxSlideSpeed) {
                    groundSlideVelocity = groundSlideVelocity.Normalize() * maxSlideSpeed;
                }
            }
        } else if ((isGrounded && !isOnSteepSlope) ||
                  (moveDirection < 0.0f && groundSlideVelocity.x > 0.0f) ||
                  (moveDirection > 0.0f && groundSlideVelocity.x < 0.0f)) {
            // 傾斜でない地面に接地しているか、プレイヤーの移動入力があるとき（滑る方向に対して反対の入力があるとき）だけ減速する。
            // ジャンプ中や空中（地面に接していない）ときはリセットせず、滑り速度をそのまま保持する
            groundSlideVelocity = Math::Lerp(groundSlideVelocity, Vector3(0.0f, 0.0f, 0.0f), lateralDeceleration);
        }
    }

    void Jumping(const float dt = GetDeltaTime() * GetGameSpeed()) {
        // 敵に接触している状態で、かつ法線が上向きならジャンプ状態にする
        if (isCollidingWithEnemy && enemyHitNormal.y > enemyCollisionThreshold) {
            isJumping = true;
        }

        // 重力は「実際に接触していないフレーム」で適用する。猶予時間中（isGroundedはtrue）でも
        // 接触が切れている間は重力で地面へ押し戻されるため、坂道・動く床でもすぐ接触が復帰する
        if (!(hasGroundContact || isCollidingWithEnemy)) {
            velocity.y -= gravity * dt * 60.0f;
        }
        if ((isGrounded || isCollidingWithEnemy) && isJumping && !wasJumping) {
            velocity.y = jumpPower;
            // ジャンプ直後は接地の猶予を打ち切り、離陸直後の再着地誤爆を防ぐ
            airborneTime = groundedGraceTime + 1.0f;
            isGrounded = false;
        }
        wasJumping = isJumping;
    }

    void ApplyVelocity(const float dt = GetDeltaTime() * GetGameSpeed()) {
        Transform@ tf = GetTransform();
        if (tf is null) return;
        // 速度の適用
        velocity.x = Clamp(velocity.x, minVelocity.x, maxVelocity.x);
        velocity.y = Clamp(velocity.y, minVelocity.y, maxVelocity.y);
        // 接地中は接地面の法線と逆方向へ押し付けて、下り坂や動く床から離れないようにする
        // （法線と垂直方向なので、斜面に沿った滑り成分は生まれない）
        Vector3 stick(0.0f, 0.0f, 0.0f);
        if (isGrounded && !isJumping && velocity.y <= 0.0f) {
            stick = groundHitNormal * (-groundStickSpeed);
            stick.z = 0.0f;
        }
        // 地面の速度は自分のvelocityとは別に、移動量にだけその場で加算する
        tf.SetTranslate(tf.GetTranslate() + velocity * dt + groundVelocity * dt + groundSlideVelocity * dt + stick * dt);
    }
}
