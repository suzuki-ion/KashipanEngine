
class Player : ScriptComponentBehavior {
    [Header("プレイヤーの基本設定")]

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

    [Header("移動方向追従オブジェクト")]

    [SerializeField, Tooltip("プレイヤーの移動方向の先へ配置するオブジェクト")]
    Object@ movementTargetObject;
    [SerializeField, Tooltip("追従オブジェクトの基準位置に加算するオフセット")]
    Vector3 movementTargetOffset = Vector3(0.0f, 0.0f, 0.0f);
    [SerializeField, Tooltip("プレイヤーの水平速度に掛ける距離倍率")]
    float movementTargetDistanceMultiplier = 1.0f;

    [Header("接地・滑りの設定")]

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

    [Header("プレイヤーの状態")]

    [SerializeField, Tooltip("最大HP")]
    int maxHp = 3;
    [SerializeField, Tooltip("現在のHP")]
    int currentHp = 3;
    [SerializeField, Tooltip("敵との接触で受けるダメージ量")]
    int damageAmount = 1;
    [SerializeField, Tooltip("被ダメージ後、連続でダメージを受けないようにする無敵時間（秒）")]
    float damageCooldown = 1.5f;
    [SerializeField, Tooltip("被ダメージ時のノックバック速度（X: 敵と反対方向の横速度, Y: 上方向のポップ速度）")]
    Vector3 knockbackPower = Vector3(4.0f, 6.0f, 0.0f);
    [SerializeField, Tooltip("被ダメージ時の点滅色")]
    Vector4 damageFlashColor = Vector4(1.0f, 0.2f, 0.2f, 1.0f);
    [SerializeField, Tooltip("被ダメージ時に点滅させる間隔（秒）")]
    float damageFlashInterval = 0.1f;

    // --- 実行時状態（保存不要） ---
    float damageCooldownTimer = 0.0f;
    float damageFlashTimer = 0.0f;
    Vector3 velocity = Vector3(0.0f, 0.0f, 0.0f);
    bool isGrounded = false;       // 猶予時間を考慮した安定した接地状態
    bool wasGrounded = false;
    bool hasGroundContact = false; // このフレームで実際に地面と接触したか（生の値）
    bool isOnSteepSlope = false;   // 接地面が slideThreshold より急かどうか
    float airborneTime = 1000.0f;  // 最後に地面と接触してからの経過時間（秒）
    bool isCollidingWithEnemy = false;
    Vector3 groundHitNormal = Vector3(0.0f, 0.0f, 0.0f);
    // 接地中の地面の、前フレームからの実際の移動量（動く床への追従用。Velocityではなく
    // PreTransformとの差分で求めるため、地面がどのような方法で動いていても追従できる）
    Vector3 groundDelta = Vector3(0.0f, 0.0f, 0.0f);
    // 動く床から離れた瞬間の床の速度（groundDelta / dt）。離れた後も他の床に接触するまで
    // 慣性としてそのまま移動量に加算し続ける（急に静止した空中へ切り替わらないようにする）
    Vector3 platformInertiaVelocity = Vector3(0.0f, 0.0f, 0.0f);
    // 最後に実際に接触していた床の速度。ジャンプ処理でisGroundedが同フレーム中にfalseへ
    // 変化しても床の速度を失わないよう、接触中に毎フレーム保存しておく
    Vector3 lastGroundVelocity = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 enemyHitNormal = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 groundSlideVelocity = Vector3(0.0f, 0.0f, 0.0f);
    bool isJumping = false;
    bool wasJumping = false;
    float moveDirection = 0.0f;

    Tag groundColliderTag = Tag("GroundBox");
    Tag enemyColliderTag = Tag("EnemySphere");
    Tag deathColliderTag = Tag("Death");
    Tag audioSourcePlayerLandingTag = Tag("PlayerLanding");
    Tag particleSystemPlayerLandingTag = Tag("PlayerLanding");

    void Update() {
        const float dt = GetDeltaTime() * GetGameSpeed();
        const bool wasGroundedAtFrameStart = isGrounded;

        InputEvent();
        UpdateGroundedState(dt);
        Landing();
        LateralMovement();
        Sliding(dt);
        Jumping(dt);
        // Jumping()は離陸時にisGroundedをfalseへ変更するため、その後で床の慣性を受け渡す
        UpdatePlatformInertia(dt, wasGroundedAtFrameStart);
        UpdateDamageCooldown(dt);
        CheckEnemyDamage();
        ApplyVelocity(dt);
        UpdateDamageFlash(dt);
        CheckDeath();
        UpdateMovementTarget();

        // 生の接触情報は毎フレームリセットする（次フレームの衝突コールバックで再設定される）
        hasGroundContact = false;
        isCollidingWithEnemy = false;
        // 猶予時間も含めて完全に地面から離れたら、地面由来の情報をリセットする
        if (!isGrounded) {
            groundDelta = Vector3(0.0f, 0.0f, 0.0f);
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
        } else if (hit.otherCollider.GetTag() == deathColliderTag) {
            // 死亡判定のコライダーに触れたら即座にHPを0にする
            currentHp = 0;
        }
    }

    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        // トリガーのコライダーは接触の通知だけを受け取り、接地判定も押し戻しも行わない（すり抜ける）
        if (hit.otherCollider !is null && hit.otherCollider.IsTrigger()) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            // 法線が上向きなら地面に接触しているとみなす
            bool grounded = hit.normal.y > groundedThreshold;
            if (!hasGroundContact && grounded) {
                hasGroundContact = true;
                groundHitNormal = hit.normal;
                // 接地面が閾値より急な斜面なら滑り状態にする
                isOnSteepSlope = hit.normal.y < slideThreshold;
            } else if (!grounded) {
                // 壁・天井との接触。侵入方向（法線と逆向き）へ進もうとしている速度成分だけを
                // 打ち消して、壁に張り付いたまま速度が蓄積し続けたり、天井に頭をぶつけても
                // 上昇速度がそのまま残ってしまったりしないようにする
                // （法線に沿った成分だけを除去するので、斜面沿いの滑り成分等はそのまま残る）
                Vector3 wallNormal = hit.normal;
                wallNormal.z = 0.0f;
                float intoSurface = velocity.Dot(wallNormal);
                if (intoSurface < 0.0f) {
                    velocity = velocity - wallNormal * intoSurface;
                }
                // 床から引き継いだ慣性は、別の壁面へ衝突した時点で終了する
                platformInertiaVelocity = Vector3(0.0f, 0.0f, 0.0f);
                lastGroundVelocity = Vector3(0.0f, 0.0f, 0.0f);
            }

            // 動く床への追従。地面のTransformとPreTransform（前フレームの値）との差分から
            // 実際の移動量を求めておき、Update()側で移動量に加算する（velocity本体に加算すると
            // 接地中に蓄積し続けてしまうため）。Velocityコンポーネントを見る方式と違い、
            // 地面がどんな方法で動いていても（スクリプトで直接Transformを書き換えていても）追従できる
            if (grounded) {
                Transform@ groundTransform;
                PreTransform@ groundPreTransform;
                if (hit.otherObject.GetComponent(@groundTransform) && hit.otherObject.GetComponent(@groundPreTransform)) {
                    groundDelta = groundTransform.GetTranslate() - groundPreTransform.GetPreviousTranslate();
                } else {
                    groundDelta = Vector3(0.0f, 0.0f, 0.0f);
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

    // 実際に床へ接触している間に最後の床速度を保存し、接地状態から離れた瞬間に慣性へ受け渡す。
    // groundDelta自体は離床フレームの末尾でリセットされるため、離床後にgroundDeltaから
    // 速度を求めようとするとゼロになる。接触中から保存しておくことでジャンプ時にも維持できる
    void UpdatePlatformInertia(const float dt, const bool wasGroundedAtFrameStart) {
        if (hasGroundContact && dt > 0.0f) {
            lastGroundVelocity = groundDelta / dt;
        }

        if (isGrounded) {
            // 同じ床に乗っている間はgroundDeltaで追従するため、慣性分は別途加算しない。
            // 別の床へ着地した場合もここでそれまでの慣性が終了する
            platformInertiaVelocity = Vector3(0.0f, 0.0f, 0.0f);
        } else if (wasGroundedAtFrameStart) {
            platformInertiaVelocity = lastGroundVelocity;
        }
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

    void UpdateDamageCooldown(const float dt) {
        if (damageCooldownTimer > 0.0f) {
            damageCooldownTimer -= dt;
        }
    }

    // 敵の上以外から触れた場合にダメージを受ける（上から踏んだ場合はJumping()側のバウンド処理に任せる）
    void CheckEnemyDamage() {
        if (damageCooldownTimer > 0.0f) return;
        if (!isCollidingWithEnemy) return;
        if (enemyHitNormal.y > enemyCollisionThreshold) return; // 上から接触＝踏みなのでダメージなし

        TakeDamage(damageAmount, enemyHitNormal);
    }

    void TakeDamage(int amount, const Vector3 &in hitNormal) {
        currentHp -= amount;
        damageCooldownTimer = damageCooldown;
        damageFlashTimer = damageCooldown;

        // 敵と反対方向へノックバック（横方向は接触方向、縦方向は常に上向きへポップさせる）
        float knockbackDirX = (hitNormal.x != 0.0f) ? Sign(hitNormal.x) : -Sign(moveDirection);
        velocity.x = knockbackDirX * knockbackPower.x;
        velocity.y = knockbackPower.y;
    }

    // 無敵時間中、プレイヤーの色を点滅させる
    void UpdateDamageFlash(const float dt) {
        if (damageFlashTimer <= 0.0f) return;
        damageFlashTimer -= dt;

        if (damageFlashTimer <= 0.0f) {
            damageFlashTimer = 0.0f;
            ResetDamageFlash();
            return;
        }

        MeshRenderer@ meshRenderer;
        if (!GetComponent(@meshRenderer)) return;
        bool blinkOn = (int(damageFlashTimer / damageFlashInterval) % 2) == 0;
        if (blinkOn) {
            meshRenderer.SetInstanceColor(damageFlashColor);
            meshRenderer.SetInstanceColorBlendMode(0); // Override
        } else {
            meshRenderer.SetInstanceColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            meshRenderer.SetInstanceColorBlendMode(1); // Multiply（見た目に影響しない）
        }
    }

    void ResetDamageFlash() {
        MeshRenderer@ meshRenderer;
        if (!GetComponent(@meshRenderer)) return;
        meshRenderer.SetInstanceColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        meshRenderer.SetInstanceColorBlendMode(1); // Multiply（見た目に影響しない）
    }

    // HPが0以下になった場合、シーン変数に保存されたチェックポイント座標からリスポーンする
    void CheckDeath() {
        if (currentHp > 0) return;

        Transform@ tf = GetTransform();
        Scene@ scene = GetScene();
        Vector3 respawnPosition;
        bool hasCheckpoint = (scene !is null) && scene.GetVariable("CheckpointPosition", respawnPosition);
        if (tf !is null && hasCheckpoint) {
            tf.SetTranslate(respawnPosition);
        }

        velocity = Vector3(0.0f, 0.0f, 0.0f);
        groundSlideVelocity = Vector3(0.0f, 0.0f, 0.0f);
        platformInertiaVelocity = Vector3(0.0f, 0.0f, 0.0f);
        lastGroundVelocity = Vector3(0.0f, 0.0f, 0.0f);
        currentHp = maxHp;
        damageCooldownTimer = damageCooldown; // リスポーン直後の無敵猶予として流用
        damageFlashTimer = 0.0f;
        ResetDamageFlash();
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
        // 接地中は床の実移動量、離床後は最後に記録した床速度による慣性のどちらか一方だけを
        // 適用する。離床フレームに両方を足すと床の移動が二重に加算されてしまう
        Vector3 platformMovement = isGrounded ? groundDelta : platformInertiaVelocity * dt;
        tf.SetTranslate(tf.GetTranslate() + velocity * dt + platformMovement + groundSlideVelocity * dt + stick * dt);
    }

    // 指定オブジェクトを、プレイヤーの現在位置を基準に水平移動速度の分だけ進行方向へ配置する。
    // 速度が大きいほど movementTargetDistanceMultiplier に比例して距離が広がる
    void UpdateMovementTarget() {
        if (movementTargetObject is null) return;

        Transform@ playerTransform = GetTransform();
        Transform@ targetTransform;
        if (playerTransform is null || !movementTargetObject.GetComponent(@targetTransform)) return;

        Vector3 horizontalVelocity = velocity + groundSlideVelocity;
        horizontalVelocity += isGrounded ? lastGroundVelocity : platformInertiaVelocity;
        horizontalVelocity.y = 0.0f;

        const Vector3 desiredWorldPosition =
            playerTransform.GetWorldPosition()
            + movementTargetOffset
            + horizontalVelocity * movementTargetDistanceMultiplier;
        const Vector3 targetWorldDelta = desiredWorldPosition - targetTransform.GetWorldPosition();
        targetTransform.SetTranslate(targetTransform.GetTranslate() + targetWorldDelta);
    }
}
