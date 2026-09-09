class Boss2 : ScriptComponentBehavior {
    [SerializeField, Tooltip("プレイヤーとの距離(振り子の半径)")]
    float pendulumRadius = 100.0f;

    [SerializeField, Tooltip("振り子の振れ幅(ラジアン)")]
    float pendulumSwingRange = 0.8f;

    [SerializeField, Tooltip("振り子の揺れる速さ")]
    float pendulumSpeed = 1.5f;

    [SerializeField, Tooltip("上下方向のおまけの揺れ幅(ふわふわ感の演出用)")]
    float floatAmplitude = 10.0f;

    [SerializeField, Tooltip("上下方向のおまけの揺れの速さ")]
    float floatSpeed = 1.5f;

    [SerializeField, Tooltip("プレイヤーから最低限離れる距離(エリア切替=ワープ時にプレイヤーと重ならないようにする)")]
    float minDistanceFromPlayer = 100.0f;

    [SerializeField, Tooltip("カメラの表示範囲からこの距離(px)以内であれば移動・攻撃を行う(画面端での見切れ猶予)")]
    float cameraActivationMargin = 16.0f;

    [SerializeField, Tooltip("カメラ表示範囲内に留めるための余白(px)。この余白の分だけ内側までしか移動しない")]
    float cameraClampMargin = 16.0f;

    [SerializeField, Tooltip("カメラ範囲内に収まってから移動・攻撃を開始するまでの猶予時間(秒)")]
    float cameraActivationDelay = 2.0f;

    [SerializeField, Tooltip("攻撃間隔(秒)")]
    float attackInterval = 3.0f;

    [SerializeField, Tooltip("攻撃前の予備動作時間(秒)")]
    float attackWindup = 0.5f;

    [SerializeField, Tooltip("HP")]
    float hp = 30.0f;

    [SerializeField, Tooltip("ダメージを受けた際の色変化時間(秒)")]
    float damageFlashDuration = 0.1f;

    [SerializeField, Tooltip("ダメージ時の色")]
    Vector4 damageColor = Vector4(10.0f, 10.0f, 10.0f, 1.0f);

    [SerializeField, Tooltip("通常時の色")]
    Vector4 normalColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    [SerializeField, Tooltip("Sickleのプレハブ")]
    Object@ sickle;

    [SerializeField, Tooltip("1回の攻撃で生成するSickleの数")]
    int sickleCount = 6;

    [SerializeField, Tooltip("Sickleをプレイヤーの周囲どれくらいの距離に発生させるか")]
    float sickleSpawnRadius = 80.0f;

    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    [SerializeField, Tooltip("死亡エフェクト")]
    Object@ deathEffect;

    [SerializeField, Tooltip("死亡エフェクトの発生数")]
    int deathEffectCount = 5;

    [SerializeField, Tooltip("死亡エフェクトの散らばる範囲(半径)")]
    float deathEffectSpread = 50.0f;

    [SerializeField, Tooltip("死亡エフェクトの生成間隔(秒)")]
    float deathEffectSpawnInterval = 0.15f;

    int spawnedEffectCount = 0;
    float deathEffectTimer = 0.0f;
    bool isAnimation = false;
    float attackTimer = 0.0f;
    array<Object@> cloneEffects;

    // 振り子移動管理用
    float swingTimer = 0.0f;
    float currentCenterAngle = 0.0f;
    bool centerAngleInitialized = false;
    float areaAngleStep = 2.39996f;
    Vector3 currentAnchorPos;

    // カメラ範囲内に留まっている時間の管理用(復帰後の行動猶予)
    float cameraInViewTimer = 0.0f;

    // おまけの上下揺れ管理用
    float floatTimer = 0.0f;

    // ダメージ演出管理用
    float damageFlashTimer = 0.0f;
    bool isFlashing = false;

    // 各種コンポーネント
    SpriteRenderer@ sprite;

    void Start() {
        GetComponent(@sprite);

        if (sprite !is null) {
            sprite.SetInstanceColor(normalColor);
        }
    }

    void Update() {
        // 会話中(isDialogueActive)は移動・攻撃などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

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

        // HPチェック
        if (hp <= 0.0f) {
            // 死亡時のエフェクト処理
            if(!isAnimation){
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
                    Vector3 centerPos = (tf !is null) ? tf.GetTranslate() : Vector3();

                    Object@ clone = GetScene().CloneObject(deathEffect, "CloneDeathEffect_" + spawnedEffectCount);
                    if(clone !is null){
                        Transform@ cloneTf = clone.GetTransform();
                        if(cloneTf !is null){
                            cloneTf.SetScale(Vector3(16.0f, 16.0f, 1.0f));
                        }

                        ScriptComponent@ sc;
                        if(clone.GetComponent(@sc)){
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
            if(cloneEffects.length() > 0){
                float deathEffectDuration = 0.6f;
                
                for(uint i = 0; i < cloneEffects.length(); i++){
                    Object@ clone = cloneEffects[i];
                    if(clone !is null && clone.IsActive()){
                        ScriptComponent@ sc;
                        if(clone.GetComponent(@sc)){
                            sc.CallMethod("UpdateAnimation");
                        }

                        // 各エフェクトが生成されてからの経過時間
                        float effectAge = deathEffectTimer - (float(i) * deathEffectSpawnInterval);

                        // アニメーションが終了したら非アクティブ化
                        if(effectAge >= deathEffectDuration){
                            clone.SetActive(false);
                        } else {
                            allEffectsFinished = false; // まだ終わっていないエフェクトがある
                        }
                    }
                }
            } else {
                allEffectsFinished = false; // まだ1つも生成されていない
            }

            // 全てのエフェクトが生成され、かつ全て終了した場合にボスの姿を消す
            if (spawnedEffectCount >= deathEffectCount && allEffectsFinished) {
                GetOwnerObject().SetComponentsActiveExceptTransformAndScript(false);
            }
        } else {
            Transform@ tf = GetTransform();
            if (tf !is null && player !is null) {
                Transform@ playerTf = player.GetTransform();
                if (playerTf !is null) {
                    if (!centerAngleInitialized) {
                        InitPendulum(tf, playerTf);
                    }

                    // カメラの表示範囲を取得し、範囲外(+余白)にいる場合は移動・攻撃処理を行わない
                    Vector3 currentPos = tf.GetTranslate();
                    Vector3 camPos;
                    Vector2 camSize;
                    bool hasCameraInfo = GetScene().GetVariable("gameplayCameraPos", camPos) && GetScene().GetVariable("gameplayCameraSize", camSize);
                    bool inCameraView = true;
                    if (hasCameraInfo) {
                        inCameraView = (currentPos.x >= camPos.x - cameraActivationMargin && currentPos.x <= camPos.x + camSize.x + cameraActivationMargin &&
                                        currentPos.y >= camPos.y - cameraActivationMargin && currentPos.y <= camPos.y + camSize.y + cameraActivationMargin);
                    }

                    // カメラ範囲内に留まっている時間を計測し、猶予時間(cameraActivationDelay)を
                    // 経過してから移動・攻撃を開始する(範囲外に出たら計測をリセット)
                    if (inCameraView) {
                        cameraInViewTimer += GetDeltaTime();
                    } else {
                        cameraInViewTimer = 0.0f;
                    }
                    bool canAct = inCameraView && cameraInViewTimer >= cameraActivationDelay;

                    if (canAct) {
                        Vector3 playerPos = playerTf.GetTranslate();

                        // 振り子運動
                        swingTimer += GetDeltaTime();
                        float swingAngle = currentCenterAngle + Sin(swingTimer * pendulumSpeed) * pendulumSwingRange;

                        // 上下方向の揺れ
                        floatTimer += GetDeltaTime();
                        float bob = Sin(floatTimer * floatSpeed) * floatAmplitude;

                        // 常にplayerPosを追従するのではなく、固定されたcurrentAnchorPosを基準に計算する
                        Vector3 pos = Vector3(
                            currentAnchorPos.x + Cos(swingAngle) * pendulumRadius,
                            currentAnchorPos.y + Sin(swingAngle) * pendulumRadius + bob,
                            currentAnchorPos.z
                        );

                        // Boss2がプレイヤーより下に行かないように制限する
                        if (pos.y < playerPos.y) {
                            pos.y = playerPos.y;
                        }

                        // ワープ(エリア切替)時などにプレイヤーと重ならないよう、
                        // 「プレイヤーより下に行かない」制約は保ったまま、横方向で最低距離(minDistanceFromPlayer)を確保する
                        float dx = pos.x - playerPos.x;
                        float dy = pos.y - playerPos.y; // 上のクランプにより 0 以上
                        if (dy < minDistanceFromPlayer) {
                            float requiredDxSq = minDistanceFromPlayer * minDistanceFromPlayer - dy * dy;
                            if (requiredDxSq > 0.0f) {
                                float requiredDx = Sqrt(requiredDxSq);
                                float sign = 1.0f;
                                if (dx < -0.0001f) {
                                    sign = -1.0f;
                                } else if (dx > -0.0001f && dx < 0.0001f) {
                                    // 真上に重なっている場合は振り子の向いている方向を使う
                                    sign = (Cos(swingAngle) >= 0.0f) ? 1.0f : -1.0f;
                                }
                                pos.x = playerPos.x + sign * requiredDx;
                            }
                        }

                        // Boss2がカメラの外に出ないよう、表示範囲内(余白を残して)にクランプする
                        if (hasCameraInfo) {
                            float minX = camPos.x + cameraClampMargin;
                            float maxX = camPos.x + camSize.x - cameraClampMargin;
                            float minY = camPos.y + cameraClampMargin;
                            float maxY = camPos.y + camSize.y - cameraClampMargin;

                            if (minX <= maxX) {
                                if (pos.x < minX) pos.x = minX;
                                if (pos.x > maxX) pos.x = maxX;
                            }
                            if (minY <= maxY) {
                                if (pos.y < minY) pos.y = minY;
                                if (pos.y > maxY) pos.y = maxY;
                            }
                        }

                        tf.SetTranslate(pos);

                        // プレイヤーの方向を向く
                        if (playerPos.x > pos.x) {
                            tf.SetRotate(Vector3(0.0f, 3.14159f, 0.0f));
                        } else {
                            tf.SetRotate(Vector3(0.0f, 0.0f, 0.0f));
                        }

                        // 攻撃処理
                        attackTimer += GetDeltaTime();
                        if (attackTimer >= attackWindup) {
                            SpawnSickles();
                            attackTimer = 0.0f;
                        }
                    }
                }
            }
        }
    }

    // 現在位置から振り子の中心角を逆算して初期化する
    void InitPendulum(Transform@ tf, Transform@ playerTf) {
        Vector3 selfPos = tf.GetTranslate();
        Vector3 playerPos = playerTf.GetTranslate();
        
        currentAnchorPos = playerPos; // 初期化時に振り子の基準座標を設定

        float dx = selfPos.x - playerPos.x;
        float dy = selfPos.y - playerPos.y;

        if (dx > 0.0001f || dx < -0.0001f || dy > 0.0001f || dy < -0.0001f) {
            currentCenterAngle = Atan2(dy, dx);
        } else {
            currentCenterAngle = 1.5708f; // 重なっている場合はプレイヤーの真上を初期値にする
        }
        centerAngleInitialized = true;
    }

    // 次のエリアを決めて、そこで振り子運動を再開する(ダメージを受けた時のみ呼ばれる)
    void ChooseNextArea() {
        currentCenterAngle += areaAngleStep;
        swingTimer = 0.0f; // 中心角の位置から振り子をやり直す

        // エリア変更時に基準座標を現在のプレイヤー位置に更新する
        if (player !is null) {
            Transform@ playerTf = player.GetTransform();
            if (playerTf !is null) {
                currentAnchorPos = playerTf.GetTranslate();
            }
        }
    }

    // プレイヤーの周囲にSickleを複数生成する
    void SpawnSickles() {
        if (sickle is null || player is null || sickleCount <= 0) return;

        Transform@ playerTf = player.GetTransform();
        if (playerTf is null) return;

        Vector3 playerPos = playerTf.GetTranslate();
        float angleStep = 6.28318f / float(sickleCount);

        for (int i = 0; i < sickleCount; i++) {
            Object@ clone = GetScene().CloneObject(sickle, "CloneSickle");
            if (clone is null) continue;

            clone.SetActive(true);

            float angle = angleStep * float(i);
            Vector3 spawnPos = Vector3(
                playerPos.x + Cos(angle) * sickleSpawnRadius,
                playerPos.y + Sin(angle) * sickleSpawnRadius,
                playerPos.z
            );

            Transform@ cloneTf = clone.GetTransform();
            if (cloneTf !is null) {
                cloneTf.SetTranslate(spawnPos);
            }
        }
    }

    void Damage(float amount) {
        if (hp <= 0.0f) return;
        hp -= amount;

        // ダメージを受けたら次のエリアへ切り替える
        ChooseNextArea();

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