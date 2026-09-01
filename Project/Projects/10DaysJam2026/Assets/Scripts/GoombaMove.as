enum MoveDirection{
    Right, // 右移動
    Left // 左移動
}

class GoombaMove : ScriptComponentBehavior {

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 1.0f;

    // 進行方向
    MoveDirection moveDir = MoveDirection::Left;

    Vector2 velocity;
    float gravity = 0.2f;

    Box2DCollider@ col;
    RigidBody2D@ rb;
    Transform@ transform;

    void Start() {
        GetComponent(@col);
        GetComponent(@rb);
        GetComponent(@transform);
    }

    void Update() {
        transform.SetRotate(Vector3(0.0f, 0.0f, 0.0f));

        // 左右移動
        float dir = (moveDir == MoveDirection::Left) ? -1.0f : 1.0f;
        velocity.x = dir * moveSpeed;

        // 重力
        velocity.y -= gravity;

        rb.SetVelocity(velocity);
    }

    void End() {
        Log("GoombaEnd");
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        // 進行方向の切り替え
        if (moveDir == MoveDirection::Left) {
            moveDir = MoveDirection::Right;
        } else {
            moveDir = MoveDirection::Left;
        }
    }

    void OnCollidionStay(const HitInfo &in hit){
        velocity.y = 0.0f;
        rb.SetUseGravity(false);
    }

    void OnCollidionExit(const HitInfo &in hit){
        rb.SetUseGravity(true);
    }
}
