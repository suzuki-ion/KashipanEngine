class Sword : ScriptComponentBehavior {
    [SerializeField, Tooltip("攻撃力")] int power = 10;
    [SerializeField, Tooltip("クールダウン(秒)")] float cooldown = 0.4f;

    float cooldownTimer = 0.0f;

    void Start(){
    }

    void Update() {
        if (cooldownTimer > 0.0f) {
            cooldownTimer -= GetDeltaTime();
        }
    }

    void Attack(Vector2 dir) {
        if (cooldownTimer > 0.0f) return;
        cooldownTimer = cooldown;

        Log("剣で攻撃");
    }

    void End() {
    }
}
