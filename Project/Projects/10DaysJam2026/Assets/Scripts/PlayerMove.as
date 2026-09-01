class PlayerMove : ScriptComponentBehavior {
    [Header("プレイヤーの基本設定")]

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 0.1f;

    [SerializeField, Tooltip("ジャンプ力")]
    float jumpPower = 16.0f;

    [SerializeField, Tooltip("重力")]
    float gravity = 0.2f;

    RigidBody2D@ rb;
    Box2DCollider@ col;
    TextureSource@ tex;

    Vector2 velocity;
    bool isJump = false;

    void Start() {
        GetComponent(@rb);
        GetComponent(@col);
        GetComponent(@tex);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 左右移動
        float moveX = GetCommandValue("MoveX");
        velocity.x = moveX * moveSpeed;

        // ジャンプ
        if(IsCommandTriggered("Jump") && rb !is null && !isJump){
            velocity.y = jumpPower;
            isJump = true;
        }

        // 重力を加算
        velocity.y -= gravity;

        // RigidBodyのVelocity変更
        rb.SetVelocity(velocity);
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        isJump = false;
    }

    void OnCollidionStay(const HitInfo &in hit){
        velocity.y = 0.0f;
        rb.SetUseGravity(false);
    }

    void OnCollidionExit(const HitInfo &in hit){
        rb.SetUseGravity(true);
    }

    void End() {
        Log("Player End");
    }
}
