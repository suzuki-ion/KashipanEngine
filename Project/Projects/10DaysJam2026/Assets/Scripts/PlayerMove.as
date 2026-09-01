class PlayerMove : ScriptComponentBehavior {
    [Header("プレイヤーの基本設定")]

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 0.1f;

    [SerializeField, Tooltip("ジャンプ力")]
    float jumpPower = 16.0f;

    [SerializeField, Tooltip("重力")]
    float gravity = 0.2f;

    Velocity@ velocity;

    void Start() {
        GetComponent(@velocity);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 左右移動
        float moveX = GetCommandValue("MoveX");
        Vector3 pos = tf.GetTranslate();
        pos.x += moveX * moveSpeed * GetDeltaTime();

        // Translateに適用
        tf.SetTranslate(pos);

        // ジャンプ
        if(IsCommandTriggered("Jump") && velocity !is null){
            velocity.AddVelocity(Vector3(0.0f, jumpPower, 0.0f));
            Log("Jump");
        }
    }

    void End() {
        Log("Player End");
    }
}
