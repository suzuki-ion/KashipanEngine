class Ovary : ScriptComponentBehavior {
    [SerializeField, Tooltip("最大HP")]
    float maxHP = 20.0f;

    [SerializeField, Tooltip("左側の弱点かどうか")]
    bool isLeftOvary = true;

    [SerializeField, Tooltip("親であるBoss3のオブジェクト")]
    Object@ boss;

    [SerializeField, Tooltip("ダメージを受けた際の色変化時間(秒)")]
    float damageFlashDuration = 0.1f;

    [SerializeField, Tooltip("ダメージ時の色")]
    Vector4 damageColor = Vector4(10.0f, 10.0f, 10.0f, 1.0f);

    [SerializeField, Tooltip("通常時の色")]
    Vector4 normalColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    [SerializeField, Tooltip("HP0になってから消滅するまでの時間(秒)")]
    float deathDelay = 0.3f;

    [SerializeField, Tooltip("上下に動く幅(振幅)")]
    float moveAmplitude = 15.0f;

    [SerializeField, Tooltip("上下に動く速さ")]
    float moveSpeed = 2.0f;

    float hp = 0.0f;
    bool isDead = false;
    float deathTimer = 0.0f;

    // ダメージ演出管理用
    float damageFlashTimer = 0.0f;
    bool isFlashing = false;

    // 上下移動用
    float initialY = 0.0f;
    float moveTimer = 0.0f;

    // との相対位置保持用
    Vector3 offsetPos;
    bool isOffsetInitialized = false;

    SpriteRenderer@ sprite;

    void Start() {
        GetComponent(@sprite);

        hp = maxHP;
        if (sprite !is null) {
            sprite.SetInstanceColor(normalColor);
        }

        // 初期高さを記憶しておく
        Transform@ tf = GetTransform();
        if (tf !is null) {
            initialY = tf.GetTranslate().y;
        }

        SetAnimation(false);
        NotifyBoss();
    }

    void Update() {
        // 会話中(isDialogueActive)は処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        // Boss3の位置に追従する処理
        if (boss !is null) {
            Transform@ bossTf = boss.GetTransform();
            Transform@ tf = GetTransform();
            if (bossTf !is null && tf !is null) {
                if (!isOffsetInitialized) {
                    // 初期配置におけるBoss3との相対オフセットを記録
                    offsetPos = tf.GetTranslate() - bossTf.GetTranslate();
                    isOffsetInitialized = true;
                }
                // Boss3の現在位置に合わせて自身の座標を更新
                tf.SetTranslate(bossTf.GetTranslate() + offsetPos);
            }
        }

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

        if (isDead) {
            deathTimer += GetDeltaTime();
            if (deathTimer >= deathDelay) {
                Object@ obj = GetOwnerObject();
                if (obj !is null) obj.SetActive(false);
            }
        }
    }

    // プレイヤーの攻撃などから呼び出されるダメージ処理
    void Damage(float amount) {
        if (isDead) return;

        hp -= amount;
        if (hp < 0.0f) hp = 0.0f;

        // ダメージ演出の開始
        isFlashing = true;
        damageFlashTimer = damageFlashDuration;
        if (sprite !is null) {
            sprite.SetInstanceColor(damageColor);
        }

        NotifyBoss();

        if (hp <= 0.0f) {
            isDead = true;
            deathTimer = 0.0f;
            SetAnimation(true);
        }
    }

    // Boss3へ現在HPを通知する
    void NotifyBoss() {
        if (boss is null) return;

        ScriptComponent@ sc;
        if (boss.GetComponent(@sc)) {
            if (isLeftOvary) {
                sc.CallMethod("OnLeftOvaryHPChanged", hp);
            } else {
                sc.CallMethod("OnRightOvaryHPChanged", hp);
            }
        }
    }

    // SpriteAnimatorを使ったアニメーション切り替え
    void SetAnimation(bool dead) {
        array<ScriptComponent@>@ scripts;
        if (GetComponents(@scripts)) {
            for (int i = 0; i < scripts.length(); ++i) {
                if (scripts[i].GetTag() == "AnimatorSC") {
                    if (dead) {
                        scripts[i].CallMethod("PlayRow", 1);
                        scripts[i].CallMethod("SetFrameCount", 1);
                    } else {
                        scripts[i].CallMethod("PlayRow", 0);
                        scripts[i].CallMethod("SetFrameCount", 2);
                    }
                }
            }
        }
    }

    void End() {
    }
}