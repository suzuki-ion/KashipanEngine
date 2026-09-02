enum State {
    Idle,
    Walk,
    Jump,
    Attack,
    WalkAttack,
    JumpAttack
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

    [SerializeField, Tooltip("攻撃モーション表示時間(秒)")]
    float attackDuration = 0.45f;

    [SerializeField, Tooltip("最大HP")]
    float maxHp = 10.0f;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    Vector2 uvStep = Vector2(0.333f, 0.166f);

    [SerializeField, Tooltip("武器一覧")]
    array<Object@>@ weapons;

    [SerializeField, Tooltip("エフェクト")]
    Object@ effect;

    [SerializeField, Tooltip("エフェクトの発生時間")]
    float effectActiveDuration = 0.1f;

    // エフェクトの発生タイマー
    float effectActiveTimer = 0.0f;

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
    float hp = maxHp;
    float animTimer = 0.0f;
    int currentWeaponIndex = 0;
    
    // 攻撃用タイマー
    float attackTimer = 0.0f;

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
        velocity.x = moveX * moveSpeed * GetDeltaTime();

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
            velocity.y = jumpPower * GetDeltaTime();
            isJump = true;
        }

        // 重力を加算
        velocity.y -= gravity * GetDeltaTime();

        // RigidBodyのVelocity変更
        rb.SetVelocity(velocity);

        // 攻撃入力の検知とタイマーリセット
        if(IsCommandTriggered("Attack")){
            attackTimer = attackDuration;
            float margin = 0.0f;

            // 攻撃
            if(weapons[currentWeaponIndex] !is null){
                ScriptComponent@ sc;
                if(weapons[currentWeaponIndex].GetComponent(@sc)){
                    float margin;
                    if(lastDirection == Direction::Right){
                        margin = 16.0f;
                    }else if(lastDirection == Direction::Left){
                        margin = -16.0f;
                    }

                    // 武器の向きに応じて中心点をずらす
                    sc.CallMethod("Attack", margin);
                }
            }

            if(effect !is null){
                ScriptComponent@ effectSc;
                if(effect.GetComponent(@effectSc)){
                    effect.SetActive(true);
                }
            }
        }

        // 武器の座標をプレイヤーに合わせる
        Object@ currentWeapon = weapons[currentWeaponIndex];
        if(currentWeapon !is null){
            ScriptComponent@ sc;
            if(currentWeapon.GetComponent(@sc)){
                Vector3 pos;
                if(sc.GetVariable("pos", pos)){
                    sc.SetVariable("pos", tf.GetTranslate());
                }
            }
        }

        // エフェクトの座標をの近くに移動
        if(effect !is null){
            ScriptComponent@ sc;
            if(effect.GetComponent(@sc)){
                Vector3 pos;
                Vector3 rotate;
                Vector3 finalPos;
                Vector3 effectOffset;

                // 方向に応じてオフセットと回転を変更
                if(lastDirection == Direction::Right){
                    effectOffset = Vector3(16.0f, 0.0f, 0.0f);
                    finalPos = Vector3(0.0f, 0.0f, 0.0f);
                }else if(lastDirection == Direction::Left){
                    effectOffset = Vector3(-16.0f, 0.0f, 0.0f);
                    finalPos = Vector3(0.0f, 3.14f, 0.0f);
                }
                
                if(sc.GetVariable("pos", pos)){
                    sc.SetVariable("pos", tf.GetTranslate() + effectOffset);
                }

                if(sc.GetVariable("rotate", rotate)){
                    sc.SetVariable("rotate", finalPos);
                }
            }
        }

        // 一定時間経過後エフェクトを非アクティブ化
        if(effect.IsActive()){
            effectActiveTimer += GetDeltaTime();
            if(effectActiveTimer >= effectActiveDuration){
                effectActiveTimer = 0.0f;
                effect.SetActive(false);
            }
        }

        // 攻撃中判定の更新
        bool isAttacking = false;
        if(attackTimer > 0.0f){
            attackTimer -= GetDeltaTime();
            isAttacking = true;
        }

        // 状態の判定
        State nextState = state;
        if (isAttacking) {
            if (isJump) {
                nextState = State::JumpAttack;
            } else if (moveX != 0.0f) {
                nextState = State::WalkAttack;
            } else {
                nextState = State::Attack;
            }
        } else {
            if (isJump) {
                nextState = State::Jump;
            } else if (moveX != 0.0f) {
                nextState = State::Walk;
            } else {
                nextState = State::Idle;
            }
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
            // 3コマ
            {
                int frame = int(animTimer / frameInterval) % 3;
                uvTranslate.x = frame * uvStep.x;
                uvTranslate.y = uvStep.y * 1.0f;
            }
            break;

        case State::Jump:
            // 1コマ目
            uvTranslate.x = 0.0f;
            uvTranslate.y = uvStep.y * 2.0f;
            break;

        case State::Attack:
            // 1コマ目
            uvTranslate.x = 0.0f;
            uvTranslate.y = uvStep.y * 3.0f;
            break;

        case State::WalkAttack:
            // 3コマ
            {
                int frame = int(animTimer / frameInterval) % 3;
                uvTranslate.x = frame * uvStep.x;
                uvTranslate.y = uvStep.y * 4.0f;
            }
            break;

        case State::JumpAttack:
            // 1コマ目
            uvTranslate.x = 0.0f;
            uvTranslate.y = uvStep.y * 5.0f;
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

    void Damage(float amount) {
        hp = Clamp(hp - amount, 0.0f, maxHp);
        Log("Damage! HP:" + hp);
    }

    void Heal(float amount) {
        hp = Clamp(hp + amount, 0.0f, maxHp);
        Log("Heal! HP:" + hp);
    }

    void End() {
        Log("Player End");
    }
}