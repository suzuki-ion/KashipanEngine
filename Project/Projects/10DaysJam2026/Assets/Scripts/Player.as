#include "WeaponList.as"

enum State {
    Idle,
    Walk,
    Jump,
    Attack,
    WalkAttack,
    JumpAttack,
    Dead
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

    [SerializeField, Tooltip("エフェクトの発生時間")]
    float effectActiveDuration = 0.1f;

    [SerializeField, Tooltip("1回の押し戻しで実際に補正する割合(0～1)。1未満にすると複数フレームに分けて収束させ、挟まれた際の上下振動を和らげる")]
    float pushBackCorrectionFactor = 0.3f;

    [SerializeField, Tooltip("手裏剣の生存時間(秒)")]
    float syurikenLifeTime = 2.0f;

    [SerializeField, Tooltip("剣の攻撃間隔(秒)")]
    float swordAttackInterval = 0.4f;

    [SerializeField, Tooltip("手裏剣の発射間隔(秒)")]
    float syurikenAttackInterval = 0.2f;

    [SerializeField, Tooltip("ヴィネットをかけるHP値")]
    float vignetteHp = 3.0f;

    [SerializeField, Tooltip("点滅の切り替え間隔(秒)")]
    float blinkInterval = 0.08f;

    [SerializeField, Tooltip("武器一覧")]
    array<Object@>@ weapons;

    [SerializeField, Tooltip("所持武器")]
    int currentWeaponType = 1;

    [SerializeField, Tooltip("押し戻しを行わない相手のタグ一覧")]
    array<string>@ pushBackExcludeTags;

    [SerializeField, Tooltip("エフェクト")]
    Object@ effect;

    [SerializeField, Tooltip("回復アイテム")]
    Object@ healItem;

    [SerializeField, Tooltip("ゲーム画面")]
    Object@ gameScreen;

    // クールダウン計算用タイマー
    float swordCooldownTimer = 0.0f;
    float syurikenCooldownTimer = 0.0f;

    // 手裏剣クローン管理用
    array<Object@> syurikenClones;
    array<float> syurikenTimers;

    // エフェクトの発生タイマー
    float effectActiveTimer = 0.0f;

    Box2DCollider@ col;
    CharacterController2D@ controller;
    SpriteRenderer@ sprite;

    Vector2 velocity;
    bool isJump = false;
    State state = State::Idle;
    Direction lastDirection = Direction::Right;
    bool isInvincible = false;
    float invincibleDuration = 1.0f;
    float invincibleTimer = 0.0f;
    bool isAlive = true;
    
    // 攻撃用タイマー
    float attackTimer = 0.0f;

    void Start() {
        GetComponent(@col);
        if(GetComponent(@controller)) {
            controller.SetSelectedCollider(col);
            controller.SetGroundedThreshold(groundedThreshold);
            controller.ClearIgnoredTags();
            if(pushBackExcludeTags !is null) {
                for(uint i = 0; i < pushBackExcludeTags.length(); ++i) {
                    controller.AddIgnoredTag(pushBackExcludeTags[i]);
                }
            }
        }
        GetComponent(@sprite);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 前フレーム後半に確定した接触結果から、接触面へ向かう縦速度だけを消す。
        // 壁との接触では縦速度を変更しない。
        if(controller !is null) {
            if(controller.IsGrounded() && velocity.y <= 0.0f) {
                velocity.y = 0.0f;
                isJump = false;
            }
            if(controller.IsTouchingCeiling() && velocity.y > 0.0f) {
                velocity.y = 0.0f;
            }
        }

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

        // 武器の切り替え
        if (weapons !is null && weapons.length() > 0) {
            int weaponCount = int(weapons.length());
            int currentIndex = int(currentWeaponType);
        
            if (IsCommandTriggered("WeaponChangeRight")) {
                currentWeaponType = WeaponList((currentIndex + 1) % weaponCount);
                Log("WeaponType" + int(currentWeaponType));
            }
            if (IsCommandTriggered("WeaponChangeLeft")) {
                currentWeaponType = WeaponList((currentIndex - 1 + weaponCount) % weaponCount);
                Log("WeaponType" + int(currentWeaponType));
            }
        }

        // 重力を加算（フレームをまたいで蓄積する値なので、ここはGetDeltaTime()を掛ける）
        velocity.y -= gravity * GetDeltaTime();

        // 移動要求は予約し、同フレーム後半にCharacterController2DがX→Y順で解決する。
        Vector2 movement = velocity * GetDeltaTime();
        if(controller !is null) {
            controller.Move(movement);
        } else {
            // CharacterController2Dが無い旧シーン用の互換フォールバック。
            tf.SetTranslate(tf.GetTranslate() + Vector3(movement.x, movement.y, 0.0f));
        }

        // weapons未設定・currentWeaponIndexが範囲外の場合の"Index out of bounds"を防ぐ
        bool hasWeapon = currentWeaponType >= 0 && uint(currentWeaponType) < weapons.length();

        // 攻撃クールダウンタイマーの更新
        if (swordCooldownTimer > 0.0f) {
            swordCooldownTimer -= GetDeltaTime();
        }
        if (syurikenCooldownTimer > 0.0f) {
            syurikenCooldownTimer -= GetDeltaTime();
        }

        // 攻撃入力の検知とタイマーリセット
        if (IsCommandTriggered("Attack")) {
            // 現在選択中の武器に応じた攻撃の可否判定
            bool canAttack = false;
            if (currentWeaponType == WeaponList::Sword && swordCooldownTimer <= 0.0f) {
                canAttack = true;
                swordCooldownTimer = swordAttackInterval; // 剣のクールダウンリセット
            } else if (currentWeaponType == WeaponList::Shuriken && syurikenCooldownTimer <= 0.0f) {
                canAttack = true;
                syurikenCooldownTimer = syurikenAttackInterval; // 手裏剣のクールダウンリセット
            }

            // 攻撃実行処理
            if (canAttack) {
                attackTimer = attackDuration;

                // 攻撃オブジェクトの呼び出し
                if (hasWeapon && weapons[currentWeaponType] !is null) {
                    ScriptComponent@ sc;
                    if (weapons[currentWeaponType].GetComponent(@sc)) {
                        float margin;
                        float syurikenMoveX;
                        Vector3 pos;
                        if (lastDirection == Direction::Right) {
                            margin = 16.0f;
                            syurikenMoveX = 1.0f;
                        } else if (lastDirection == Direction::Left) {
                            margin = -16.0f;
                            syurikenMoveX = -1.0f;
                        }

                        switch (currentWeaponType) {
                        case WeaponList::Sword:
                            sc.CallMethod("Attack", margin);
                            break;

                        case WeaponList::Shuriken:
                            {
                                Object@ cloneSyuriken = GetScene().CloneObject(weapons[currentWeaponType], "CloneSyuriken");
                                if (cloneSyuriken !is null) {
                                    cloneSyuriken.SetActive(true);

                                    Transform@ cloneTf = cloneSyuriken.GetTransform();
                                    if (cloneTf !is null) {
                                        cloneTf.SetScale(Vector3(6.0f, 6.0f, 1.0f));
                                        cloneTf.SetTranslate(tf.GetTranslate());
                                    }
                                    
                                    ScriptComponent@ cloneSc;
                                    if (cloneSyuriken.GetComponent(@cloneSc)) {
                                        cloneSc.SetVariable("pos", tf.GetTranslate());
                                        cloneSc.CallMethod("Attack", syurikenMoveX);
                                    }

                                    syurikenClones.insertLast(cloneSyuriken);
                                    syurikenTimers.insertLast(0.0f);
                                }
                            }
                            break;
                        }
                    }
                }

                // エフェクト制御
                switch (currentWeaponType) {
                case WeaponList::Sword:
                    if (effect !is null) {
                        ScriptComponent@ effectSc;
                        if (effect.GetComponent(@effectSc)) {
                            effect.SetActive(true);
                        }
                    }
                    break;

                case WeaponList::Shuriken:
                    break;

                default:
                    break;
                }
            }
        }

        // 武器の座標制御
        uint swordIndex = uint(WeaponList::Sword);
        if (weapons !is null && swordIndex < weapons.length() && weapons[swordIndex] !is null) {
            ScriptComponent@ sc;
            if (weapons[swordIndex].GetComponent(@sc)) {
                Vector3 targetPos;
                
                // 装備中がソードならプレイヤーの位置に、手裏剣などそれ以外なら退避座標へ移動
                if (currentWeaponType == WeaponList::Sword) {
                    targetPos = tf.GetTranslate();
                } else {
                    targetPos = Vector3(-100.0f, 0.0f, 0.0f);
                }

                Vector3 pos;
                if (sc.GetVariable("pos", pos)) {
                    sc.SetVariable("pos", targetPos);
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

        // HPが指定値以下になったらヴィネットをかける
        // if(hp <= vignetteHp){
        //     ScriptComponent@ sc;
        //     if(GetComponent(@sc)){
                
        //     }
        // }

        // HP0になったら死亡
        if(hp <= 0.0f){
            isAlive = false;
        }

        // 状態の判定
        State nextState = state;
        if (!isAlive) {
            nextState = State::Dead;
        }
        else if (isAttacking) {
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

        // 状態が切り替わったらアニメーターに指示を出す
        if (nextState != state) {
            state = nextState;

            ScriptComponent@ animSc;
            if (GetComponent(@animSc)) {
                // Y位置の変更。Stateの数値をそのまま行番号として渡す
                animSc.CallMethod("PlayRow", int(state));

                // X位置の変更。歩きと歩き攻撃のみ3コマ、他は1コマ
                if (state == State::Walk || state == State::WalkAttack) {
                    animSc.CallMethod("SetFrameCount", 3);
                } else {
                    animSc.CallMethod("SetFrameCount", 1);
                }
            }
        }

        // 無敵時間タイマー更新および点滅処理
        if (isInvincible) {
            invincibleTimer += GetDeltaTime();

            // blinkIntervalごとにフラグを交互に切り替える
            bool isVisible = (int(invincibleTimer / blinkInterval) % 2 == 0);
            if (sprite !is null) {
                sprite.SetActive(isVisible);
            }

            // 無敵時間終了
            if (invincibleTimer >= invincibleDuration) {
                invincibleTimer = 0.0f;
                isInvincible = false;

                // 終了時は必ず表示状態に戻す
                if (sprite !is null) {
                    sprite.SetActive(true);
                }
            }
        }

        // 一定時間経過したら
        if(invincibleTimer >= invincibleDuration){
            invincibleTimer = 0.0f;
            isInvincible = false; // 無敵状態解除
        }

        // 手裏剣クローンの更新および削除処理
        for (uint i = 0; i < syurikenClones.length(); ) {
            Object@ clone = syurikenClones[i];

            if (clone !is null) {
                // タイマー加算
                syurikenTimers[i] += GetDeltaTime();

                // 当たり判定等で非アクティブ化されたか、生存時間を超えた場合
                if (!clone.IsActive() || syurikenTimers[i] >= syurikenLifeTime) {
                    clone.SetActive(false);
                    syurikenClones.removeAt(i);
                    syurikenTimers.removeAt(i);
                    continue; // インデックスを進めずに次の要素へ
                }
            } else {
                // nullの場合も配列から除外
                syurikenClones.removeAt(i);
                syurikenTimers.removeAt(i);
                continue;
            }

            i++;
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
