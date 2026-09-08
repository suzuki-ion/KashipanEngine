enum Boss3State {
    Idle,
    BloodTelegraph,  // Blood攻撃の予備動作
    BloodAttack,     // Blood攻撃
    RubbleTelegraph, // Rubble攻撃の予備動作
    RubbleAttack,    // Rubble攻撃
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

    [SerializeField, Tooltip("Rubble攻撃の演出時間(秒)/ Blood攻撃では全バースト終了後の締めの待機時間としても使用")]
    float attackDuration = 0.5f;

    [SerializeField, Tooltip("Bloodのプレハブ")]
    Object@ bloodPrefab;

    [SerializeField, Tooltip("この攻撃全体で発射を連続して行う回数(バースト数)")]
    int bloodBurstCount = 5;

    [SerializeField, Tooltip("1回の発射で放つBloodの数(方向数)。インスペクターで調整可能")]
    int bloodDirectionCount = 3;

    [SerializeField, Tooltip("一斉発射時の速度(速め推奨)")]
    float bloodBurstSpeed = 260.0f;

    [SerializeField, Tooltip("Bloodを1体ずつ生成する間隔(秒)")]
    float bloodSpawnInterval = 0.05f;

    [SerializeField, Tooltip("1回の発射が完了してから次の発射を始めるまでの間隔(秒)")]
    float bloodBurstInterval = 0.4f;

    [SerializeField, Tooltip("ボスの中心座標からのBlood発射位置オフセット。下の方から出す場合はYをマイナスにする")]
    Vector2 bloodSpawnOffset = Vector2(0.0f, -40.0f);

    [SerializeField, Tooltip("Bloodが扇状に広がる角度の範囲(度)。0にすると全方向とも同じ向きに飛ぶ")]
    float bloodFanAngle = 90.0f;

    [SerializeField, Tooltip("扇の中心となる発射方向(度)。0=右、90=上、-90=下、180=左")]
    float bloodFanCenterAngleDeg = -90.0f;

    [SerializeField, Tooltip("岩(中)のプレハブ")]
    Object@ rubbleMedium;

    [SerializeField, Tooltip("岩(中)の生成数")]
    int rubbleMediumCount = 4;

    [SerializeField, Tooltip("岩を降らせる高さ(固定)")]
    float rubbleDropHeight = 150.0f;

    [SerializeField, Tooltip("Ovary位置を基準にどれくらいランダムに横方向へばらけさせるか")]
    float rubbleRandomRangeX = 50.0f;

    [SerializeField, Tooltip("左側Ovary基準の岩生成オフセットX")]
    float rubbleOffsetLeftX = 0.0f;

    [SerializeField, Tooltip("右側Ovary基準の岩生成オフセットX")]
    float rubbleOffsetRightX = 0.0f;

    [SerializeField, Tooltip("岩を1つ落とすごとにずらす落下開始遅延(秒)")]
    float rubbleDropDelayStep = 0.2f;

    [SerializeField, Tooltip("待機中・Blood攻撃中に上下に動く幅(振幅)")]
    float moveAmplitude = 10.0f;

    [SerializeField, Tooltip("待機中・Blood攻撃中に上下に動く速さ")]
    float moveSpeed = 2.0f;

    [SerializeField, Tooltip("死亡エフェクト")]
    Object@ deathEffect;

    [SerializeField, Tooltip("死亡エフェクトの発生数")]
    int deathEffectCount = 5;

    [SerializeField, Tooltip("死亡エフェクトの散らばる範囲(半径)")]
    float deathEffectSpread = 50.0f;

    [SerializeField, Tooltip("死亡エフェクトの生成間隔(秒)")]
    float deathEffectSpawnInterval = 0.15f;

    [SerializeField, Tooltip("Boss3撃破後に出現させるFinalBossのプレハブ")]
    Object@ finalBossPrefab;

    [SerializeField, Tooltip("FinalBossの出現位置オフセット(ボスの上の方から出す場合はYをプラスにする)")]
    Vector2 finalBossSpawnOffset = Vector2(0.0f, 60.0f);

    Boss3State state = Boss3State::Idle;
    Boss3State lastState = Boss3State::Idle;
    float stateTimer = 0.0f;

    // 上下移動用タイマー
    float moveTimer = 0.0f;

    // 攻撃パターンのサイクル用
    int attackCycle = 0;

    float leftOvaryHP = 0.0f;
    float rightOvaryHP = 0.0f;
    float maxHP = 40.0f;

    int hpTier = 0;
    int lastHpTier = 0;

    Vector3 initialBossPos;

    // Blood攻撃の進行管理用
    int bloodBurstsFired = 0;
    int spawnedBloodCount = 0;
    float bloodSpawnTimer = 0.0f;
    bool isWaitingNextBurst = false;
    float burstWaitTimer = 0.0f;
    Vector3 currentBurstCenter;
    array<Object@> currentBurstBlood;
    array<float> currentBurstAngle;

    // 死亡エフェクト管理用
    int spawnedEffectCount = 0;
    float deathEffectTimer = 0.0f;
    bool isAnimation = false;
    array<Object@> cloneEffects;

    // FinalBoss出現管理用(二重出現防止)
    bool finalBossSpawned = false;

    // 疑似乱数
    uint rngState = 88172645;

    // スプライト
    SpriteRenderer@ sprite;

    void Start() {
        GetComponent(@sprite);

        // インスペクタの設定ミスを防ぐため、Boss側から各Ovaryへ強制的に左右属性をセットする
        if (ovaryLeft !is null) {
            ScriptComponent@ scLeft;
            if (ovaryLeft.GetComponent(@scLeft)) {
                scLeft.SetVariable("isLeftOvary", true);
                scLeft.SetVariable("boss", GetOwnerObject());
            }
        }
        if (ovaryRight !is null) {
            ScriptComponent@ scRight;
            if (ovaryRight.GetComponent(@scRight)) {
                scRight.SetVariable("isLeftOvary", false);
                scRight.SetVariable("boss", GetOwnerObject());
            }
        }

        leftOvaryHP = initialLeftOvaryHP;
        rightOvaryHP = initialRightOvaryHP;
        maxHP = initialLeftOvaryHP + initialRightOvaryHP;
        if (maxHP <= 0.0f) maxHP = 1.0f;

        Transform@ tf = GetTransform();
        if (tf !is null) {
            initialBossPos = tf.GetTranslate();
            // ボスの座標を種としてばらつきを持たせる
            rngState = uint(Abs(initialBossPos.x) * 1000.0f) + uint(Abs(initialBossPos.y) * 37.0f) + 12345;
            if (rngState == 0) rngState = 12345;
        }

        hpTier = CalcHpTier();
        lastHpTier = hpTier;
        SetAnimation(state, hpTier);
    }

    void Update() {
        // 会話中(isDialogueActive)は移動・攻撃などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        // まだDead状態になっておらず、左右の弱点が両方ともHP0以下になった場合に死亡状態へ移行
        if (state != Boss3State::Dead && leftOvaryHP <= 0.0f && rightOvaryHP <= 0.0f) {
            ChangeState(Boss3State::Dead);
        }

        if (state == Boss3State::Dead) {
            // 死亡演出の更新処理
            UpdateDeathAnimation();
        } else {
            stateTimer += GetDeltaTime();

            switch (state) {
                case Boss3State::Idle:
                    // 待機中の上下移動
                    moveTimer += GetDeltaTime();
                    {
                        Transform@ tf = GetTransform();
                        if (tf !is null) {
                            Vector3 pos = initialBossPos;
                            pos.y += Sin(moveTimer * moveSpeed) * moveAmplitude;
                            tf.SetTranslate(pos);
                        }
                    }

                    if (stateTimer >= idleDuration) {
                        if (attackCycle == 0) ChangeState(Boss3State::BloodTelegraph);
                        else ChangeState(Boss3State::RubbleTelegraph);

                        attackCycle = (attackCycle + 1) % 2; // Blood/Rubbleを交互に
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

                // 攻撃発動ステート
                case Boss3State::BloodAttack:
                    // Blood攻撃中も上下移動を継続
                    moveTimer += GetDeltaTime();
                    {
                        Transform@ tf = GetTransform();
                        if (tf !is null) {
                            Vector3 pos = initialBossPos;
                            pos.y += Sin(moveTimer * moveSpeed) * moveAmplitude;
                            tf.SetTranslate(pos);
                        }
                    }
                    UpdateBloodAttack();
                    break;

                case Boss3State::RubbleAttack:
                    if (stateTimer >= attackDuration) {
                        ChangeState(Boss3State::Idle);
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

        // HPに応じたスプライトの変化
        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(Vector2(0.0f, float(hpTier) * 0.25f));
        }
    }
    
    // 死亡エフェクトの生成と更新処理
    void UpdateDeathAnimation() {
        if (!isAnimation) {
            isAnimation = true;
            deathEffectTimer = 0.0f;
            spawnedEffectCount = 0; // 生成数の初期化
        }

        deathEffectTimer += GetDeltaTime();

        // エフェクトを順番に生成
        if (spawnedEffectCount < deathEffectCount) {
            // 経過時間が生成タイミングに達したら生成
            if (deathEffectTimer >= float(spawnedEffectCount) * deathEffectSpawnInterval) {
                Transform@ tf = GetTransform();
                Vector3 centerPos = (tf !is null) ? tf.GetTranslate() : initialBossPos;

                Object@ clone = GetScene().CloneObject(deathEffect, "CloneDeathEffect_" + spawnedEffectCount);
                if (clone !is null) {
                    Transform@ cloneTf = clone.GetTransform();
                    if (cloneTf !is null) {
                        cloneTf.SetScale(Vector3(32.0f, 32.0f, 1.0f));
                    }

                    ScriptComponent@ sc;
                    if (clone.GetComponent(@sc)) {
                        sc.CallMethod("StartAnimation");
                        
                        float angle = 6.28318f * (float(spawnedEffectCount) / float(deathEffectCount));
                        float r = deathEffectSpread * (spawnedEffectCount % 2 == 0 ? 1.0f : 0.6f); 
                        
                        Vector3 spawnPos = Vector3(
                            centerPos.x + Cos(angle) * r,
                            centerPos.y + Sin(angle) * r,
                            centerPos.z - 0.1f 
                        );
                        
                        sc.SetVariable("pos", spawnPos);
                    }
                    cloneEffects.insertLast(clone);
                }
                spawnedEffectCount++;
            }
        }

        // アニメーション更新処理
        bool allEffectsFinished = true;
        if (cloneEffects.length() > 0) {
            float deathEffectDuration = 0.6f;
            
            for (uint i = 0; i < cloneEffects.length(); i++) {
                Object@ clone = cloneEffects[i];
                if (clone !is null && clone.IsActive()) {
                    ScriptComponent@ sc;
                    if (clone.GetComponent(@sc)) {
                        sc.CallMethod("UpdateAnimation");
                    }

                    // 各エフェクトが生成されてからの経過時間
                    float effectAge = deathEffectTimer - (float(i) * deathEffectSpawnInterval);

                    // アニメーションが終了したら非アクティブ化
                    if (effectAge >= deathEffectDuration) {
                        clone.SetActive(false);
                    } else {
                        allEffectsFinished = false; // まだ終わっていないエフェクトがある
                    }
                }
            }
        } else {
            allEffectsFinished = false; // まだ1つも生成されていない
        }

        // 全てのエフェクトが生成され、かつ全て終了した場合にボスの姿を消し、FinalBossを出現させる
        if (spawnedEffectCount >= deathEffectCount && allEffectsFinished) {
            if (!finalBossSpawned) {
                SpawnFinalBoss();
                finalBossSpawned = true;
            }
            GetOwnerObject().SetComponentsActiveExceptTransformAndScript(false);
        }
    }

    // Boss3撃破後、FinalBossをBoss3の上部から出現させる
    void SpawnFinalBoss() {
        if (finalBossPrefab is null) return;

        Object@ clone = GetScene().CloneObject(finalBossPrefab, "FinalBoss");
        if (clone is null) return;

        clone.SetActive(true);
        Transform@ cloneTf = clone.GetTransform();
        if (cloneTf !is null) {
            Vector3 spawnPos = Vector3(
                initialBossPos.x + finalBossSpawnOffset.x,
                initialBossPos.y + finalBossSpawnOffset.y,
                initialBossPos.z
            );
            cloneTf.SetTranslate(spawnPos);
        }
    }

    void ChangeState(Boss3State newState) {
        state = newState;
        stateTimer = 0.0f;

        switch (state) {
            case Boss3State::Idle:
                moveTimer = 0.0f; // 中心から滑らかに動かし始める
                ResetPosition();
                break;
            case Boss3State::Dead:
                ResetPosition();
                break;
            case Boss3State::BloodAttack:
                moveTimer = 0.0f; // Blood攻撃開始時もタイマーをリセットして滑らかに動かす
                ResetPosition();
                StartBloodAttack();
                break;
            case Boss3State::RubbleAttack:
                ResetPosition();
                DropRubbles();
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
            // Blood
            pos.x += wave * intensity;
            pos.y += 15.0f * progress;
        } else if (attackType == 1) {
            // Rubble
            pos.y += (wave * intensity) - (15.0f * progress);
        }

        tf.SetTranslate(pos);
    }

    void ResetPosition() {
        Transform@ tf = GetTransform();
        if (tf !is null) {
            tf.SetTranslate(initialBossPos);
        }
    }

    int CalcHpTier() {
        float ratio = (leftOvaryHP + rightOvaryHP) / maxHP;
        if (ratio > 0.75f) return 0;
        if (ratio > 0.5f) return 1;
        if (ratio > 0.25f) return 2;
        return 3;
    }

    // Blood攻撃
    void StartBloodAttack() {
        bloodBurstsFired = 0;
        spawnedBloodCount = 0;
        bloodSpawnTimer = 0.0f;
        isWaitingNextBurst = false;
        burstWaitTimer = 0.0f;
        currentBurstBlood.resize(0);
        currentBurstAngle.resize(0);
        PickBloodSpawnPoint();
    }

    void UpdateBloodAttack() {
        float dt = GetDeltaTime();

        if (isWaitingNextBurst) {
            // 発射後の間隔待ち、または全バースト終了後の締めの待機
            burstWaitTimer += dt;
            float waitTarget = (bloodBurstsFired < bloodBurstCount) ? bloodBurstInterval : attackDuration;
            if (burstWaitTimer >= waitTarget) {
                isWaitingNextBurst = false;
                burstWaitTimer = 0.0f;

                if (bloodBurstsFired >= bloodBurstCount) {
                    ChangeState(Boss3State::Idle);
                } else {
                    spawnedBloodCount = 0;
                    currentBurstBlood.resize(0);
                    currentBurstAngle.resize(0);
                    PickBloodSpawnPoint();
                }
            }
            return;
        }

        if (spawnedBloodCount < bloodDirectionCount) {
            // Bloodを1体ずつ生成していく
            bloodSpawnTimer += dt;
            if (bloodSpawnTimer >= bloodSpawnInterval) {
                SpawnOneBlood(spawnedBloodCount);
                spawnedBloodCount++;
                bloodSpawnTimer = 0.0f;
            }
        } else {
            // 配置が完了したので各方向へ一斉発射
            LaunchBurstBlood();
            bloodBurstsFired++;
            isWaitingNextBurst = true;
            burstWaitTimer = 0.0f;
        }
    }

    // ボスの下の方(bloodSpawnOffset分ずらした位置)を、今回の発射位置として選ぶ
    void PickBloodSpawnPoint() {
        Vector3 basePos = initialBossPos;
        Transform@ tf = GetTransform();
        if (tf !is null) basePos = tf.GetTranslate();

        currentBurstCenter = Vector3(
            basePos.x + bloodSpawnOffset.x,
            basePos.y + bloodSpawnOffset.y,
            basePos.z
        );
    }

    // 発射位置にBloodを1体だけ生成する(この時点では全方向とも同じ位置に生成される)
    void SpawnOneBlood(int index) {
        if (bloodPrefab is null || bloodDirectionCount <= 0) return;

        float angle = CalcFanAngle(index);

        Object@ clone = GetScene().CloneObject(bloodPrefab, "CloneBlood");
        if (clone is null) return;

        clone.SetActive(true);
        Transform@ cloneTf = clone.GetTransform();
        if (cloneTf !is null) {
            cloneTf.SetTranslate(currentBurstCenter);
        }

        currentBurstBlood.insertLast(clone);
        currentBurstAngle.insertLast(angle);
    }

    // bloodDirectionCount本を、bloodFanCenterAngleDeg方向を中心にbloodFanAngle度の扇状に均等配分した角度(ラジアン)を求める
    float CalcFanAngle(int index) {
        const float degToRad = 3.14159265f / 180.0f;
        float centerAngle = bloodFanCenterAngleDeg * degToRad;

        if (bloodDirectionCount <= 1) return centerAngle;

        float fanRad = bloodFanAngle * degToRad;
        float startAngle = centerAngle - fanRad * 0.5f;
        float step = fanRad / float(bloodDirectionCount - 1);
        return startAngle + step * float(index);
    }

    // 発射位置に並んだBloodを、それぞれ扇状の方向へ一斉に発射する
    void LaunchBurstBlood() {
        for (uint i = 0; i < currentBurstBlood.length(); ++i) {
            Object@ b = currentBurstBlood[i];
            if (b is null) continue;

            float angle = currentBurstAngle[i];
            Vector2 vel = Vector2(Cos(angle) * bloodBurstSpeed, Sin(angle) * bloodBurstSpeed);

            ScriptComponent@ sc;
            if (b.GetComponent(@sc)) {
                sc.CallMethod("Launch", vel);
            }
        }
    }

    // Rubble攻撃
    void DropRubbles() {
        if (rubbleMedium is null) return;

        Transform@ tf = GetTransform();
        if (tf is null) return;
        Vector3 basePos = tf.GetTranslate();

        Vector3 leftPos = basePos;
        Vector3 rightPos = basePos;

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

        float currentDelay = 0.0f;

        for (int i = 0; i < rubbleMediumCount; i++) {
            Object@ clone = GetScene().CloneObject(rubbleMedium, "CloneRubbleMedium");
            if (clone is null) continue;

            clone.SetActive(true);
            Transform@ cloneTf = clone.GetTransform();
            if (cloneTf !is null) {
                Vector3 targetPos = (i % 2 == 0) ? leftPos : rightPos;
                float offsetX = RandomRange(-rubbleRandomRangeX, rubbleRandomRangeX);
                Vector3 spawnPos = Vector3(targetPos.x + offsetX, targetPos.y + rubbleDropHeight, targetPos.z);
                cloneTf.SetTranslate(spawnPos);
            }

            ScriptComponent@ sc;
            if (clone.GetComponent(@sc)) {
                sc.CallMethod("SetDropDelay", currentDelay);
            }
            currentDelay += rubbleDropDelayStep;
        }
    }

    void OnLeftOvaryHPChanged(float hp) { leftOvaryHP = hp; }
    void OnRightOvaryHPChanged(float hp) { rightOvaryHP = hp; }
    void Damage(float amount) { }

    float NextRandom01() {
        rngState = rngState * 1664525 + 1013904223;
        return float(rngState % 100000) / 100000.0f;
    }

    float RandomRange(float minVal, float maxVal) {
        return minVal + NextRandom01() * (maxVal - minVal);
    }

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
                            stateOffset = 0;
                            frameCount = 3;
                            break;
                        case Boss3State::BloodAttack:
                            stateOffset = 1;
                            frameCount = 4;
                            break;
                        case Boss3State::RubbleAttack:
                            stateOffset = 2;
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