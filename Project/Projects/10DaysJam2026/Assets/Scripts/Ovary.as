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

    float hp = 0.0f;
    bool isDead = false;
    float deathTimer = 0.0f;

    // ダメージ演出管理用
    float damageFlashTimer = 0.0f;
    bool isFlashing = false;

    SpriteRenderer@ sprite;

    void Start() {
        GetComponent(@sprite);

        hp = maxHP;
        if (sprite !is null) {
            sprite.SetInstanceColor(normalColor);
        }

        SetAnimation(false);
        NotifyBoss();
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
