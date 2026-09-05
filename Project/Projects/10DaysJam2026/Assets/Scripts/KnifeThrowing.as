class KnifeThrowing : ScriptComponentBehavior {
    [SerializeField, Tooltip("ナイフの発射間隔")]
    float shotDuration = 2.0f;

    [SerializeField, Tooltip("ナイフオブジェクト")]
    Object@ knife;

    [SerializeField, Tooltip("ナイフの発射方向(X)。右向きなら1.0、左向きなら-1.0")]
    float shotDirectionX = 1.0f;

    [SerializeField, Tooltip("ナイフの生存時間(秒)")]
    float knifeLifeTime = 5.0f;

    [SerializeField, Tooltip("ナイフの発生位置オフセット")]
    Vector3 knifeOffset = Vector3(0.0f, 0.0f, 0.0f);

    [SerializeField, Tooltip("前後の揺れ幅")]
    float hoverAmplitude = 8.0f;

    [SerializeField, Tooltip("1往復にかかる時間(秒)")]
    float hoverCycle = 2.0f;

    [SerializeField, Tooltip("HP")]
    float hp = 1.0f;

    [SerializeField, Tooltip("expValue")]
    float expValue = 5.0f;

    [SerializeField, Tooltip("無敵時間(秒)")]
    float invincibleDuration = 0.5f;

    [SerializeField, Tooltip("点滅の切り替え間隔(秒)")]
    float blinkInterval = 0.08f;

    [SerializeField, Tooltip("死亡エフェクト")]
    Object@ deathEffect;

    [SerializeField, Tooltip("セルフオブジェ")]
    Object@ self;

    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    // 発射タイマー
    float shotTimer = 0.0f;

    // 揺れ計算用の変数
    Vector3 basePos;
    float hoverTimer = 0.0f;
    bool isBasePosInitialized = false;

    // 発射したナイフのクローン管理用
    array<Object@> knifeClones;
    array<float> knifeTimers;

    Object@ cloneEffect;

    bool isAlive = true;
    bool isAnimation = false;
    float deathEffectTimer = 0.0f;

    // 無敵時間・点滅管理用
    bool isInvincible = false;
    float invincibleTimer = 0.0f;
    SpriteRenderer@ sprite;

    void Start() {
        GetComponent(@sprite);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 基準座標の初期化（ノックバック等の影響を防ぐため初回のみ保存）
        if (!isBasePosInitialized) {
            basePos = tf.GetTranslate();
            isBasePosInitialized = true;
        }

        // 生存時の無敵時間タイマー更新および点滅処理
        if (isAlive && isInvincible) {
            invincibleTimer += GetDeltaTime();

            bool isVisible = (int(invincibleTimer / blinkInterval) % 2 == 0);
            if (sprite !is null) {
                sprite.SetActive(isVisible);
            }

            if (invincibleTimer >= invincibleDuration) {
                invincibleTimer = 0.0f;
                isInvincible = false;
                if (sprite !is null) {
                    sprite.SetActive(true);
                }
            }
        }

        // HPが0になったら
        if(hp <= 0.0f){
            isAlive = false;
        }

        if(isAlive){
            // 揺れる処理
            if (hoverCycle > 0.0f) {
                hoverTimer += GetDeltaTime();
                float timeRatio = hoverTimer / hoverCycle;
                timeRatio -= int(timeRatio); 
                float wave = (timeRatio < 0.5f) ? (timeRatio * 4.0f - 1.0f) : (3.0f - timeRatio * 4.0f);
                float v = (wave + 1.0f) * 0.5f;
                float smoothV = v * v * (3.0f - 2.0f * v);
                float finalWave = smoothV * 2.0f - 1.0f;

                // X軸に揺れを適用する
                Vector3 newPos = basePos;
                newPos.x = basePos.x + finalWave * hoverAmplitude;
                tf.SetTranslate(newPos);
            }

            // プレイヤーの方を向く処理
            if (player !is null) {
                Transform@ playerTf = player.GetTransform();
                if (playerTf !is null) {
                    if (playerTf.GetTranslate().x > tf.GetTranslate().x) {
                        // プレイヤーが右側にいる場合
                        shotDirectionX = 1.0f;
                        tf.SetRotate(Vector3(0.0f, 3.14159f, 0.0f));
                    } else {
                        // プレイヤーが左側にいる場合
                        shotDirectionX = -1.0f;
                        tf.SetRotate(Vector3(0.0f, 0.0f, 0.0f));
                    }
                }
            }

            shotTimer += GetDeltaTime();

            // 発射間隔ごとにアニメーション待機なしで即座に投擲する
            if(shotTimer >= shotDuration){
                shotTimer = 0.0f;

                if (knife !is null) {
                    // 向いている方向によってオフセットのX方向も反転させる
                    Vector3 spawnOffset = Vector3(knifeOffset.x * shotDirectionX, knifeOffset.y, knifeOffset.z);
                    Vector3 spawnPos = tf.GetTranslate() + spawnOffset;

                    Object@ cloneKnife = GetScene().CloneObject(knife, "CloneEnemyKnife");
                    if (cloneKnife !is null) {
                        cloneKnife.SetActive(true);

                        Transform@ cloneTf = cloneKnife.GetTransform();
                        if (cloneTf !is null) {
                            cloneTf.SetTranslate(spawnPos);
                        }

                        ScriptComponent@ cloneSc;
                        if (cloneKnife.GetComponent(@cloneSc)) {
                            cloneSc.SetVariable("pos", spawnPos);
                            cloneSc.CallMethod("Attack", shotDirectionX);
                        }

                        knifeClones.insertLast(cloneKnife);
                        knifeTimers.insertLast(0.0f);
                    }
                }
            }
        }

        // 死亡時処理
        if(!isAlive){
            if(!isAnimation){
                if (player !is null) {
                    ScriptComponent@ playerSc;
                    if (player.GetComponent(@playerSc)) {
                        playerSc.CallMethod("AddExp", expValue);
                    }
                }

                @cloneEffect = GetScene().CloneObject(deathEffect, "CloneDeathEffect");

                if(cloneEffect !is null){
                    Transform@ cloneTf = cloneEffect.GetTransform();
                    if(cloneTf !is null){
                        cloneTf.SetScale(Vector3(32.0f, 32.0f, 1.0f));
                    }

                    ScriptComponent@ sc;
                    if(cloneEffect.GetComponent(@sc)){
                        sc.CallMethod("StartAnimation");
                        sc.SetVariable("pos", tf.GetTranslate());
                    }
                }

                self.SetComponentsActiveExceptTransformAndScript(false);
                isAnimation = true;
            }

            if(cloneEffect !is null){
                deathEffectTimer += GetDeltaTime();
                float deathEffectDuration = 0.6f;
                ScriptComponent@ sc;
                if(cloneEffect.GetComponent(@sc)){
                    sc.CallMethod("UpdateAnimation");

                    if(deathEffectTimer >= deathEffectDuration){
                        cloneEffect.SetActive(false);
                    }
                }
            }
        }

        // 発射済みのナイフの更新および削除処理
        for (uint i = 0; i < knifeClones.length(); ) {
            Object@ clone = knifeClones[i];

            if (clone !is null) {
                knifeTimers[i] += GetDeltaTime();

                if (!clone.IsActive() || knifeTimers[i] >= knifeLifeTime) {
                    clone.SetActive(false);
                    knifeClones.removeAt(i);
                    knifeTimers.removeAt(i);
                    continue; 
                }
            } else {
                knifeClones.removeAt(i);
                knifeTimers.removeAt(i);
                continue;
            }

            i++;
        }
    }

    void Damage(float amount) {
        if (!isAlive || isInvincible) return;

        hp -= amount;
        isInvincible = true;
        invincibleTimer = 0.0f;
    }

    void End() {
    }
}