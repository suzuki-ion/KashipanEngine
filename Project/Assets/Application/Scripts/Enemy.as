class Enemy : ScriptComponentBehavior {
    // プレイヤーとの衝突とみなす法線の閾値（この値未満＝上から踏まれた場合に死亡）
    [SerializeField, Tooltip("プレイヤーとの衝突判定閾値")]
    float playerCollisionThreshold = 0.4f;

    bool isGrounded = false;
    bool isCollidingWithPlayer = false;
    Vector3 hitNormal = Vector3(0.0f, 0.0f, 0.0f);
    bool isAlive = true;

    Tag groundColliderTag = Tag("GroundBox");
    Tag playerColliderTag = Tag("PlayerSphere");

    void Start() {
        Log("Enemy start: " + GetOwnerObject().GetName());
    }

    void Update() {
        if (!isAlive) return;

        // ConsumePlayerCollision() 相当（このフレームだけ有効なパルスとして消費する）
        bool playerContact = isCollidingWithPlayer;
        isCollidingWithPlayer = false;

        // プレイヤーと衝突しているかつ衝突の法線が上向きなら死亡状態にする
        if (playerContact && hitNormal.y < playerCollisionThreshold) {
            isAlive = false;
            Log(GetOwnerObject().GetName() + " defeated!");
            GetOwnerObject().SetActive(false);
        }
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            // 法線が上向きなら地面に接触しているとみなす
            isGrounded = hit.normal.y > 0.5f;
        } else if (hit.otherCollider.GetTag() == playerColliderTag) {
            isCollidingWithPlayer = true;
            // プレイヤーにダメージを与えるなどの処理をここに追加
        }
        hitNormal = hit.normal;
    }

    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            // 衝突判定から押し戻しベクトルを計算してエネミーを押し戻す
            Transform@ tf = GetTransform();
            if (tf !is null) {
                Vector3 pushBack = hit.normal * hit.penetration;
                pushBack.z = 0.0f;
                tf.SetTranslate(tf.GetTranslate() + pushBack);
            }
        } else if (hit.otherCollider.GetTag() == playerColliderTag) {
            // プレイヤーにダメージを与えるなどの処理をここに追加
        }
        hitNormal = hit.normal;
    }

    void OnCollisionExit(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            isGrounded = false;
        } else if (hit.otherCollider.GetTag() == playerColliderTag) {
            isCollidingWithPlayer = false;
            // プレイヤーとの接触が終了したときの処理をここに追加
        }
        hitNormal = hit.normal;
    }

    void End() {
        Log("Enemy end");
    }
}
