class WallEnemy : ScriptComponentBehavior {
    [SerializeField, Tooltip("弾の発射間隔")]
    float shotDuration = 2.0f;

    [SerializeField, Tooltip("弾")]
    Object@ bullet;

    [SerializeField, Tooltip("弾の発射方向(X)。右向きなら1.0、左向きなら-1.0")]
    float shotDirectionX = -1.0f;

    [SerializeField, Tooltip("弾の生存時間(秒)")]
    float bulletLifeTime = 5.0f;

    [SerializeField, Tooltip("弾の発生位置オフセット")]
    Vector3 bulletOffset = Vector3(0.0f, 0.0f, 0.0f);

    [SerializeField, Tooltip("アニメーション再生開始から弾を発射するまでの遅延(秒)。2コマ目が表示されるタイミングに合わせる")]
    float shotDelay = 0.15f;

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

    // 弾の発射タイマー
    float shotTimer = 0.0f;

    // 発射までの遅延タイマー（アニメーションとの同期用）
    float shotDelayTimer = 0.0f;
    bool isShooting = false;

    // 発射した弾のクローン管理用
    array<Object@> bulletClones;
    array<float> bulletTimers;

    // 死亡エフェクトのクローン
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

        // 生存時の無敵時間タイマー更新および点滅処理
        if (isAlive && isInvincible) {
            invincibleTimer += GetDeltaTime();

            // blinkIntervalごとにスプライトの表示・非表示を交互に切替
            bool isVisible = (int(invincibleTimer / blinkInterval) % 2 == 0);
            if (sprite !is null) {
                sprite.SetActive(isVisible);
            }

            // 無敵時間終了
            if (invincibleTimer >= invincibleDuration) {
                invincibleTimer = 0.0f;
                isInvincible = false;

                // 終了時は必ず表示状態に戻す
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
            shotTimer += GetDeltaTime();

            // 発射間隔ごとにアニメーションを頭から再生し、発射までの遅延タイマーを開始する
            if(!isShooting && shotTimer >= shotDuration){
                shotTimer = 0.0f;
                isShooting = true;
                shotDelayTimer = 0.0f;

                // アニメーションを頭から再生
                array<ScriptComponent@>@ animScripts;
                if (GetComponents(@animScripts)) {
                    for (int i = 0; i < animScripts.length(); ++i) {
                        if (animScripts[i].GetTag() == "AnimatorSC") {
                            animScripts[i].CallMethod("PlayRowForce", 0);
                        }
                    }
                }
            }

            // アニメーションが2コマ目に切り替わるタイミングで弾を発射する
            if(isShooting){
                shotDelayTimer += GetDeltaTime();
                if(shotDelayTimer >= shotDelay){
                    isShooting = false;

                    if (bullet !is null) {
                        Vector3 spawnPos = tf.GetTranslate() + bulletOffset;

                        Object@ cloneBullet = GetScene().CloneObject(bullet, "CloneEnemyBullet");
                        if (cloneBullet !is null) {
                            cloneBullet.SetActive(true);

                            Transform@ cloneTf = cloneBullet.GetTransform();
                            if (cloneTf !is null) {
                                cloneTf.SetTranslate(spawnPos);
                            }

                            ScriptComponent@ cloneSc;
                            if (cloneBullet.GetComponent(@cloneSc)) {
                                cloneSc.SetVariable("pos", spawnPos);
                                cloneSc.CallMethod("Attack", shotDirectionX);
                            }

                            bulletClones.insertLast(cloneBullet);
                            bulletTimers.insertLast(0.0f);
                        }
                    }
                }
            }
        }

        if(!isAlive){
            if(!isAnimation){
                // 経験値付与処理
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

                // 自身の描画と当たり判定を無効化
                self.SetComponentsActiveExceptTransformAndScript(false);

                isAnimation = true;
            }

            // アニメーション更新処理
            if(cloneEffect !is null){
                deathEffectTimer += GetDeltaTime();
                float deathEffectDuration = 0.6f;
                ScriptComponent@ sc;
                if(cloneEffect.GetComponent(@sc)){
                    sc.CallMethod("UpdateAnimation");

                    // アニメーションが終了したら非アクティブ化
                    if(deathEffectTimer >= deathEffectDuration){
                        cloneEffect.SetActive(false);
                    }
                }
            }
        }

        // 発射済みの弾の更新および削除処理
        for (uint i = 0; i < bulletClones.length(); ) {
            Object@ clone = bulletClones[i];

            if (clone !is null) {
                bulletTimers[i] += GetDeltaTime();

                // 当たり判定等で非アクティブ化されたか、生存時間を超えた場合
                if (!clone.IsActive() || bulletTimers[i] >= bulletLifeTime) {
                    clone.SetActive(false);
                    bulletClones.removeAt(i);
                    bulletTimers.removeAt(i);
                    continue; // インデックスを進めずに次の要素へ
                }
            } else {
                // nullの場合も配列から除外
                bulletClones.removeAt(i);
                bulletTimers.removeAt(i);
                continue;
            }

            i++;
        }
    }

    // ダメージを受けて無敵状態を開始するメソッド
    void Damage(float amount) {
        if (!isAlive || isInvincible) return;

        hp -= amount;
        isInvincible = true;
        invincibleTimer = 0.0f;
    }

    void End() {
    }
}