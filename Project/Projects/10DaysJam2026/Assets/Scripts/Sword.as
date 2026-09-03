class Sword : ScriptComponentBehavior {
    [SerializeField, Tooltip("攻撃力")] int power = 10;
    [SerializeField, Tooltip("クールダウン(秒)")] float cooldown = 0.4f;
    [SerializeField, Tooltip("座標")] Vector3 pos;
    [SerializeDield, ToolTip("ダメージ値")] float damageAmount = 1.0f;

    float cooldownTimer = 0.0f;
    Box2DCollider@ col;
    bool isActive = false;
    float activeDuration = 0.1f;
    float activeTimer = 0.0f;

    void Start(){
        GetComponent(@col);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        if (cooldownTimer > 0.0f) {
            cooldownTimer -= GetDeltaTime();
        }

        tf.SetTranslate(pos);

        // アクティブ時間の制御
        if(isActive){
            activeTimer += GetDeltaTime();
            if(activeTimer >= activeDuration){
                activeTimer = 0.0f;
                isActive = false;
                col.SetTrigger(true);
                col.SetCenter(Vector2(0.0f, 0.0f));
            }
        }
    }

    void Attack(float margin) {
        if (cooldownTimer > 0.0f) return;
        cooldownTimer = cooldown;
        col.SetTrigger(false);
        col.SetCenter(Vector2(margin, 0.0f));
        isActive = true;

        Log("剣で攻撃");
    }

    void End() {
    }

    void OnCollisionEnter(const HitInfo &in hit){
        if(hit.otherCollider.GetTag() == "Enemy"){
            Object@ enemy = hit.otherObject;

            if (enemy !is null) {
                ScriptComponent@ sc;
                if (enemy.GetComponent(@sc)) {
                    float hp;
                    if (sc.GetVariable("hp", hp)) {
                        Log("敵のHP: " + hp);
                        sc.SetVariable("hp", Clamp(hp - damageAmount, 0.0f, 100.0f));
                    }
                }
            }
        }
    }
}
