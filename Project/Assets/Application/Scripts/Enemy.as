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
    [SerializeField, Tooltip("エッジセンサーが連続して地面を検知できなかった場合に反転するまでの猶予時間（秒）。継ぎ目をまたぐ一瞬だけセンサーが両方の床の隙間に入ってしまう誤検知を無視するためのもの")]
    float edgeSensorMissTolerance = 0.05f;

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

    // 進行方向側・逆方向側それぞれのエッジセンサー（足元の少し先・少し下を判定する専用コライダー）が
    // このフレームで地面に触れているか（生の値）。本体コライダーとは別に判定することで、
    // 実際に足を踏み外す前に「この先に地面が無い」ことを検知できる
    bool hasEdgeSensorRightContact = false;
    bool hasEdgeSensorLeftContact = false;
    // 進行方向側のエッジセンサーが地面を検知できていない状態が続いている時間（秒）
    float edgeSensorMissTimer = 0.0f;

    Tag groundColliderTag = Tag("GroundBox");
    Tag playerColliderTag = Tag("PlayerSphere");
    // 自分自身のコライダーの判別用（HitInfo.selfColliderのタグと比較する）
    Tag bodyColliderTag = Tag("EnemySphere");
    Tag edgeSensorRightTag = Tag("EnemyEdgeSensorRight");
    Tag edgeSensorLeftTag = Tag("EnemyEdgeSensorLeft");

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

        UpdateGroundEdgeTurn(dt);
        Move(dt);

        // 生の接触情報は毎フレームリセットする（次フレームの衝突コールバックで再設定される）
        hasGroundContact = false;
        hasEdgeSensorRightContact = false;
        hasEdgeSensorLeftContact = false;
    }

    // 進行方向の反転判定。
    // 1) エッジセンサー方式（優先）: 現在の進行方向側のエッジセンサーが地面に触れていない状態が
    //    edgeSensorMissTolerance秒以上続いたら、実際に足を踏み外す前にその場で反転する。
    //    単発フレームでの検知漏れを猶予するのは、継ぎ目をまたぐ一瞬だけエッジセンサー（本体より
    //    小さい判定範囲）がちょうど2枚の床の境界の隙間に重なってしまい、一時的に地面を検知できない
    //    ことがあるため（本体コライダーはサイズが大きく境界をまたいでも接触が途切れないが、
    //    エッジセンサーは小さいぶんこの影響を受けやすい）。継ぎ目ではすぐに反対側の床を検知できて
    //    タイマーがリセットされるので反転までは至らない
    // 2) 保険（フォールバック）: それでも検知できなかった場合に備え、本体コライダーの地面との接触が
    //    「途切れた瞬間」にも従来どおり反転する（継ぎ目でつながった地面同士は接触が途切れないため、
    //    このタイミングでは反転しない）
    // 同一フレームで両方の条件を満たしても反転が二重に掛からないようにする
    void UpdateGroundEdgeTurn(const float dt) {
        bool turned = false;
        if (hasGroundContact) {
            bool leadingSensorGrounded = (moveDirection >= 0.0f) ? hasEdgeSensorRightContact : hasEdgeSensorLeftContact;
            if (leadingSensorGrounded) {
                edgeSensorMissTimer = 0.0f;
            } else {
                edgeSensorMissTimer += dt;
                if (edgeSensorMissTimer >= edgeSensorMissTolerance) {
                    moveDirection = -moveDirection;
                    edgeSensorMissTimer = 0.0f;
                    turned = true;
                }
            }
        } else {
            edgeSensorMissTimer = 0.0f;
        }
        if (!turned && wasGroundContact && !hasGroundContact) {
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
            HandleGroundColliderHit(hit);
        } else if (hit.otherCollider.GetTag() == playerColliderTag) {
            isCollidingWithPlayer = true;
            playerHitNormal = hit.normal;
        }
    }

    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == groundColliderTag) {
            HandleGroundColliderHit(hit);

            // 押し戻しは本体コライダーの接触のみ行う（エッジセンサーは判定専用で押し戻さない）
            if (hit.selfCollider !is null && hit.selfCollider.GetTag() == bodyColliderTag) {
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

    // 地面コライダーとの接触を、接触したのが自分のどのコライダー（本体/右エッジセンサー/
    // 左エッジセンサー）かで振り分ける
    void HandleGroundColliderHit(const HitInfo &in hit) {
        if (hit.selfCollider is null) return;
        Tag selfTag = hit.selfCollider.GetTag();
        if (selfTag == bodyColliderTag) {
            HandleGroundContact(hit);
        } else if (selfTag == edgeSensorRightTag) {
            if (hit.normal.y > groundedThreshold) hasEdgeSensorRightContact = true;
        } else if (selfTag == edgeSensorLeftTag) {
            if (hit.normal.y > groundedThreshold) hasEdgeSensorLeftContact = true;
        }
    }

    // 本体コライダーと地面コライダーとの接触を処理する（接地フラグの更新と、壁に当たった場合の反転）
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
