enum MoveDirection{
    Right, // 右移動
    Left // 左移動
}

class Green : ScriptComponentBehavior {

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 1.0f;

    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    float uvStep = 0.5f;

    // 進行方向
    MoveDirection moveDir = MoveDirection::Left;

    Vector2 velocity;
    float gravity = 0.2f;

    Box2DCollider@ col;
    RigidBody2D@ rb;
    SpriteRenderer@ sprite;

    // アニメーション用タイマー
    float animTimer = 0.0f;

    void Start() {
        GetComponent(@col);
        GetComponent(@rb);
        GetComponent(@sprite);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // moveDirに応じてY軸の回転を設定
        float rotY = (moveDir == MoveDirection::Left) ? 0.0f : 3.14159f;
        tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

        // 左右移動
        float dir = (moveDir == MoveDirection::Left) ? -1.0f : 1.0f;
        velocity.x = dir * moveSpeed;

        // 重力
        velocity.y -= gravity;

        rb.SetVelocity(velocity);

        // アニメーション処理
        animTimer += GetDeltaTime();
        int frame = int(animTimer / frameInterval) % 2;

        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(Vector2(frame * uvStep, 0.0f));
        }
    }

    void End() {
        Log("GoombaEnd");
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        // 進行方向の切り替え
        if(hit.otherObject.GetTag() == "Wall"){
            if (moveDir == MoveDirection::Left) {
                moveDir = MoveDirection::Right;
                Log("Right");
            } else {
                moveDir = MoveDirection::Left;
                Log("Left");
            }
        }
    }

    void OnCollisionStay(const HitInfo &in hit){
        velocity.y = 0.0f;
    }
}