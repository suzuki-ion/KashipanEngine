enum BossState {
    Idle,
    SmallJump,
    BigJump,
    Dead
}

class Boss1 : ScriptComponentBehavior {
    [SerializeField, Tooltip("移動速度(X方向のジャンプ幅)")]
    float moveSpeed = 80.0f;

    [SerializeField, Tooltip("小ジャンプのジャンプ力")]
    float smallJumpPower = 100.0f;

    [SerializeField, Tooltip("大ジャンプのジャンプ力")]
    float bigJumpPower = 200.0f;

    [SerializeField, Tooltip("重力")]
    float gravity = 150.0f;

    [SerializeField, Tooltip("待機時間(秒)")]
    float idleDuration = 2.0f;

    [SerializeField, Tooltip("HP")]
    float hp = 20.0f;

    [SerializeField, Tooltip("ダメージを受けた際の色変化時間(秒)")]
    float damageFlashDuration = 0.1f;

    [SerializeField, Tooltip("ダメージ時の色")]
    Vector4 damageColor = Vector4(10.0f, 10.0f, 10.0f, 1.0f);

    [SerializeField, Tooltip("通常時の色")]
    Vector4 normalColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    [SerializeField, Tooltip("岩(小)のプレハブ")]
    Object@ rubbleSmall;

    [SerializeField, Tooltip("岩(中)のプレハブ")]
    Object@ rubbleMedium;

    [SerializeField, Tooltip("岩(小)の生成数")]
    int rubbleSmallCount = 2;

    [SerializeField, Tooltip("岩(中)の生成数")]
    int rubbleMediumCount = 2;

    [SerializeField, Tooltip("接地とみなす法線Y成分のしきい値")]
    float groundedThreshold = 0.5f;

    [SerializeField, Tooltip("押し戻しを行わない相手のタグ一覧")]
    array<string>@ pushBackExcludeTags;

    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    BossState state = BossState::Idle;
    BossState lastState = BossState::Idle;
    
    // 行動管理用
    float stateTimer = 0.0f;
    bool nextIsBigJump = false;
    Vector2 velocity;
    
    // ダメージ演出管理用
    float damageFlashTimer = 0.0f;
    bool isFlashing = false;

    // 各種コンポーネント
    SpriteRenderer@ sprite;
    Box2DCollider@ col;
    CharacterController2D@ controller;
    
    // 岩の生成管理用
    array<Object@> rubbles;

    void Start() {
        GetComponent(@sprite);
        GetComponent(@col);
        
        if (GetComponent(@controller)) {
            if (col !is null) {
                controller.SetSelectedCollider(col);
            }
            controller.SetGroundedThreshold(groundedThreshold);
            controller.ClearIgnoredTags();
            if (pushBackExcludeTags !is null) {
                for (uint i = 0; i < pushBackExcludeTags.length(); ++i) {
                    controller.AddIgnoredTag(pushBackExcludeTags[i]);
                }
            }
        }
        
        if (sprite !is null) {
            sprite.SetInstanceColor(normalColor);
        }

        SetAnimation(BossState::Idle);
    }

    void Update() {
        // ダメージ時の色点滅処理
        if (isFlashing) {
            damageFlashTimer -= GetDeltaTime();
            if (damageFlashTimer <= 0.0f) {
                isFlashing = false;
                if (sprite !is null) {
                    sprite.SetInstanceColor(normalColor);
                }
            }
        }

        if (state == BossState::Dead) return;

        // HPチェック
        if (hp <= 0.0f) {
            ChangeState(BossState::Dead);
        } else {
            Transform@ tf = GetTransform();
            if (tf !is null) {
                // 重力適用
                velocity.y -= gravity * GetDeltaTime();

                // 移動・当たり判定処理
                if (controller !is null) {
                    controller.Move(velocity * GetDeltaTime());
                    
                    // 着地判定
                    if (controller.IsGrounded() && velocity.y <= 0.0f) {
                        velocity.y = 0.0f;
                        velocity.x = 0.0f; // 着地したらX方向の移動も止める
                        
                        // ジャンプからの着地時処理
                        if (state == BossState::SmallJump || state == BossState::BigJump) {
                            if (state == BossState::BigJump) {
                                DropRubbles(tf.GetTranslate());
                            }
                            ChangeState(BossState::Idle);
                        }
                    }
                    if (controller.IsTouchingCeiling() && velocity.y > 0.0f) {
                        velocity.y = 0.0f;
                    }
                } else {
                    // controllerがない場合の簡易フォールバック
                    tf.SetTranslate(tf.GetTranslate() + Vector3(velocity.x * GetDeltaTime(), velocity.y * GetDeltaTime(), 0.0f));
                    if (tf.GetTranslate().y <= 0.0f) {
                        Vector3 pos = tf.GetTranslate();
                        pos.y = 0.0f;
                        tf.SetTranslate(pos);
                        velocity.y = 0.0f;
                        velocity.x = 0.0f;

                        if (state == BossState::SmallJump || state == BossState::BigJump) {
                            if (state == BossState::BigJump) {
                                DropRubbles(pos);
                            }
                            ChangeState(BossState::Idle);
                        }
                    }
                }

                // Idle状態の更新
                if (state == BossState::Idle) {
                    stateTimer += GetDeltaTime();
                    if (stateTimer >= idleDuration) {
                        if (nextIsBigJump) {
                            ChangeState(BossState::BigJump);
                        } else {
                            ChangeState(BossState::SmallJump);
                        }
                        nextIsBigJump = !nextIsBigJump; // 次回のジャンプを切り替え
                    }
                }
            }
        }

        // 状態が変化したときのみアニメーションを更新
        if (state != lastState) {
            lastState = state;
            SetAnimation(state);
        }
    }

    // 状態変更処理
    void ChangeState(BossState newState) {
        state = newState;
        stateTimer = 0.0f;

        Transform@ tf = GetTransform();
        float directionX = 1.0f;

        // プレイヤーの方向を向く
        if (player !is null && tf !is null) {
            Transform@ playerTf = player.GetTransform();
            if (playerTf !is null) {
                if (playerTf.GetTranslate().x > tf.GetTranslate().x) {
                    directionX = 1.0f;
                    tf.SetRotate(Vector3(0.0f, 3.14159f, 0.0f));
                } else {
                    directionX = -1.0f;
                    tf.SetRotate(Vector3(0.0f, 0.0f, 0.0f));
                }
            }
        }

        // 状態に応じた処理
        switch (state) {
            case BossState::Idle:
                break;
            case BossState::SmallJump:
                velocity.y = smallJumpPower;
                velocity.x = moveSpeed * directionX;
                break;
            case BossState::BigJump:
                velocity.y = bigJumpPower;
                velocity.x = moveSpeed * directionX;
                break;
            case BossState::Dead:
                velocity.x = 0.0f;
                break;
        }
    }

    // SpriteAnimator を使ったアニメーション切り替え
    void SetAnimation(BossState animState) {
        array<ScriptComponent@>@ scripts;
        if (GetComponents(@scripts)) {
            for(int i = 0; i < scripts.length(); ++i){
                if(scripts[i].GetTag() == "AnimatorSC"){
                    switch (animState) {
                        case BossState::Idle:
                            scripts[i].CallMethod("PlayRow", 0);
                            scripts[i].CallMethod("SetFrameCount", 3);
                            break;
                        case BossState::SmallJump:
                        case BossState::BigJump:
                            scripts[i].CallMethod("PlayRow", 1);
                            scripts[i].CallMethod("SetFrameCount", 1);
                            break;
                        case BossState::Dead:
                            scripts[i].CallMethod("PlayRow", 2);
                            scripts[i].CallMethod("SetFrameCount", 1);
                            break;
                    }
                } 
            }
        }
    }

    // 岩を降らせる処理
    void DropRubbles(Vector3 basePos) {
        float startHeight = 150.0f; // 降らせる高さ
        float spreadWidth = 60.0f;  // 横方向への散らばり幅

        // 岩(小)の生成
        if (rubbleSmall !is null) {
            for (int i = 0; i < rubbleSmallCount; i++) {
                Object@ clone = GetScene().CloneObject(rubbleSmall, "CloneRubbleSmall");
                if (clone !is null) {
                    clone.SetActive(true);
                    Transform@ cloneTf = clone.GetTransform();
                    if (cloneTf !is null) {
                        float offsetX = (i % 2 == 0 ? 1.0f : -1.0f) * spreadWidth * (0.5f + (i * 0.2f));
                        Vector3 spawnPos = Vector3(basePos.x + offsetX, basePos.y + startHeight, basePos.z);
                        cloneTf.SetTranslate(spawnPos);
                    }
                    rubbles.insertLast(clone);
                }
            }
        }

        // 岩(中)の生成
        if (rubbleMedium !is null) {
            for (int i = 0; i < rubbleMediumCount; i++) {
                Object@ clone = GetScene().CloneObject(rubbleMedium, "CloneRubbleMedium");
                if (clone !is null) {
                    clone.SetActive(true);
                    Transform@ cloneTf = clone.GetTransform();
                    if (cloneTf !is null) {
                        float offsetX = (i % 2 == 0 ? -1.0f : 1.0f) * spreadWidth * (0.3f + (i * 0.3f));
                        Vector3 spawnPos = Vector3(basePos.x + offsetX, basePos.y + startHeight + 20.0f, basePos.z);
                        cloneTf.SetTranslate(spawnPos);
                    }
                    rubbles.insertLast(clone);
                }
            }
        }
    }

    void Damage(float amount) {
        if (state == BossState::Dead) return;
        hp -= amount;

        // ダメージ演出の開始
        isFlashing = true;
        damageFlashTimer = damageFlashDuration;
        if (sprite !is null) {
            sprite.SetInstanceColor(damageColor);
        }
    }

    void End() {
    }
}