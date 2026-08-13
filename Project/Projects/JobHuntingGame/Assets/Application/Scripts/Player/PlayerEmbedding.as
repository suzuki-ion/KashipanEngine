// Cone形態時、何かに接触した瞬間その場へ刺さって静止する「刺さり」機構を担当する。
// 刺さっている間はPlayerMovementの通常の移動処理を完全に止め、代わりにこのクラスが
// Transformを直接操作する（動く床/壁/敵に刺さった場合は、その相手のPreTransformとの
// 差分をそのまま自機へ加算して追従させる。PlayerMovementのsurfaceVelocityと同じ手法）
class PlayerEmbedding {
    Player@ owner;
    PlayerTransformation@ transformation;

    bool isEmbedded = false;
    // 刺さった面の法線（ジャンプで解除する際、この方向へ飛び出す）
    Vector3 embedNormal = Vector3(0.0f, 0.0f, 0.0f);
    // 刺さっている相手オブジェクト（動く床/壁/敵への追従用）
    Object@ embedSurfaceObject;

    Tag groundColliderTag = Tag("GroundBox");
    Tag enemyColliderTag = Tag("EnemySphere");

    PlayerEmbedding(Player@ inOwner, PlayerTransformation@ inTransformation) {
        @owner = inOwner;
        @transformation = inTransformation;
    }

    bool IsEmbedded() const {
        return isEmbedded;
    }

    // OnCollisionEnter/OnCollisionStayの両方から呼ばれる（Stayからも呼ぶのは、既に地面に
    // 接触した状態でCone形態へ変身した場合、Enterが再発火せず刺されなくなるのを防ぐため）
    void HandleCollisionContact(const HitInfo &in hit) {
        if (isEmbedded) return;
        if (!transformation.CanEmbedOnImpact()) return;
        if (hit.otherObject is null || hit.otherCollider is null) return;
        if (hit.otherCollider.IsTrigger()) return;

        Tag otherTag = hit.otherCollider.GetTag();
        bool isEnemy = (otherTag == enemyColliderTag);
        if (otherTag != groundColliderTag && !isEnemy) return;

        Embed(hit.normal, hit.otherObject, isEnemy);
    }

    void Embed(const Vector3 &in normal, Object@ surfaceObject, bool isEnemy) {
        isEmbedded = true;
        embedNormal = normal;
        @embedSurfaceObject = surfaceObject;

        if (isEnemy) {
            RequestEnemyDefeat(surfaceObject);
        }
    }

    // 刺さっている間、相手オブジェクトの移動量へ追従させる
    void FollowSurface() {
        if (!isEmbedded || embedSurfaceObject is null) return;
        if (!IsValidObject(embedSurfaceObject)) {
            // 刺さっていた相手（撃破された敵など）が消滅していたら、その場で解除しておく
            Release();
            return;
        }

        Transform@ surfaceTransform;
        PreTransform@ surfacePreTransform;
        if (!embedSurfaceObject.GetComponent(@surfaceTransform) || !embedSurfaceObject.GetComponent(@surfacePreTransform)) return;

        Vector3 delta = surfaceTransform.GetTranslate() - surfacePreTransform.GetPreviousTranslate();
        if (delta.LengthSquared() <= 0.0f) return;

        Transform@ tf = GetTransform();
        if (tf !is null) {
            tf.SetTranslate(tf.GetTranslate() + delta);
        }
    }

    // ジャンプ入力があれば刺さりを解除し、刺さっていた面の法線方向へ飛び出す速度を返す
    bool TryRelease(bool jumpTriggered, Vector3 &out popVelocity) {
        if (!isEmbedded || !jumpTriggered) return false;
        popVelocity = embedNormal * owner.jumpPower;
        Release();
        return true;
    }

    void Release() {
        isEmbedded = false;
        @embedSurfaceObject = null;
    }
}
