class Breakable : ScriptComponentBehavior {
    [SerializeField, Tooltip("耐久力(HP)")]
    float hp = 1.0f;

    [SerializeField, Tooltip("破壊時に生成するエフェクト(未設定可)")]
    Object@ deathEffect;

    [SerializeField, Tooltip("破壊してから完全に消滅するまでの遅延(秒)")]
    float destroyDelay = 0.0f;

    bool isBroken = false;
    float destroyTimer = 0.0f;

    void Start() {
    }

    void Update() {
        if (!isBroken) return;

        // 破壊後、演出のための猶予時間を待ってから消滅させる
        destroyTimer += GetDeltaTime();
        if (destroyTimer >= destroyDelay) {
            Object@ obj = GetOwnerObject();
            if (obj !is null) obj.SetActive(false);
        }
    }

    void End() {
    }

    // 武器(コライダータグ "Weapon")との接触を検知してダメージを与える
    void OnCollisionEnter(const HitInfo &in hit) {
        if (isBroken) return;
        if (hit.otherCollider.GetTag() != "Weapon") return;

        Object@ weapon = hit.otherObject;
        if (weapon is null) return;

        float damageAmount = 0.0f;
        ScriptComponent@ sc;
        if (weapon.GetComponent(@sc) && sc.GetVariable("damageAmount", damageAmount)) {
            Damage(damageAmount);
        }
    }

    void Damage(float amount) {
        if (isBroken) return;

        hp -= amount;
        if (hp <= 0.0f) {
            Break();
        }
    }

    void Break() {
        if (isBroken) return;
        isBroken = true;

        Transform@ tf = GetTransform();

        if (deathEffect !is null && tf !is null) {
            Object@ clone = GetScene().CloneObject(deathEffect, "CloneBreakEffect");
            if (clone !is null) {
                clone.SetActive(true);

                Transform@ cloneTf = clone.GetTransform();
                if (cloneTf !is null) {
                    cloneTf.SetTranslate(tf.GetTranslate());
                }
            }
        }

        // 見た目と当たり判定を即座に消す(destroyDelay経過後にUpdate()側で完全に非アクティブ化する)
        Object@ obj = GetOwnerObject();
        if (obj !is null) {
            obj.SetComponentsActiveExceptTransformAndScript(false);
        }
    }
}
