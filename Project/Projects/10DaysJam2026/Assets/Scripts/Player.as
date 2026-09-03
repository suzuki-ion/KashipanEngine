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

    [SerializeField, Tooltip("接地判定に使わない足元左右端の割合(0～0.5)。壁コライダーの継ぎ目を床として拾うことを防ぐ")]
    float groundedFootInsetRatio = 0.5f;

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
            // 法線が上向きでも、足元中央が相手の上に乗っていなければ壁の継ぎ目による
            // 見かけ上の床接触として扱う
            bool hasGroundSupport = HasGroundSupport(hit);

            // めり込み分を押し戻す
            ResolvePenetration(hit, hasGroundSupport);

            // 床との接触かつ上昇中でない時だけ着地扱いにする
            if (hit.normal.y > groundedThreshold && hasGroundSupport && velocity.y <= 0.0f) {
                isJump = false;
                velocity.y = 0.0f;
            }
        }
    }

    // hit.normal（自分を押し出す方向） * hit.penetration（めり込み量）だけTransformを移動させ、
    // 他コライダーとのめり込みを解消する。
    // 床・天井方向（法線が縦寄り）の接触だけpushBackCorrectionFactorで補正量を弱め、複数フレームに
    // 分けて収束させる（床と天井に同時に挟まれた際の上下振動を和らげるため）。
    // 壁方向（法線が横寄り）の接触は毎フレーム全量で押し戻す。ここを弱めてしまうと、移動入力で
    // 壁へ押し込み続けている間はめり込みが数ピクセル分残り続けてしまい、同じ幅の壁コライダーが
    // 縦に並んでいる継ぎ目にプレイヤーの矩形が両方浅くかぶることで、片方の判定が横ではなく
    // 上向きの法線を返してしまい、着地扱いされて引っかかってしまう
    void ResolvePenetration(const HitInfo &in hit, bool hasGroundSupport) {
        if(hit.penetration <= 0.0f) return;
        if(IsPushBackExcluded(hit)) return;

        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 上向き法線でも足元中央に支持面が無い場合は、上下に並ぶ壁コライダーの継ぎ目へ
        // 横から入り込んだ接触。上へ押すと一時的な着地になるため、矩形同士の横方向の
        // めり込み量を使って壁の外へ全量押し戻す
        if(hit.normal.y > groundedThreshold && !hasGroundSupport && ResolveUnsupportedGroundAsWall(hit, tf)) {
            return;
        }

        bool isVerticalContact = Abs(hit.normal.y) > groundedThreshold;
        float factor = isVerticalContact ? pushBackCorrectionFactor : 1.0f;
        tf.SetTranslate(tf.GetTranslate() + hit.normal * (hit.penetration * factor));
    }

    // 足元の中央寄りの範囲が相手のBox2Dコライダー上に存在するかを調べる。
    // 接触相手がBox2D以外の場合は、従来どおり法線だけで接地判定する。
    bool HasGroundSupport(const HitInfo &in hit) const {
        if(hit.normal.y <= groundedThreshold) return true;
        if(col is null || hit.otherCollider is null) return true;

        Box2DCollider@ otherBox = cast<Box2DCollider>(hit.otherCollider);
        if(otherBox is null) return true;

        // 横幅から支持範囲を求められる、X軸に平行な矩形だけを追加判定の対象にする。
        // 傾斜した矩形は法線どおりに処理し、斜面での従来挙動を変えない。
        if(Abs(Sin(col.GetSyncedOwnerRotationEuler().z)) > 0.001f
                || Abs(Sin(otherBox.GetSyncedOwnerRotationEuler().z)) > 0.001f) {
            return true;
        }

        float playerCenterX;
        float playerHalfWidth;
        GetBoxHorizontalBounds(col, playerCenterX, playerHalfWidth);

        float otherCenterX;
        float otherHalfWidth;
        GetBoxHorizontalBounds(otherBox, otherCenterX, otherHalfWidth);

        float insetRatio = Clamp(groundedFootInsetRatio, 0.0f, 0.5f);
        float footHalfWidth = playerHalfWidth * (1.0f - insetRatio * 2.0f);
        float otherLeft = otherCenterX - otherHalfWidth;
        float otherRight = otherCenterX + otherHalfWidth;

        // 0.5では足元中央の1点を接地プローブとして扱う。横移動量やフレーム時間が
        // 大きくなって壁へ深く入った場合にも、壁端の重なりを床と誤認しない。
        if(footHalfWidth <= 0.001f) {
            return playerCenterX > otherLeft + 0.001f && playerCenterX < otherRight - 0.001f;
        }

        float overlap = Min(playerCenterX + footHalfWidth, otherCenterX + otherHalfWidth)
                      - Max(playerCenterX - footHalfWidth, otherLeft);
        return overlap > 0.001f;
    }

    // 足元の支持が無い上向き接触を、Box2D同士の横方向の押し戻しとして解決する。
    bool ResolveUnsupportedGroundAsWall(const HitInfo &in hit, Transform@ tf) const {
        if(col is null || hit.otherCollider is null) return false;

        Box2DCollider@ otherBox = cast<Box2DCollider>(hit.otherCollider);
        if(otherBox is null) return false;

        float playerCenterX;
        float playerHalfWidth;
        GetBoxHorizontalBounds(col, playerCenterX, playerHalfWidth);

        float otherCenterX;
        float otherHalfWidth;
        GetBoxHorizontalBounds(otherBox, otherCenterX, otherHalfWidth);

        float horizontalPenetration = playerHalfWidth + otherHalfWidth - Abs(playerCenterX - otherCenterX);
        if(horizontalPenetration <= 0.0f) return false;

        float pushDirectionX = playerCenterX < otherCenterX ? -1.0f : 1.0f;
        tf.SetTranslate(tf.GetTranslate() + Vector3(pushDirectionX * horizontalPenetration, 0.0f, 0.0f));
        return true;
    }

    // 回転したBox2Dにも対応できるよう、矩形をX軸へ投影した中心と半幅を返す。
    void GetBoxHorizontalBounds(Box2DCollider@ box, float &out centerX, float &out halfWidth) const {
        Vector3 ownerPosition = box.GetSyncedOwnerPosition();
        Vector2 rotatedCenter = box.RotateOffsetBySyncedRotation2D(box.GetCenter());
        Vector3 scale = box.GetSyncedOwnerScale();
        Vector2 size = box.GetSize();
        float angle = box.GetSyncedOwnerRotationEuler().z;
        float halfSizeX = Abs(size.x * scale.x) * 0.5f;
        float halfSizeY = Abs(size.y * scale.y) * 0.5f;

        centerX = ownerPosition.x + rotatedCenter.x;
        halfWidth = Abs(Cos(angle)) * halfSizeX + Abs(Sin(angle)) * halfSizeY;
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
