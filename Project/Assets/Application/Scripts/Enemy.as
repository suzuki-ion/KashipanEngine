class Enemy : ScriptComponentBehavior {
    [SerializeField, Tooltip("プレイヤーとの衝突判定閾値")]
    float playerCollisionThreshold = 0.4f;
    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 2.0f;
    [SerializeField, Tooltip("重力")]
    float gravity = 0.2f;
    [SerializeField, Tooltip("最大落下速度")]
    float maxFallSpeed = 16.0f;
    [SerializeField, Tooltip("地面と判定する法線Yの閾値（絶対値がこの値未満の法線は壁とみなす）")]
    float groundedThreshold = 0.4f;
    [SerializeField, Tooltip("接地中に地面へ押し付ける速度。継ぎ目のわずかな段差や下り坂で浮かないようにする")]
    float groundStickSpeed = 2.0f;
    [SerializeField, Tooltip("開始時の移動方向（正: +X方向、負: -X方向）")]
    float moveDirection = 1.0f;

    // --- 実行時状態（保存不要） ---
    bool isAlive = true;
    bool isCollidingWithPlayer = false;
    // プレイヤーとの衝突法線専用（地面との衝突法線と混ざらないよう分離する。
    // 混ぜると、常に接触している地面の法線[上向き]が後から上書きしてしまい、
    // 踏まれた判定[下向きの法線]が消えてしまうバグの原因になる）
    Vector3 playerHitNormal = Vector3(0.0f, 0.0f, 0.0f);

    // このフレームで実際に地面（床）と接触したか（生の値。次のOnCollisionコールバックまでの間だけ有効）。
    // 継ぎ目でつながった地面同士は境界付近で両方のコライダーに同時に触れるため、この値は途切れない
    bool hasGroundContact = false;
    // 前フレームのhasGroundContact（接地→非接地に変わった瞬間＝地面の端に到達した瞬間を検出するため）
    bool wasGroundContact = false;
    Vector3 groundHitNormal = Vector3(0.0f, 1.0f, 0.0f);
    // 接地中の地面の、前フレームからの実際の移動量（動く床への追従用。Velocityではなく
    // PreTransformとの差分で求めるため、地面がどのような方法で動いていても追従できる）
    Vector3 groundDelta = Vector3(0.0f, 0.0f, 0.0f);
    float velocityY = 0.0f;

    Tag groundColliderTag = Tag("GroundBox");
    Tag playerColliderTag = Tag("PlayerSphere");

    void Start() {
        Log("Enemy start: " + GetOwnerObject().GetName());
    }

    void Update() {
        if (!isAlive) return;
        const float dt = GetDeltaTime() * GetGameSpeed();

        // ConsumePlayerCollision() 相当（このフレームだけ有効なパルスとして消費する）
        bool playerContact = isCollidingWithPlayer;
        isCollidingWithPlayer = false;

        // プレイヤーと衝突しているかつ衝突の法線が上向きなら死亡状態にする
        if (playerContact && playerHitNormal.y < playerCollisionThreshold) {
            isAlive = false;
            Log(GetOwnerObject().GetName() + " defeated!");
            GetOwnerObject().SetActive(false);
            return;
        }

        UpdateGroundEdgeTurn();
        Move(dt);

        // 生の接触情報は毎フレームリセットする（次フレームの衝突コールバックで再設定される）
        hasGroundContact = false;
    }

    // 地面との接触が「途切れた瞬間」（＝地面の端に到達した瞬間）にだけ移動方向を反転する。
    // 継ぎ目でつながった地面同士は接触が途切れないため、このタイミングでは反転しない
    // （マリオの赤ノコノコのように、繋がった地面の上ではそのまま歩き続ける）。
    void UpdateGroundEdgeTurn() {
        if (wasGroundContact && !hasGroundContact) {
            moveDirection = -moveDirection;
        }
        wasGroundContact = hasGroundContact;
    }

    void Move(const float dt) {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        if (hasGroundContact) {
            velocityY = 0.0f;
        } else {
            velocityY -= gravity * dt * 60.0f;
            if (velocityY < -maxFallSpeed) velocityY = -maxFallSpeed;
        }

        Vector3 translate = tf.GetTranslate();
        translate.x += moveSpeed * Sign(moveDirection) * dt;
        translate.y += velocityY * dt;
        if (hasGroundContact) {
            // 接地面へ押し付けて、継ぎ目の段差や下り坂で浮かないようにする
            translate.x += -groundHitNormal.x * groundStickSpeed * dt;
            translate.y += -groundHitNormal.y * groundStickSpeed * dt;
            // 動く床への追従（既にdt分の実移動量なので、dtを掛けずにそのまま加算する）
            translate += groundDelta;
        }
        tf.SetTranslate(translate);
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            HandleGroundContact(hit);
        } else if (hit.otherCollider.GetTag() == playerColliderTag) {
            isCollidingWithPlayer = true;
            playerHitNormal = hit.normal;
        }
    }

    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            HandleGroundContact(hit);

            // 衝突判定から押し戻しベクトルを計算してエネミーを押し戻す
            Transform@ tf = GetTransform();
            if (tf !is null) {
                Vector3 pushBack = hit.normal * hit.penetration;
                pushBack.z = 0.0f;
                // 床の押し戻しはY方向だけにして、横移動を妨げないようにする
                if (hit.normal.y > groundedThreshold) {
                    pushBack.x = 0.0f;
                }
                tf.SetTranslate(tf.GetTranslate() + pushBack);
            }
        } else if (hit.otherCollider.GetTag() == playerColliderTag) {
            // プレイヤーにダメージを与えるなどの処理をここに追加
        }
    }

    void OnCollisionExit(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == playerColliderTag) {
            isCollidingWithPlayer = false;
            // プレイヤーとの接触が終了したときの処理をここに追加
        }
    }

    // 地面コライダーとの接触を処理する（接地フラグの更新と、壁に当たった場合の反転）
    void HandleGroundContact(const HitInfo &in hit) {
        const bool isFloor = hit.normal.y > groundedThreshold;
        if (isFloor) {
            hasGroundContact = true;
            groundHitNormal = hit.normal;

            // 動く床への追従。地面のTransformとPreTransform（前フレームの値）との差分から
            // 実際の移動量を求める（Velocityコンポーネントを見る方式と違い、地面がどんな方法で
            // 動いていても＝スクリプトで直接Transformを書き換えていても追従できる）
            Transform@ groundTransform;
            PreTransform@ groundPreTransform;
            if (hit.otherObject.GetComponent(@groundTransform) && hit.otherObject.GetComponent(@groundPreTransform)) {
                groundDelta = groundTransform.GetTranslate() - groundPreTransform.GetPreviousTranslate();
            } else {
                groundDelta = Vector3(0.0f, 0.0f, 0.0f);
            }
        } else if (Abs(hit.normal.y) < groundedThreshold) {
            // ほぼ真横の法線＝壁。進行方向を塞ぐ向きの壁に当たったら反転する
            if (Sign(hit.normal.x) == -Sign(moveDirection)) {
                moveDirection = -moveDirection;
            }
        }
    }

    void End() {
        Log("Enemy end");
    }
}
