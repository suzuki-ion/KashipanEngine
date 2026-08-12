// プレイヤーの接地・斜面判定、移動、ジャンプ、速度適用を担当する
// 設定値（moveSpeed 等）は Player 側の [SerializeField] をそのまま参照する
class PlayerMovement {
    Player@ owner;

    Vector3 velocity = Vector3(0.0f, 0.0f, 0.0f);
    bool isGrounded = false;       // 猶予時間を考慮した安定した接地状態
    bool wasGrounded = false;
    bool hasGroundContact = false; // このフレームで実際に地面と接触したか（生の値）
    bool isOnSteepSlope = false;   // 接地面が slideThreshold より急かどうか
    float airborneTime = 1000.0f;  // 最後に地面と接触してからの経過時間（秒）
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
    Vector3 groundSlideVelocity = Vector3(0.0f, 0.0f, 0.0f);
    bool isJumping = false;
    bool wasJumping = false;

    Tag groundColliderTag = Tag("GroundBox");
    Tag audioSourcePlayerLandingTag = Tag("PlayerLanding");
    Tag particleSystemPlayerLandingTag = Tag("PlayerLanding");

    PlayerMovement(Player@ inOwner) {
        @owner = inOwner;
    }

    void UpdateGroundedState(float dt) {
        // 坂道や動く床の上では、押し戻しの結果次のフレームで一瞬だけ接触が途切れることがあり、
        // 生の接触フラグをそのまま接地判定に使うと着地判定が毎フレームのように誤爆してしまう。
        // 「最後に接触してからの経過時間」が猶予時間以内であれば接地扱いとすることで安定させる
        if (hasGroundContact) {
            airborneTime = 0.0f;
        } else {
            airborneTime += dt;
        }
        isGrounded = (airborneTime <= owner.groundedGraceTime);
    }

    // 実際に床へ接触している間に最後の床速度を保存し、接地状態から離れた瞬間に慣性へ受け渡す。
    // groundDelta自体は離床フレームの末尾でリセットされるため、離床後にgroundDeltaから
    // 速度を求めようとするとゼロになる。接触中から保存しておくことでジャンプ時にも維持できる
    void UpdatePlatformInertia(float dt, bool wasGroundedAtFrameStart) {
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

    void LateralMovement(float moveDirection) {
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
        && (velocity.x > owner.minVelocity.x && velocity.x < owner.maxVelocity.x)) {
            velocity += targetNormal * owner.moveSpeed * moveDirection;
        } else {
            velocity.x = Easing::Lerp(velocity.x, 0.0f, owner.lateralDeceleration);
            if (isGrounded && !isJumping && !wasJumping) {
                velocity.y = Easing::Lerp(velocity.y, 0.0f, owner.lateralDeceleration);
            }
        }
    }

    void Sliding(float dt, float moveDirection) {
        if (isGrounded && isOnSteepSlope && !isJumping) {
            // 重力を斜面へ投影した方向（法線の向きに関わらず必ず「下り」方向になる）へ加速する
            Vector3 gravityDir(0.0f, -1.0f, 0.0f);
            Vector3 downhill = gravityDir - groundHitNormal * gravityDir.Dot(groundHitNormal);
            downhill.z = 0.0f;
            if (downhill.LengthSquared() > 0.0001f) {
                // 投影ベクトルの長さはちょうど sin(斜面角度) になる（平地で0、垂直壁で1）ため、
                // これを使って「急な斜面ほど強く加速する」ようにする
                float steepness = downhill.Length();
                float acceleration = owner.slideAcceleration + steepness * owner.slideAngleAcceleration;
                groundSlideVelocity += downhill.Normalize() * acceleration * dt * 60.0f;
                // 滑り速度は最大速度でクランプする
                if (groundSlideVelocity.Length() > owner.maxSlideSpeed) {
                    groundSlideVelocity = groundSlideVelocity.Normalize() * owner.maxSlideSpeed;
                }
            }
        } else if ((isGrounded && !isOnSteepSlope) ||
                  (moveDirection < 0.0f && groundSlideVelocity.x > 0.0f) ||
                  (moveDirection > 0.0f && groundSlideVelocity.x < 0.0f)) {
            // 傾斜でない地面に接地しているか、プレイヤーの移動入力があるとき（滑る方向に対して反対の入力があるとき）だけ減速する。
            // ジャンプ中や空中（地面に接していない）ときはリセットせず、滑り速度をそのまま保持する
            groundSlideVelocity = Math::Lerp(groundSlideVelocity, Vector3(0.0f, 0.0f, 0.0f), owner.lateralDeceleration);
        }
    }

    // isCollidingWithEnemy/enemyHitNormal は PlayerCombat が同フレームで検知した敵接触情報
    void Jumping(float dt, bool isCollidingWithEnemy, const Vector3 &in enemyHitNormal) {
        // 敵に接触している状態で、かつ法線が上向きならジャンプ状態にする
        if (isCollidingWithEnemy && enemyHitNormal.y > owner.enemyCollisionThreshold) {
            isJumping = true;
        }

        // 重力は「実際に接触していないフレーム」で適用する。猶予時間中（isGroundedはtrue）でも
        // 接触が切れている間は重力で地面へ押し戻されるため、坂道・動く床でもすぐ接触が復帰する
        if (!(hasGroundContact || isCollidingWithEnemy)) {
            velocity.y -= owner.gravity * dt * 60.0f;
        }
        if ((isGrounded || isCollidingWithEnemy) && isJumping && !wasJumping) {
            velocity.y = owner.jumpPower;
            // ジャンプ直後は接地の猶予を打ち切り、離陸直後の再着地誤爆を防ぐ
            airborneTime = owner.groundedGraceTime + 1.0f;
            isGrounded = false;
        }
        wasJumping = isJumping;
    }

    void ApplyVelocity(float dt) {
        Transform@ tf = GetTransform();
        if (tf is null) return;
        // 速度の適用
        velocity.x = Clamp(velocity.x, owner.minVelocity.x, owner.maxVelocity.x);
        velocity.y = Clamp(velocity.y, owner.minVelocity.y, owner.maxVelocity.y);
        // 接地中は接地面の法線と逆方向へ押し付けて、下り坂や動く床から離れないようにする
        // （法線と垂直方向なので、斜面に沿った滑り成分は生まれない）
        Vector3 stick(0.0f, 0.0f, 0.0f);
        if (isGrounded && !isJumping && velocity.y <= 0.0f) {
            stick = groundHitNormal * (-owner.groundStickSpeed);
            stick.z = 0.0f;
        }
        // 接地中は床の実移動量、離床後は最後に記録した床速度による慣性のどちらか一方だけを
        // 適用する。離床フレームに両方を足すと床の移動が二重に加算されてしまう
        Vector3 platformMovement = isGrounded ? groundDelta : platformInertiaVelocity * dt;
        tf.SetTranslate(tf.GetTranslate() + velocity * dt + platformMovement + groundSlideVelocity * dt + stick * dt);
    }

    // カメラ等の追従先を配置するための水平速度（滑り・床追従・慣性を合算し、Y成分を除いたもの）
    Vector3 GetHorizontalVelocity() const {
        Vector3 horizontalVelocity = velocity + groundSlideVelocity;
        horizontalVelocity += isGrounded ? lastGroundVelocity : platformInertiaVelocity;
        horizontalVelocity.y = 0.0f;
        return horizontalVelocity;
    }

    // 死亡・リスポーン時に移動関連の速度を全てリセットする
    void ResetVelocities() {
        velocity = Vector3(0.0f, 0.0f, 0.0f);
        groundSlideVelocity = Vector3(0.0f, 0.0f, 0.0f);
        platformInertiaVelocity = Vector3(0.0f, 0.0f, 0.0f);
        lastGroundVelocity = Vector3(0.0f, 0.0f, 0.0f);
    }

    void HandleCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        // トリガーのコライダーは接触の通知だけを受け取り、接地判定も押し戻しも行わない（すり抜ける）
        if (hit.otherCollider !is null && hit.otherCollider.IsTrigger()) return;
        if (hit.otherCollider.GetTag() != groundColliderTag) return;

        // 法線が上向きなら地面に接触しているとみなす
        bool grounded = hit.normal.y > owner.groundedThreshold;
        if (!hasGroundContact && grounded) {
            hasGroundContact = true;
            groundHitNormal = hit.normal;
            // 接地面が閾値より急な斜面なら滑り状態にする
            isOnSteepSlope = hit.normal.y < owner.slideThreshold;
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
