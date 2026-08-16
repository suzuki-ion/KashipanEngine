// Cone形態時、何かに接触した瞬間その場へ刺さって静止する「刺さり」機構を担当する。
// 刺さっている間はPlayerMovementの通常の移動処理を完全に止め、代わりにこのクラスが
// Transformを直接操作する（動く床/壁に刺さった場合は、このクラスで保持した相手の
// 前回位置・回転との差分を自機へ反映して追従させる）
class PlayerEmbedding {
    Player@ owner;
    PlayerTransformation@ transformation;

    bool isEmbedded = false;
    // 刺さり解除直後に、まだ重なっている同じ面へ即座に再び刺さらないための猶予時間
    float reembedGraceTimer = 0.0f;
    float reembedGraceTime = 0.1f;
    float releaseSeparationMargin = 0.01f;
    // 解除直後の接触通知を通常移動側でも無視するため、解除元の面を猶予時間中だけ保持する
    Object@ releasedSurfaceObject;
    // 刺さった面の法線（ジャンプで解除する際、この方向へ飛び出す）
    Vector3 embedNormal = Vector3(0.0f, 0.0f, 0.0f);
    // 刺さった時点でのめり込み量。解除時に面の外へ確実に押し出すために保持する
    float embedPenetration = 0.0f;
    // 刺さっている相手オブジェクト（動く床/壁への追従用）
    Object@ embedSurfaceObject;
    Vector3 previousSurfacePosition = Vector3(0.0f, 0.0f, 0.0f);
    Quaternion previousSurfaceRotation = Math::IdentityQuaternion();
    bool hasPreviousSurfaceTransform = false;

    Tag groundColliderTag = Tag("GroundBox");

    PlayerEmbedding(Player@ inOwner, PlayerTransformation@ inTransformation) {
        @owner = inOwner;
        @transformation = inTransformation;
    }

    bool IsEmbedded() const {
        return isEmbedded;
    }

    // 刺さり解除直後は、Coneの傾きによって解除元の床から斜めの接触法線が返ることがある。
    // その接触を通常移動へ渡すと、設定したジャンプ速度が打ち消されて高度が角度依存になる。
    bool ShouldIgnoreReleasedSurface(const HitInfo &in hit) const {
        return reembedGraceTimer > 0.0f &&
               releasedSurfaceObject !is null &&
               hit.otherObject is releasedSurfaceObject;
    }

    // OnCollisionEnter/OnCollisionStayの両方から呼ばれる（Stayからも呼ぶのは、既に地面に
    // 接触した状態でCone形態へ変身した場合、Enterが再発火せず刺されなくなるのを防ぐため）
    void HandleCollisionContact(const HitInfo &in hit) {
        if (isEmbedded) return;
        if (reembedGraceTimer > 0.0f) return;
        if (!transformation.CanEmbedOnImpact()) return;
        if (hit.otherObject is null || hit.otherCollider is null) return;
        if (hit.otherCollider.IsTrigger()) return;

        // 敵は刺さり対象にしない。通常の敵接触処理へ渡すことで、Sphere/Boxと同じく
        // 上から接触した際に敵を倒し、jumpPowerで跳ね返る挙動にする。
        if (hit.otherCollider.GetTag() != groundColliderTag) return;

        Embed(hit.normal, hit.penetration, hit.otherObject);
    }

    void Embed(const Vector3 &in normal, float penetration, Object@ surfaceObject) {
        isEmbedded = true;
        embedNormal = normal;
        embedPenetration = penetration;
        @embedSurfaceObject = surfaceObject;
        Transform@ surfaceTransform;
        hasPreviousSurfaceTransform = surfaceObject !is null && surfaceObject.GetComponent(@surfaceTransform);
        if (hasPreviousSurfaceTransform) {
            previousSurfacePosition = surfaceTransform.GetWorldPosition();
            previousSurfaceRotation = surfaceTransform.GetWorldRotateQuaternion();
        }

    }

    // 刺さっている間、相手オブジェクトの移動・回転へ追従させる
    void FollowSurface() {
        if (!isEmbedded || embedSurfaceObject is null) return;
        if (!IsValidObject(embedSurfaceObject) || !embedSurfaceObject.IsActive()) {
            // 刺さっていた相手が削除または非アクティブ化されたら、その場で解除する。
            // 敵の撃破処理はSetActive(false)を使うため、IsValidObjectだけでは検出できない。
            Release();
            return;
        }

        Transform@ surfaceTransform;
        if (!embedSurfaceObject.GetComponent(@surfaceTransform)) {
            // 追従先の位置を取得できない状態で刺さり続けると永久に固定されるため解除する
            Release();
            return;
        }

        Vector3 currentSurfacePosition = surfaceTransform.GetWorldPosition();
        Quaternion currentSurfaceRotation = surfaceTransform.GetWorldRotateQuaternion();
        if (!hasPreviousSurfaceTransform) {
            previousSurfacePosition = currentSurfacePosition;
            previousSurfaceRotation = currentSurfaceRotation;
            hasPreviousSurfaceTransform = true;
            return;
        }

        // 前回から現在までの床の回転差分。床の回転中心から見たPlayerの相対位置にも
        // 適用することで、床の角度だけでなく回転に伴う周回移動にも追従させる。
        Quaternion rotationDelta = (currentSurfaceRotation * previousSurfaceRotation.Inverse()).Normalize();

        Transform@ tf = GetTransform();
        if (tf !is null) {
            Vector3 playerWorldPosition = tf.GetWorldPosition();
            Vector3 previousRelativePosition = playerWorldPosition - previousSurfacePosition;
            Vector3 followedWorldPosition = currentSurfacePosition + rotationDelta.RotateVector(previousRelativePosition);

            // Playerの親Transformはゲーム既定オブジェクト（単位Transform）なので、
            // ワールド移動量を現在のローカル座標へ加算して位置を更新できる。
            tf.SetTranslate(tf.GetTranslate() + (followedWorldPosition - playerWorldPosition));
            tf.SetRotateQuaternion((rotationDelta * tf.GetRotateQuaternion()).Normalize());

            // 解除ジャンプも回転後の面から離れる方向になるよう、刺さった面の法線を追従させる。
            embedNormal = rotationDelta.RotateVector(embedNormal).Normalize();
        }

        previousSurfacePosition = currentSurfacePosition;
        previousSurfaceRotation = currentSurfaceRotation;
    }

    // ジャンプ入力があれば刺さりを解除し、刺さっていた面の法線方向へ飛び出す速度を返す
    bool TryRelease(bool jumpTriggered, Vector3 &out popVelocity) {
        if (!isEmbedded || !jumpTriggered) return false;
        popVelocity = embedNormal * owner.jumpPower;
        // Coneの側面寄りで床へ刺さると、水平な床でも接触法線が斜めになり、
        // groundedThresholdを下回る場合がある。上向き成分がある接触は床・上向き斜面として
        // 扱い、Cone自身の刺さり角度にかかわらず解除時の上向き速度を一定にする。
        if (embedNormal.y > 0.001f) {
            popVelocity.y = owner.jumpPower;
        } else if (Abs(embedNormal.y) <= 0.001f) {
            // 壁からの解除では、法線方向に飛び出す力へ上向きの力を加える。
            popVelocity.y += owner.coneWallReleaseUpwardPower;
        }
        // 刺さり時点のめり込み量を含めて面の外側へ押し出す。速度だけを設定すると
        // 次フレームの衝突通知で同じ面に再び刺さってしまい、解除できないように見える
        Transform@ tf = GetTransform();
        if (tf !is null) {
            tf.SetTranslate(tf.GetTranslate() + embedNormal * (embedPenetration + releaseSeparationMargin));
        }
        @releasedSurfaceObject = embedSurfaceObject;
        Release();
        reembedGraceTimer = reembedGraceTime;
        return true;
    }

    void Update(float dt) {
        if (reembedGraceTimer > 0.0f) {
            reembedGraceTimer -= dt;
            if (reembedGraceTimer <= 0.0f) {
                @releasedSurfaceObject = null;
            }
        }
    }

    void Release() {
        isEmbedded = false;
        embedPenetration = 0.0f;
        hasPreviousSurfaceTransform = false;
        @embedSurfaceObject = null;
    }
}
