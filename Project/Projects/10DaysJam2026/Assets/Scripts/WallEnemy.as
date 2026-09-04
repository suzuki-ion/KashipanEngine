class WallEnemy : ScriptComponentBehavior {
    [SerializeField, Tooltip("弾の発射間隔")]
    float shotDuration = 2.0f;

    [SerializeField, Tooltip("弾")]
    Object@ bullet;

    // 弾の発射タイマー
    float shotTimer = 0.0f;

    void Start() {
    }

    void Update() {
        shotTimer += GetDeltaTime();

        // 弾の発射
        if(shotTimer >= shotDuration){
            shotTimer = 0.0f;
        }
    }

    void End() {
    }
}
