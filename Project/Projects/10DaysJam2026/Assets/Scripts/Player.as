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
    float moveSpeed = 66.6667f;

    [SerializeField, Tooltip("ジャンプ力")]
    float jumpPower = 66.6667f;

    [SerializeField, Tooltip("重力")]
    float gravity = 120.0f;

    [SerializeField, Tooltip("接地とみなす法線Y成分のしきい値")]
    float groundedThreshold = 0.5f;

    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("攻撃モーション表示時間(秒)")]
    float attackDuration = 0.45f;

    [SerializeField, Tooltip("最大HP")]
    float maxHp = 10.0f;

    [SerializeField, Tooltip("HP")]
    float hp = maxHp;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    Vector2 uvStep = Vector2(0.333f, 0.166f);

    [SerializeField, Tooltip("武器一覧")]
    array<Object@>@ weapons;

    [SerializeField, Tooltip("エフェクト")]
    Object@ effect;

    [SerializeField, Tooltip("エフェクトの発生時間")]
    float effectActiveDuration = 0.1f;

    [SerializeField, Tooltip("押し戻しを行わない相手のタグ一覧")]
    array<string>@ pushBackExcludeTags;

    [SerializeField, Tooltip("1回の押し戻しで実際に補正する割合(0～1)。1未満にすると複数フレームに分けて収束させ、挟まれた際の上下振動を和らげる")]
    float pushBackCorrectionFactor = 0.3f;

    [SerializeField, Tooltip("回復アイテム")]
    Object@ healItem;

    // エフェクトの発生タイマー
    float effectActiveTimer = 0.0f;

    Box2DCollider@ col;
    SpriteRenderer@ sprite;

    Vector2 velocity;
    bool isJump = false;
    State state = State::Idle;
    Direction lastDirection = Direction::Right;
    bool isInvincible = false;
    float invincibleDuration = 1.0f;
    float invincibleTimer = 0.0f;
    float animTimer = 0.0f;
    int currentWeaponIndex = 0;
    
    // 攻撃用タイマー
    float attackTimer = 0.0f;

    void Start() {
        GetComponent(@col);
        GetComponent(@sprite);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 左右移動
        // velocityは「1秒あたりの移動量」なので、この時点ではGetDeltaTime()を掛けない
        // （掛けるのは下の位置積分の1箇所だけにする）
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
        if(IsCommandTriggered("Jump") && !isJump){
            velocity.y = jumpPower;
            isJump = true;
        }

        // 重力を加算（フレームをまたいで蓄積する値なので、ここはGetDeltaTime()を掛ける）
        velocity.y -= gravity * GetDeltaTime();

        // RigidBodyを使わず、速度を自前でTransformへ積分する
        tf.SetTranslate(tf.GetTranslate() + Vector3(velocity.x, velocity.y, 0.0f) * GetDeltaTime());

        // weapons未設定・currentWeaponIndexが範囲外の場合の"Index out of bounds"を防ぐ
        bool hasWeapon = currentWeaponIndex >= 0 && uint(currentWeaponIndex) < weapons.length();

        // 攻撃入力の検知とタイマーリセット
        if(IsCommandTriggered("Attack")){
            attackTimer = attackDuration;
            float margin = 0.0f;

            // 攻撃
            if(hasWeapon && weapons[currentWeaponIndex] !is null){
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
        Object@ currentWeapon = hasWeapon ? weapons[currentWeaponIndex] : null;
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
        if(effect !is null && effect.IsActive()){
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
        // 押し戻し除外タグの相手とは、物理的な押し戻しだけでなく着地判定も行わずすり抜けさせる
        if(!IsPushBackExcluded(hit)) {
            // めり込み分を押し戻す
            ResolvePenetration(hit);

            // 床（法線が上向き）からの接触で、かつ上昇中でない（velocity.y <= 0）時だけ着地扱いにする。
            // velocity.yの条件が無いと、ジャンプで押し出した直後にまだ床とのめり込みが
            // 解消しきっていない数フレームの間、ここでvelocity.yがすぐ0に戻されてしまい
            // ジャンプが打ち消される（＝ジャンプも落下も起きないように見える）
            if (hit.normal.y > groundedThreshold && velocity.y <= 0.0f) {
                isJump = false;
                velocity.y = 0.0f;
            }
        }

        // 敵と当たったら
        if(hit.otherCollider.GetTag() == "Enemy" && !isInvincible){
            Damage(1.0f); // HP減算
            isInvincible = true; // 無敵状態に変更
        }

        // 回復アイテムと当たったら
        if(hit.otherCollider.GetTag() == "Heart"){
            Heal(1.0f); // HP増加
            healItem.SetActive(false);
        }
    }

    void OnCollisionStay(const HitInfo &in hit){
        // 押し戻し除外タグの相手とは、物理的な押し戻しだけでなく着地判定も行わずすり抜けさせる
        if(!IsPushBackExcluded(hit)) {
            // めり込み分を押し戻す
            ResolvePenetration(hit);

            // OnCollisionEnterと同じく、床との接触かつ上昇中でない時だけ着地扱いにする
            if (hit.normal.y > groundedThreshold && velocity.y <= 0.0f) {
                isJump = false;
                velocity.y = 0.0f;
            }
        }
    }

    // hit.normal（自分を押し出す方向） * hit.penetration（めり込み量） * pushBackCorrectionFactorだけ
    // Transformを移動させ、他コライダーとのめり込みを解消する。
    // 全量を一度に補正しない（pushBackCorrectionFactor < 1）ことで、2つ以上のコライダーへ同時に
    // めり込むような狭い隙間でも、双方の補正が全量でぶつかり合って上下に振動し続けるのを和らげる
    void ResolvePenetration(const HitInfo &in hit) {
        if(hit.penetration <= 0.0f) return;
        if(IsPushBackExcluded(hit)) return;

        Transform@ tf = GetTransform();
        if(tf is null) return;

        tf.SetTranslate(tf.GetTranslate() + hit.normal * (hit.penetration * pushBackCorrectionFactor));
    }

    // pushBackExcludeTagsに衝突相手のタグが含まれているかを調べる
    bool IsPushBackExcluded(const HitInfo &in hit) const {
        if(pushBackExcludeTags is null || hit.otherCollider is null) return false;

        for(uint i = 0; i < pushBackExcludeTags.length(); ++i){
            if(hit.otherCollider.GetTag() == pushBackExcludeTags[i]) return true;
        }
        return false;
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