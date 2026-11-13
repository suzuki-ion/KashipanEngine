// プレイヤーの接地状態（Airborne/Grounded/Sliding）
enum PlayerGroundState {
    Airborne, // 空中（猶予時間を含めて地面と接触していない）
    Grounded, // 平坦〜緩やかな地面に接地している
    Sliding   // 急斜面に接地していて滑り落ちる
}

// プレイヤーの接地状態遷移、移動、ジャンプ、速度適用を担当する
// 設定値（moveSpeed 等）は Player 側の [SerializeField] をそのまま参照する
//
// 速度は役割ごとに3つのベクトルへ集約している（以前はgroundDelta/platformInertiaVelocity/
// lastGroundVelocityの3つに分かれていたが、「乗っている/最後に乗っていた床の速度」という
// 同じ概念なのでsurfaceVelocity 1つへ統合した）:
//   - velocity:        自機自身の運動速度（横移動・重力・ジャンプ・ノックバック）
//   - surfaceVelocity: 床由来の速度（接地中は毎フレーム実測値へ更新され、離床した瞬間の
//                       値がそのまま慣性としてairborne中も使われ続ける。着地すると再び
//                       実測値で上書きされるため、慣性の開始・終了を別途管理する必要が無い）
//   - slideVelocity:   急斜面を滑り落ちる速度
class PlayerMovement {
    Player@ owner;
    PlayerTransformation@ transformation;

    PlayerGroundState state = PlayerGroundState::Airborne;
    bool wasGrounded = false;           // 直前フレームが接地状態だったか（着地検出用）
    bool hasGroundContact = false;      // このフレームで実際に地面と接触したか（生の値）
    bool isOnSteepSlopeContact = false; // 直近の地面接触が急斜面だったか
    float airborneTime = 1000.0f;       // 最後に地面と接触してからの経過時間（秒）
    Vector3 groundHitNormal = Vector3(0.0f, 0.0f, 0.0f);
    // このフレームで測定した床の移動量（HandleCollisionStayで設定し、UpdateGroundStateで
    // surfaceVelocityへ変換する。hasGroundContactがfalseの間は読まれないため明示的なリセットは不要）
    Vector3 rawGroundDelta = Vector3(0.0f, 0.0f, 0.0f);

    Vector3 velocity = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 surfaceVelocity = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 slideVelocity = Vector3(0.0f, 0.0f, 0.0f);

    bool isJumping = false;
    bool wasJumping = false;

    Tag groundColliderTag = Tag("GroundBox");
    Tag audioSourcePlayerLandingTag = Tag("PlayerLanding");
    Tag particleSystemPlayerLandingTag = Tag("PlayerLanding");

    PlayerMovement(Player@ inOwner, PlayerTransformation@ inTransformation) {
        @owner = inOwner;
        @transformation = inTransformation;
    }

    bool IsGrounded() const {
        return state != PlayerGroundState::Airborne;
    }

    // 接地の猶予判定・状態遷移・床速度の実測・着地演出をまとめて行う
    void UpdateGroundState(float dt) {
        ProbeGround();
        // 坂道や動く床の上では、押し戻しの結果次のフレームで一瞬だけ接触が途切れることがあり、
        // 生の接触フラグをそのまま接地判定に使うと着地判定が毎フレームのように誤爆してしまう。
        // 「最後に接触してからの経過時間」が猶予時間以内であれば接地扱いとすることで安定させる
        if (hasGroundContact) {
            airborneTime = 0.0f;
        } else {
            airborneTime += dt;
        }

        // 接地中の床の速度を実測する（動く床への追従・離床後の慣性の両方に使う）。
        // 猶予期間中で今フレーム接触が無い場合は、前回接触時に測定した値をそのまま使い続ける
        if (hasGroundContact && dt > 0.0f) {
            surfaceVelocity = rawGroundDelta.LengthSquared()<=4 ? rawGroundDelta / dt : Vector3();
        }

        const bool grounded = (airborneTime <= owner.groundedGraceTime);
        const PlayerGroundState newState = !grounded
            ? PlayerGroundState::Airborne
            : ((isOnSteepSlopeContact && transformation.CanSlide()) ? PlayerGroundState::Sliding : PlayerGroundState::Grounded);

        // 落下中（velocity.y <= 0）に接地した場合のみ着地として扱う
        // （猶予時間で接地状態が安定しているため、坂道・動く床での瞬断では再発火しない）
        if (grounded && !wasGrounded && velocity.y <= 0.0f) {
            PlayLandingFeedback();
        }

        state = newState;
        wasGrounded = grounded;
    }

    void PlayLandingFeedback() {
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

    void LateralMovement(float moveDirection) {
        // 移動時の法線方向を計算する
        Vector3 targetNormal;
        if (IsGrounded()) {
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
            if (IsGrounded() && !isJumping && !wasJumping) {
                velocity.y = Easing::Lerp(velocity.y, 0.0f, owner.lateralDeceleration);
            }
        }
    }

    void UpdateSlideVelocity(float dt, float moveDirection) {
        if (state == PlayerGroundState::Sliding && !isJumping) {
            // 重力を斜面へ投影した方向（法線の向きに関わらず必ず「下り」方向になる）へ加速する
            Vector3 gravityDir(0.0f, -1.0f, 0.0f);
            Vector3 downhill = gravityDir - groundHitNormal * gravityDir.Dot(groundHitNormal);
            downhill.z = 0.0f;
            if (downhill.LengthSquared() > 0.0001f) {
                // 投影ベクトルの長さはちょうど sin(斜面角度) になる（平地で0、垂直壁で1）ため、
                // これを使って「急な斜面ほど強く加速する」ようにする
                float steepness = downhill.Length();
                float acceleration = owner.slideAcceleration + steepness * owner.slideAngleAcceleration;
                slideVelocity += downhill.Normalize() * acceleration * dt * 60.0f;
                // 滑り速度は最大速度でクランプする
                if (slideVelocity.Length() > owner.maxSlideSpeed) {
                    slideVelocity = slideVelocity.Normalize() * owner.maxSlideSpeed;
                }
            }
        } else if (state == PlayerGroundState::Grounded ||
                  (moveDirection < 0.0f && slideVelocity.x > 0.0f) ||
                  (moveDirection > 0.0f && slideVelocity.x < 0.0f)) {
            // 傾斜でない地面に接地しているか、プレイヤーの移動入力があるとき（滑る方向に対して反対の入力があるとき）だけ減速する。
            // ジャンプ中や空中（地面に接していない）ときはリセットせず、滑り速度をそのまま保持する
            slideVelocity = Math::Lerp(slideVelocity, Vector3(0.0f, 0.0f, 0.0f), owner.lateralDeceleration);
        }
    }

    // isCollidingWithEnemy/enemyHitNormal は PlayerCombat が同フレームで検知した敵接触情報
    void UpdateVerticalMotion(float dt, bool isCollidingWithEnemy, const Vector3 &in enemyHitNormal, bool forceEnemyBounce) {
        // 通常の上からの踏みつけ、またはConeでの敵への突進ならジャンプ状態にする
        bool bouncedOnEnemy = isCollidingWithEnemy &&
            (enemyHitNormal.y > owner.enemyCollisionThreshold || forceEnemyBounce);
        if (bouncedOnEnemy) {
            isJumping = true;
            // 敵へのバウンドはジャンプ入力の前フレーム状態に依存させず、必ず同じ強さで跳ねる。
            // Coneがジャンプ中に敵へ突進した場合でも、ここで確実に上向き速度へ上書きする。
            velocity.y = owner.jumpPower;
            airborneTime = owner.groundedGraceTime + 1.0f;
            state = PlayerGroundState::Airborne;
        }

        // 重力は「実際に接触していないフレーム」で適用する。猶予時間中（接地扱い）でも
        // 接触が切れている間は重力で地面へ押し戻されるため、坂道・動く床でもすぐ接触が復帰する
        if (!(hasGroundContact || isCollidingWithEnemy)) {
            velocity.y -= owner.gravity * dt * 60.0f;
        }
        if (!bouncedOnEnemy && (IsGrounded() || isCollidingWithEnemy) && isJumping && !wasJumping) {
            velocity.y = owner.jumpPower;
            // ジャンプ直後は接地の猶予を打ち切り、離陸直後の再着地誤爆を防ぐ
            airborneTime = owner.groundedGraceTime + 1.0f;
            state = PlayerGroundState::Airborne;
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
        if (IsGrounded() && !isJumping && velocity.y <= 0.0f) {
            stick = groundHitNormal * (-owner.groundStickSpeed);
            stick.z = 0.0f;
        }
        Vector3 motion=(velocity+surfaceVelocity+slideVelocity+stick)*Min(dt,.1f);
        if(owner.useClassicForms || transformation.form!=PlayerForm::Sphere){tf.SetTranslate(tf.GetTranslate()+motion);return;}
        int steps=int(Ceil(motion.Length()/.10f));if(steps<1)steps=1;if(steps>256)steps=256;
        Vector3 increment=motion/float(steps);for(int step=0;step<steps;++step){Vector3 before=tf.GetTranslate();tf.SetTranslate(before+increment);if(!ResolveMovementOverlap()){tf.SetTranslate(before);if(Abs(increment.x)>.00001f){increment.x=0;velocity.x=0;slideVelocity.x=0;surfaceVelocity.x=0;}else{increment.y=0;velocity.y=0;}}}
    }

    // カメラ等の追従先を配置するための水平速度（自機速度・滑り・床速度を合算し、Y成分を除いたもの）
    Vector3 GetHorizontalVelocity() const {
        Vector3 horizontalVelocity = velocity + slideVelocity + surfaceVelocity;
        horizontalVelocity.y = 0.0f;
        return horizontalVelocity;
    }

    // Boxのローカル+Y軸を接地面の法線へ、少し遅れて追従させる。
    // 接地していない間は上向きへ戻すため、空中へ飛び出した後も傾きが残り続けない。
    void UpdateBoxGroundTilt(float dt) {
        if (!transformation.IsBox()) return;

        Vector3 targetNormal = hasGroundContact ? groundHitNormal : Vector3(0.0f, 1.0f, 0.0f);
        targetNormal.z = 0.0f;
        if (targetNormal.LengthSquared() <= 0.0001f) return;
        targetNormal = targetNormal.Normalize();

        // 行ベクトル規約でローカル+Yを法線へ向けるZ軸回転。
        float targetAngle = Atan2(-targetNormal.x, targetNormal.y);
        Transform@ tf = GetTransform();
        if (tf is null) return;

        float currentAngle = tf.GetRotate().z;
        float angleDifference = targetAngle - currentAngle;
        const float pi = GetPI();
        while (angleDifference > pi) angleDifference -= pi * 2.0f;
        while (angleDifference < -pi) angleDifference += pi * 2.0f;

        // dt依存の追従率にして、フレームレートにかかわらず同じ感触に近づける。
        float follow = Clamp(owner.boxGroundTiltFollowSpeed * dt, 0.0f, 1.0f);
        tf.SetRotate(Vector3(0.0f, 0.0f, currentAngle + angleDifference * follow));
    }

    // Coneの先端（プリミティブメッシュのローカル+Y軸）を、自機自身の移動方向へ向ける。
    // 動く床から受け取るsurfaceVelocityは意図的に含めないため、床に運ばれるだけでは回転しない。
    void UpdateConeFacing() {
        if (!transformation.CanEmbedOnImpact()) return;

        Vector3 selfVelocity = velocity + slideVelocity;
        selfVelocity.z = 0.0f;
        if (selfVelocity.LengthSquared() <= 0.0001f) return;

        // Z軸回転でローカル+YをselfVelocityの方向へ合わせる。
        // 行ベクトル規約では+Yを(x, y)へ向ける角度は atan2(-x, y) となる。
        float angle = Atan2(-selfVelocity.x, selfVelocity.y);
        Transform@ tf = GetTransform();
        if (tf !is null) {
            tf.SetRotate(Vector3(0.0f, 0.0f, angle));
        }
    }

    // 死亡・リスポーン時に移動関連の速度を全てリセットする
    void ResetVelocities() {
        rawGroundDelta=Vector3();hasGroundContact=false;isOnSteepSlopeContact=false;groundHitNormal=Vector3(0,1,0);airborneTime=1000;state=PlayerGroundState::Airborne;wasGrounded=false;isJumping=false;wasJumping=false;
        velocity = Vector3(0.0f, 0.0f, 0.0f);
        slideVelocity = Vector3(0.0f, 0.0f, 0.0f);
        surfaceVelocity = Vector3(0.0f, 0.0f, 0.0f);
    }

    // Cone形態の刺さり解除など、外部要因で速度を強制的に上書きして空中状態へ遷移させる
    void ApplyExternalVelocity(const Vector3 &in newVelocity) {
        ResetVelocities();
        velocity = newVelocity;
        state = PlayerGroundState::Airborne;
        airborneTime = owner.groundedGraceTime + 1.0f;
        hasGroundContact = false;
    }

    // 速度ベクトルvから、surfaceNormal方向へ侵入する成分だけを取り除いたものを返す
    // （壁・天井への衝突時、各velocity系フィールドをこれへ通すことで、そのフィールドが
    // 何であっても侵入方向の成分だけを一律に打ち消せる）
    Vector3 CancelIntoSurface(const Vector3 &in v, const Vector3 &in surfaceNormal) const {
        float intoSurface = v.Dot(surfaceNormal);
        return (intoSurface < 0.0f) ? (v - surfaceNormal * intoSurface) : v;
    }

    bool QueryPlayerBox(CollisionShape3D &out shape){
        BoxCollider@ box;Transform@ tf=GetTransform();if(tf is null||!GetComponent(@box)||!box.IsActive())return false;
        Vector3 size=box.GetSize(),scale=tf.GetScale();shape.type=CollisionShapeType3D::Box;shape.center=tf.GetWorldPosition()+box.GetCenter();shape.halfExtents=Vector3(Abs(size.x*scale.x),Abs(size.y*scale.y),Abs(size.z*scale.z))*.5f;return true;
    }
    void ProbeGround(){
        if(transformation.form!=PlayerForm::Sphere||hasGroundContact||velocity.y>0||isJumping)return;CollisionShape3D shape;if(!QueryPlayerBox(shape))return;shape.center.y-=.035f;shape.halfExtents.x*=.97f;
        array<CollisionHit@>@ hits=QueryCollision3D(shape,false,GetOwnerObject());
        for(uint i=0;i<hits.length();++i){CollisionHit@ h=hits[i];if(h.otherCollider is null||h.otherCollider.GetTag()!=groundColliderTag||h.normal.y<=owner.groundedThreshold)continue;
            hasGroundContact=true;groundHitNormal=h.normal;isOnSteepSlopeContact=h.normal.y<owner.slideThreshold;rawGroundDelta=Vector3();Transform@ surface;PreTransform@ previous;
            if(h.otherObject !is null&&h.otherObject.GetComponent(@surface)&&h.otherObject.GetComponent(@previous))rawGroundDelta=surface.GetWorldPosition()-previous.GetPreviousWorldPosition();break;}
    }
    bool TryQueryStep(CollisionShape3D shape){
        if(!(hasGroundContact||IsGrounded())||velocity.y>0||isJumping)return false;Vector3 center=shape.center;shape.center.y+=.078f;if(GroundOverlap(shape))return false;
        float low=0,high=.078f;for(uint i=0;i<5;++i){float middle=(low+high)*.5f;shape.center.y=center.y+middle;if(GroundOverlap(shape))low=middle;else high=middle;}
        GetTransform().SetTranslate(GetTransform().GetTranslate()+Vector3(0,high+.002f,0));return true;
    }
    bool ResolveMovementOverlap(){
        for(int pass=0;pass<5;++pass){CollisionShape3D shape;if(!QueryPlayerBox(shape))return true;array<CollisionHit@>@ hits=QueryCollision3D(shape,false,GetOwnerObject());int best=-1;float deepest=.0002f;
            for(uint i=0;i<hits.length();++i)if(hits[i].otherCollider !is null&&hits[i].otherCollider.GetTag()==groundColliderTag&&hits[i].penetration>deepest){best=int(i);deepest=hits[i].penetration;}
            if(best<0)return true;CollisionHit@ hit=hits[uint(best)];Vector3 n=hit.normal;n.z=0;if(n.LengthSquared()<.01f)return false;
            if(Abs(n.x)>.5f&&Abs(n.y)<.2f&&TryQueryStep(shape))continue;
            Vector3 correction=n*(hit.penetration+.0005f);if(n.y>owner.groundedThreshold){correction=Vector3(0,(hit.penetration+.0005f)/n.y,0);hasGroundContact=true;groundHitNormal=n;}
            GetTransform().SetTranslate(GetTransform().GetTranslate()+correction);velocity=CancelIntoSurface(velocity,n);slideVelocity=CancelIntoSurface(slideVelocity,n);
        }
        CollisionShape3D remaining;if(!QueryPlayerBox(remaining))return true;return !GroundOverlap(remaining);
    }
    bool GroundOverlap(const CollisionShape3D &in shape) {
        array<CollisionHit@>@ hits=QueryCollision3D(shape,false,GetOwnerObject());
        for(uint i=0;i<hits.length();++i)
            if(hits[i].otherCollider !is null&&hits[i].otherCollider.GetTag()==groundColliderTag&&hits[i].penetration>.0001f)return true;
        return false;
    }

    // A box pressed into a floor can hit the exposed side of the next floor
    // before its top contact is delivered. Resolve only a shallow, clear step;
    // a full-height wall or an obstructed ceiling must still block movement.
    bool ResolveFloorSeam(const HitInfo &in hit) {
        if(transformation.form!=PlayerForm::Sphere||Abs(hit.normal.x)<.5f||Abs(hit.normal.y)>.2f)return false;
        if(!(hasGroundContact||IsGrounded())||velocity.y>0||isJumping)return false;
        BoxCollider@ box;Transform@ tf=GetTransform();
        if(tf is null||!GetComponent(@box)||!box.IsActive())return false;
        Vector3 rotation=tf.GetWorldRotate();
        if(rotation.LengthSquared()>.0001f)return false;
        Vector3 size=box.GetSize(),scale=tf.GetScale();
        CollisionShape3D shape;shape.type=CollisionShapeType3D::Box;
        shape.center=tf.GetWorldPosition()+box.GetCenter();
        shape.halfExtents=Vector3(Abs(size.x*scale.x),Abs(size.y*scale.y),Abs(size.z*scale.z))*.5f;
        const float limit=.078f,skin=.002f; // Total correction is at most 8cm.
        Vector3 center=shape.center;shape.center.y=center.y+limit;
        if(GroundOverlap(shape))return false;
        float low=0,high=limit;
        for(uint i=0;i<5;++i){float middle=(low+high)*.5f;shape.center.y=center.y+middle;if(GroundOverlap(shape))low=middle;else high=middle;}
        shape.center.y=center.y+high+skin;
        if(GroundOverlap(shape))return false;
        tf.SetTranslate(tf.GetTranslate()+Vector3(0,high+skin,0));
        return true;
    }

    void HandleCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null || hit.otherCollider is null) return;
        // トリガーのコライダーは接触の通知だけを受け取り、接地判定も押し戻しも行わない（すり抜ける）
        if (hit.otherCollider !is null && hit.otherCollider.IsTrigger()) return;
        if (hit.otherCollider.GetTag() != groundColliderTag) return;

        // The callback batch can describe a position before an earlier contact
        // correction or a teleport. Humanoids resolve from current transforms,
        // never add those stale penetration vectors together.
        if(!owner.useClassicForms && transformation.form==PlayerForm::Sphere){ResolveMovementOverlap();return;}

        // 法線が上向きなら地面に接触しているとみなす
        bool seam=ResolveFloorSeam(hit);
        Vector3 contactNormal=seam?Vector3(0,1,0):hit.normal;
        bool grounded = contactNormal.y > owner.groundedThreshold;
        if (!hasGroundContact && grounded) {
            hasGroundContact = true;
            groundHitNormal = contactNormal;
            // 接地面が閾値より急な斜面なら滑り状態にする
            isOnSteepSlopeContact = contactNormal.y < owner.slideThreshold;
        } else if (!grounded) {
            // 壁・天井との接触。侵入方向（法線と逆向き）へ進もうとしている速度成分だけを
            // 打ち消して、壁に張り付いたまま速度が蓄積し続けたり、天井に頭をぶつけても
            // 上昇速度がそのまま残ってしまったりしないようにする
            // （法線に沿った成分だけを除去するので、壁に沿った方向の成分はそのまま残る）。
            // velocity/slideVelocityのどちらも壁へ侵入し得るため、両方をこの共通処理へ通す
            // （新しい速度ベクトルが増えた場合も、ここへ1行追加するだけで打ち消しが効くようにする）
            Vector3 wallNormal = hit.normal;
            wallNormal.z = 0.0f;
            velocity = CancelIntoSurface(velocity, wallNormal);
            slideVelocity = CancelIntoSurface(slideVelocity, wallNormal);
            // 床から引き継いだ慣性は、別の壁面へ衝突した時点で終了する
            surfaceVelocity = Vector3(0.0f, 0.0f, 0.0f);
        }

        // 動く床への追従。地面のTransformとPreTransform（前フレームの値）との差分から
        // 実際の移動量を求めておき、UpdateGroundState()側でsurfaceVelocityへ変換する
        // （velocity本体に加算すると接地中に蓄積し続けてしまうため）。Velocityコンポーネントを
        // 見る方式と違い、地面がどんな方法で動いていても（スクリプトで直接Transformを
        // 書き換えていても）追従できる
        if (grounded) {
            Transform@ groundTransform;
            PreTransform@ groundPreTransform;
            if (hit.otherObject.GetComponent(@groundTransform) && hit.otherObject.GetComponent(@groundPreTransform)) {
                rawGroundDelta = groundTransform.GetTranslate() - groundPreTransform.GetPreviousTranslate();
            } else {
                rawGroundDelta = Vector3(0.0f, 0.0f, 0.0f);
            }
        }

        // 衝突判定から押し戻しベクトルを計算してプレイヤーを押し戻す
        Transform@ tf = GetTransform();
        if (tf !is null) {
            Vector3 pushBack = seam?Vector3():hit.normal * hit.penetration;
            pushBack.z = 0.0f;
            // 着地判定がある場合はY方向だけ押し戻す
            if (grounded) {
                pushBack.x = 0.0f;pushBack.y=hit.penetration/Max(hit.normal.y,.001f);
            }
            tf.SetTranslate(tf.GetTranslate() + pushBack);
        }
    }
}
