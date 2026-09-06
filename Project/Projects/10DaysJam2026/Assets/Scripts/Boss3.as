enum Boss3State {
    Idle,
    BloodTelegraph,         // 通常のBlood攻撃の予備動作
    BloodAttack,            // 通常のBlood攻撃
    RubbleTelegraph,        // 通常のRubble攻撃の予備動作
    RubbleAttack,           // 通常のRubble攻撃
    BloodTargetedTelegraph, // 追尾型Blood攻撃の予備動作
    BloodTargetedAttack,    // 追尾型Blood攻撃
    RubbleCenterTelegraph,  // 中央集中Rubble攻撃の予備動作
    RubbleCenterAttack,     // 中央集中Rubble攻撃
    Dead
}

class Boss3 : ScriptComponentBehavior {
    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    [SerializeField, Tooltip("左側の弱点(Ovary)")]
    Object@ ovaryLeft;

    [SerializeField, Tooltip("右側の弱点(Ovary)")]
    Object@ ovaryRight;

    [SerializeField, Tooltip("左側弱点の初期HP(Ovary側のmaxHPと一致させること)")]
    float initialLeftOvaryHP = 20.0f;

    [SerializeField, Tooltip("右側弱点の初期HP(Ovary側のmaxHPと一致させること)")]
    float initialRightOvaryHP = 20.0f;

    [SerializeField, Tooltip("攻撃と攻撃の間の待機時間(チャンスタイム)(秒)")]
    float idleDuration = 2.0f;

    [SerializeField, Tooltip("各攻撃の予備動作時間(ピンチの兆候)(秒)")]
    float telegraphDuration = 1.2f;

    [SerializeField, Tooltip("各攻撃の演出時間(秒)")]
    float attackDuration = 0.5f;

    [SerializeField, Tooltip("Bloodのプレハブ")]
    Object@ bloodPrefab;

    [SerializeField, Tooltip("通常のBlood攻撃で生成する弾数")]
    int bloodCount = 3;

    [SerializeField, Tooltip("追尾型のBlood攻撃で生成する弾数")]
    int bloodTargetedCount = 5;

    [SerializeField, Tooltip("Blood同士の横方向の間隔")]
    float bloodSpreadX = 40.0f;

    [SerializeField, Tooltip("ボス基準位置からのBlood生成オフセット(Y)")]
    float bloodSpawnOffsetY = -10.0f;

    [SerializeField, Tooltip("岩(小)のプレハブ")]
    Object@ rubbleSmall;

    [SerializeField, Tooltip("岩(中)のプレハブ")]
    Object@ rubbleMedium;

    [SerializeField, Tooltip("通常の岩(小)の生成数")]
    int rubbleSmallCount = 2;

    [SerializeField, Tooltip("通常の岩(中)の生成数")]
    int rubbleMediumCount = 2;

    [SerializeField, Tooltip("中央集中の岩(小)の生成数")]
    int rubbleCenterSmallCount = 3;

    [SerializeField, Tooltip("中央集中の岩(中)の生成数")]
    int rubbleCenterMediumCount = 3;

    [SerializeField, Tooltip("岩を降らせる高さ")]
    float rubbleDropHeight = 150.0f;

    [SerializeField, Tooltip("岩の横方向の散らばり幅")]
    float rubbleSpreadWidth = 60.0f;

    [SerializeField, Tooltip("左側Ovary基準の岩生成オフセットX")]
    float rubbleOffsetLeftX = 0.0f;

    [SerializeField, Tooltip("右側Ovary基準の岩生成オフセットX")]
    float rubbleOffsetRightX = 0.0f;

    Boss3State state = Boss3State::Idle;
    Boss3State lastState = Boss3State::Idle;
    float stateTimer = 0.0f;

    // 攻撃パターンのサイクル用
    int attackCycle = 0; 

    float leftOvaryHP = 0.0f;
    float rightOvaryHP = 0.0f;
    float maxHP = 40.0f;

    int hpTier = 0;
    int lastHpTier = 0;

    Vector3 initialBossPos;

    void Start() {
        leftOvaryHP = initialLeftOvaryHP;
        rightOvaryHP = initialRightOvaryHP;
        maxHP = initialLeftOvaryHP + initialRightOvaryHP;
        if (maxHP <= 0.0f) maxHP = 1.0f;

        Transform@ tf = GetTransform();
        if (tf !is null) {
            initialBossPos = tf.GetTranslate();
        }

        hpTier = CalcHpTier();
        lastHpTier = hpTier;
        SetAnimation(state, hpTier);
    }

    void Update() {
        if (state == Boss3State::Dead) return;

        if (leftOvaryHP <= 0.0f && rightOvaryHP <= 0.0f) {
            ChangeState(Boss3State::Dead);
        } else {
            FacePlayer();
            stateTimer += GetDeltaTime();

            switch (state) {
                case Boss3State::Idle:
                    if (stateTimer >= idleDuration) {
                        // 順番に攻撃を行う
                        if (attackCycle == 0) ChangeState(Boss3State::BloodTelegraph);
                        else if (attackCycle == 1) ChangeState(Boss3State::RubbleTelegraph);
                        else if (attackCycle == 2) ChangeState(Boss3State::BloodTargetedTelegraph);
                        else if (attackCycle == 3) ChangeState(Boss3State::RubbleCenterTelegraph);
                        
                        attackCycle = (attackCycle + 1) % 4; // 次の攻撃へ
                    }
                    break;

                // 予備動作ステート
                case Boss3State::BloodTelegraph:
                    UpdateTelegraphMotion(stateTimer, telegraphDuration, 0);
                    if (stateTimer >= telegraphDuration) ChangeState(Boss3State::BloodAttack);
                    break;

                case Boss3State::RubbleTelegraph:
                    UpdateTelegraphMotion(stateTimer, telegraphDuration, 1);
                    if (stateTimer >= telegraphDuration) ChangeState(Boss3State::RubbleAttack);
                    break;

                case Boss3State::BloodTargetedTelegraph:
                    UpdateTelegraphMotion(stateTimer, telegraphDuration, 2);
                    if (stateTimer >= telegraphDuration) ChangeState(Boss3State::BloodTargetedAttack);
                    break;

                case Boss3State::RubbleCenterTelegraph:
                    UpdateTelegraphMotion(stateTimer, telegraphDuration, 3);
                    if (stateTimer >= telegraphDuration) ChangeState(Boss3State::RubbleCenterAttack);
                    break;

                // 攻撃発動ステート
                case Boss3State::BloodAttack:
                case Boss3State::RubbleAttack:
                case Boss3State::BloodTargetedAttack:
                case Boss3State::RubbleCenterAttack:
                    if (stateTimer >= attackDuration) {
                        ChangeState(Boss3State::Idle); // 攻撃が終わったらIdle
                    }
                    break;
            }
        }

        hpTier = CalcHpTier();
        if (state != lastState || hpTier != lastHpTier) {
            lastState = state;
            lastHpTier = hpTier;
            SetAnimation(state, hpTier);
        }
    }

    void ChangeState(Boss3State newState) {
        state = newState;
        stateTimer = 0.0f;

        switch (state) {
            case Boss3State::Idle:
            case Boss3State::Dead:
                ResetPosition();
                break;
            case Boss3State::BloodAttack:
                ResetPosition();
                SpawnBlood(false); // 通常Blood
                break;
            case Boss3State::RubbleAttack:
                ResetPosition();
                DropRubbles(false); // 通常Rubble
                break;
            case Boss3State::BloodTargetedAttack:
                ResetPosition();
                SpawnBlood(true); // 追尾型Blood
                break;
            case Boss3State::RubbleCenterAttack:
                ResetPosition();
                DropRubbles(true); // 中央集中Rubble
                break;
            default:
                break;
        }
    }

    // 攻撃種類ごとの予備動作
    void UpdateTelegraphMotion(float timer, float maxTime, int attackType) {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        float progress = timer / maxTime;
        float intensity = 8.0f * progress; // 時間経過で揺れを強くする
        float wave = ((int(timer * 60.0f) % 2) == 0) ? 1.0f : -1.0f;
        
        Vector3 pos = initialBossPos;

        if (attackType == 0) {
            // 通常Blood
            pos.x += wave * intensity;
            pos.y += 15.0f * progress;
        } 
        else if (attackType == 1) {
            // 通常Rubble
            pos.y += (wave * intensity) - (15.0f * progress);
        }
        else if (attackType == 2) {
            // 追尾Blood
            pos.x += wave * (intensity * 1.5f);
        }
        else if (attackType == 3) {
            // 中央Rubble
            float wave2 = ((int(timer * 60.0f + 1) % 2) == 0) ? 1.0f : -1.0f;
            pos.x += wave * intensity;
            pos.y += 10.0f * progress + (wave2 * intensity);
        }

        tf.SetTranslate(pos);
    }

    void ResetPosition() {
        Transform@ tf = GetTransform();
        if (tf !is null) {
            tf.SetTranslate(initialBossPos);
        }
    }

    void FacePlayer() {
        Transform@ tf = GetTransform();
        if (player is null || tf is null) return;
        Transform@ playerTf = player.GetTransform();
        if (playerTf is null) return;

        if (playerTf.GetTranslate().x > tf.GetTranslate().x) {
            tf.SetRotate(Vector3(0.0f, 3.14159f, 0.0f));
        } else {
            tf.SetRotate(Vector3(0.0f, 0.0f, 0.0f));
        }
    }

    int CalcHpTier() {
        float ratio = (leftOvaryHP + rightOvaryHP) / maxHP;
        if (ratio > 0.75f) return 0;
        if (ratio > 0.5f) return 1;
        if (ratio > 0.25f) return 2;
        return 3;
    }

    // Blood攻撃の生成
    void SpawnBlood(bool isTargeted) {
        if (bloodPrefab is null) return;
        Transform@ tf = GetTransform();
        if (tf is null) return;

        Vector3 basePos = tf.GetTranslate();
        int count = isTargeted ? bloodTargetedCount : bloodCount;
        
        // 追尾の場合はプレイヤーのX座標を基準にする
        if (isTargeted && player !is null) {
            Transform@ pTf = player.GetTransform();
            if (pTf !is null) {
                basePos.x = pTf.GetTranslate().x;
                basePos.y += 40.0f; // ボスより少し高い位置から降らせる
            }
        }

        float centerOffset = float(count - 1) * 0.5f;

        for (int i = 0; i < count; i++) {
            Object@ clone = GetScene().CloneObject(bloodPrefab, "CloneBlood");
            if (clone is null) continue;
            clone.SetActive(true);
            Transform@ cloneTf = clone.GetTransform();
            if (cloneTf !is null) {
                // 追尾型の場合は少し狭めの間隔で降らせる
                float currentSpread = isTargeted ? (bloodSpreadX * 0.6f) : bloodSpreadX;
                float offsetX = (float(i) - centerOffset) * currentSpread;
                Vector3 spawnPos = Vector3(basePos.x + offsetX, basePos.y + bloodSpawnOffsetY, basePos.z);
                cloneTf.SetTranslate(spawnPos);
            }
        }
    }

    // Rubble攻撃の生成
    void DropRubbles(bool isCenter) {
        Transform@ tf = GetTransform();
        if (tf is null) return;
        Vector3 basePos = tf.GetTranslate();
        
        Vector3 leftPos = basePos;
        Vector3 rightPos = basePos;

        if (!isCenter) {
            if (ovaryLeft !is null) {
                Transform@ leftTf = ovaryLeft.GetTransform();
                if (leftTf !is null) leftPos = leftTf.GetTranslate();
                leftPos.x += rubbleOffsetLeftX;
            }
            if (ovaryRight !is null) {
                Transform@ rightTf = ovaryRight.GetTransform();
                if (rightTf !is null) rightPos = rightTf.GetTranslate();
                rightPos.x += rubbleOffsetRightX;
            }
        }

        int sCount = isCenter ? rubbleCenterSmallCount : rubbleSmallCount;
        if (rubbleSmall !is null) {
            for (int i = 0; i < sCount; i++) {
                Object@ clone = GetScene().CloneObject(rubbleSmall, "CloneRubbleSmall");
                if (clone !is null) {
                    clone.SetActive(true);
                    Transform@ cloneTf = clone.GetTransform();
                    if (cloneTf !is null) {
                        Vector3 targetPos = isCenter ? basePos : ((i % 2 == 0) ? leftPos : rightPos);
                        float spread = isCenter ? (rubbleSpreadWidth * 1.2f) : rubbleSpreadWidth;
                        float offsetX = (i % 2 == 0 ? 1.0f : -1.0f) * spread * (0.2f + (i * 0.2f));
                        Vector3 spawnPos = Vector3(targetPos.x + offsetX, targetPos.y + rubbleDropHeight, targetPos.z);
                        cloneTf.SetTranslate(spawnPos);
                    }
                }
            }
        }

        int mCount = isCenter ? rubbleCenterMediumCount : rubbleMediumCount;
        if (rubbleMedium !is null) {
            for (int i = 0; i < mCount; i++) {
                Object@ clone = GetScene().CloneObject(rubbleMedium, "CloneRubbleMedium");
                if (clone !is null) {
                    clone.SetActive(true);
                    Transform@ cloneTf = clone.GetTransform();
                    if (cloneTf !is null) {
                        Vector3 targetPos = isCenter ? basePos : ((i % 2 == 0) ? rightPos : leftPos);
                        float spread = isCenter ? (rubbleSpreadWidth * 1.5f) : rubbleSpreadWidth;
                        float offsetX = (i % 2 == 0 ? -1.0f : 1.0f) * spread * (0.1f + (i * 0.3f));
                        Vector3 spawnPos = Vector3(targetPos.x + offsetX, targetPos.y + rubbleDropHeight + 20.0f, targetPos.z);
                        cloneTf.SetTranslate(spawnPos);
                    }
                }
            }
        }
    }

    void OnLeftOvaryHPChanged(float hp) { leftOvaryHP = hp; }
    void OnRightOvaryHPChanged(float hp) { rightOvaryHP = hp; }
    void Damage(float amount) { }

    void SetAnimation(Boss3State animState, int tier) {
        const int rowsPerTier = 4;
        array<ScriptComponent@>@ scripts;
        if (GetComponents(@scripts)) {
            for (int i = 0; i < scripts.length(); ++i) {
                if (scripts[i].GetTag() == "AnimatorSC") {
                    int stateOffset = 0;
                    int frameCount = 1;

                    switch (animState) {
                        case Boss3State::Idle:
                        case Boss3State::BloodTelegraph:
                        case Boss3State::RubbleTelegraph:
                        case Boss3State::BloodTargetedTelegraph:
                        case Boss3State::RubbleCenterTelegraph:
                            stateOffset = 0; // 待機や予備動作はすべてIdleアニメーション
                            frameCount = 3;
                            break;
                        case Boss3State::BloodAttack:
                        case Boss3State::BloodTargetedAttack:
                            stateOffset = 1; // Blood系の攻撃アニメーション
                            frameCount = 4;
                            break;
                        case Boss3State::RubbleAttack:
                        case Boss3State::RubbleCenterAttack:
                            stateOffset = 2; // Rubble系の攻撃アニメーション
                            frameCount = 4;
                            break;
                        case Boss3State::Dead:
                            stateOffset = 3;
                            frameCount = 1;
                            break;
                    }

                    int row = tier * rowsPerTier + stateOffset;
                    scripts[i].CallMethod("PlayRow", row);
                    scripts[i].CallMethod("SetFrameCount", frameCount);
                }
            }
        }
    }

    void End() { }
}