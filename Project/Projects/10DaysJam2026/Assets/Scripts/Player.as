enum State {
    Idle,
    Walk,
    Jump
}

enum Direction{
    Right,
    Left
}

class Player : ScriptComponentBehavior {
    [Header("プレイヤーの基本設定")]

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 0.1f;

    [SerializeField, Tooltip("ジャンプ力")]
    float jumpPower = 16.0f;

    [SerializeField, Tooltip("重力")]
    float gravity = 0.2f;

    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("最大HP")]
    int maxHp = 10;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    Vector2 uvStep = Vector2(0.333f, 0.166f);

    RigidBody2D@ rb;
    Box2DCollider@ col;
    SpriteRenderer@ sprite;

    Vector2 velocity;
    bool isJump = false;
    State state = State::Idle;
    Direction lastDirection = Direction::Right;
    bool isInvincible = false;
    float invincibleDuration = 1.0f;
    float invincibleTimer = 0.0f;
    int hp = maxHp;
    float animTimer = 0.0f;
    int currentWeaponIndex = 0;

    void Start() {
        GetComponent(@rb);
        GetComponent(@col);
        GetComponent(@sprite);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 左右移動
        float moveX = GetCommandValue("MoveX");
        velocity.x = moveX * moveSpeed;

        // 移動入力がある時だけ向きを更新
        if (moveX >= 0.01f) {
            lastDirection = Direction::Right;
        } else if (moveX <= -0.01f) {
            lastDirection = Direction::Left;
        }

        // lastDirectionを元に回転を適用
        float rotY = (lastDirection == Direction::Left) ? 3.14159f : 0.0f;
        tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

        // ジャンプ
        if(IsCommandTriggered("Jump") && rb !is null && !isJump){
            velocity.y = jumpPower;
            isJump = true;
        }

        // 重力を加算
        velocity.y -= gravity;

        // RigidBodyのVelocity変更
        rb.SetVelocity(velocity);

        // 状態の判定
        State nextState = state;
        if (isJump) {
            nextState = State::Jump;
        } else if (moveX != 0.0f) {
            nextState = State::Walk;
        } else {
            nextState = State::Idle;
        }

        // 状態が切り替わったらタイマーをリセット
        if (nextState != state) {
            state = nextState;
            animTimer = 0.0f;
        }

        // タイマー更新
        animTimer += GetDeltaTime();

        // UVオフセット値の計算
        Vector2 uvTranslate = Vector2(0.0f, 0.0f);

        switch (state) {
        case State::Idle:
            // 1コマ目
            uvTranslate.x = 0.0f;
            uvTranslate.y = 0.0f;
            break;

        case State::Walk:
            // 3コマのアニメーション
            {
                int frame = int(animTimer / frameInterval) % 3;
                uvTranslate.x = frame * uvStep.x;
                uvTranslate.y = uvStep.y;
            }
            break;

        case State::Jump:
            // 1コマ目
            uvTranslate.x = 0.0f;
            uvTranslate.y = uvStep.y * 2.0f;
            break;

        default:
            break;
        }

        // UVTranslateの適用
        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(uvTranslate);
        }

        // 無敵時間タイマー更新
        if(isInvincible){
            invincibleTimer += GetDeltaTime();
        }

        // 一定時間経過したら
        if(invincibleTimer >= invincibleDuration){
            invincibleTimer = 0.0f;
            isInvincible = false; // 無敵状態解除
        }
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        isJump = false;
        velocity.y = 0.0f;

        // 敵と当たったら
        if(hit.otherCollider.GetTag() == "Enemy" && !isInvincible){
            Damage(1.0f); // HP減算
            isInvincible = true; // 無敵状態に変更
        }

        // 回復アイテムと当たったら
        if(hit.otherCollider.GetTag() == "Heart"){
            Heal(1.0f); // HP増加
            GetScene().DeleteObject(GetScene().GetObject("Heart"));
        }
    }

    void OnCollidionStay(const HitInfo &in hit){
        velocity.y = 0.0f;
    }

    void Damage(int amount) {
        hp = int(Clamp(float(hp - amount), 0.0f, float(maxHp)));
        Log("Damage! HP:" + hp);
    }

    void Heal(int amount) {
        hp = int(Clamp(float(hp + amount), 0.0f, float(maxHp)));
        Log("Heal! HP:" + hp);
    }

    void End() {
        Log("Player End");
    }
}