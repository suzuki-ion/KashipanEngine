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

    [SerializeField, Tooltip("1回の押し戻しで実際に補正する割合")]
    float pushBackCorrectionFactor = 0.3f;

    [SerializeField, Tooltip("手裏剣の生存時間(秒)")]
    float syurikenLifeTime = 2.0f;

    [SerializeField, Tooltip("手裏剣の発射間隔(秒)")]
    float syurikenAttackInterval = 0.2f;

    [SerializeField, Tooltip("剣の攻撃間隔(秒)")]
    float swordAttackInterval = 0.4f;

    [SerializeField, Tooltip("斧の攻撃間隔(秒)")]
    float axeAttackInterval = 0.5f;

    [SerializeField, Tooltip("斧の生存時間(秒)")]
    float axeLifeTime = 2.0f;

    [SerializeField, Tooltip("ボールの攻撃間隔(秒)")]
    float ballAttackInterval = 0.5f;

    [SerializeField, Tooltip("ボールの生存時間(秒)")]
    float ballLifeTime = 3.0f;

    [SerializeField, Tooltip("ヴィネットをかけるHP値")]
    float vignetteHp = 3.0f;

    [SerializeField, Tooltip("点滅の切り替え間隔(秒)")]
    float blinkInterval = 0.08f;

    [SerializeField, Tooltip("獲得可能なすべての武器オブジェクト一覧")]
    array<Object@>@ allWeapons;

    [SerializeField, Tooltip("武器一覧")]
    array<Object@>@ weapons;

    [SerializeField, Tooltip("現在装備中の武器タイプ(WeaponListの値。-1は未所持=武器なし)")]
    int currentWeaponType = -1;

    [SerializeField, Tooltip("押し戻しを行わない相手のタグ一覧")]
    array<string>@ pushBackExcludeTags;

    [SerializeField, Tooltip("回復アイテム")]
    Object@ healItem;

    [SerializeField, Tooltip("ゲーム画面")]
    Object@ gameScreen;

    VignetteEffect@ vignetteEffect;

    // クールダウン計算用タイマー
    float swordCooldownTimer = 0.0f;
    float syurikenCooldownTimer = 0.0f;
    float axeCooldownTimer = 0.0f;
    float ballCooldownTimer = 0.0f;

    // 斧クローン管理用
    array<Object@> axeClones;
    array<float> axeTimers;

    // 手裏剣クローン管理用
    array<Object@> syurikenClones;
    array<float> syurikenTimers;

    // ボールクローン管理用を追加
    array<Object@> ballClones;
    array<float> ballTimers;

    Box2DCollider@ col;
    CharacterController2D@ controller;
    SpriteRenderer@ sprite;
    array<AudioSource@>@ audioSources;

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
        GetComponents(@audioSources);
    }

    void PlayTaggedAudio(const string &in tagName) {
        if(audioSources is null) return;
        for(uint i = 0; i < audioSources.length(); ++i) {
            if(audioSources[i] !is null && audioSources[i].GetTag() == tagName) {
                audioSources[i].Play();
            }
        }
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        if(controller !is null) {
            if(controller.IsGrounded() && velocity.y <= 0.0f) {
                velocity.y = 0.0f;
                if(isJump) {
                    PlayTaggedAudio("Landing");
                }
                isJump = false;
            }
            if(controller.IsTouchingCeiling() && velocity.y > 0.0f) {
                velocity.y = 0.0f;
            }
        }

        float moveX = GetCommandValue("MoveX");
        velocity.x = moveX * moveSpeed;

        if (moveX >= 0.01f) {
            lastDirection = Direction::Right;
        } else if (moveX <= -0.01f) {
            lastDirection = Direction::Left;
        }

        float rotY = (lastDirection == Direction::Left) ? 3.14159f : 0.0f;
        tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

        if(IsCommandTriggered("Jump") && !isJump){
            velocity.y = jumpPower;
            isJump = true;
            PlayTaggedAudio("Jump");
        }

        if (weapons !is null && weapons.length() > 0) {
            if (IsCommandTriggered("WeaponChangeRight")) {
                int next = FindOwnedWeapon(1);
                if (next >= 0) {
                    currentWeaponType = next;
                    Log("WeaponType" + currentWeaponType);
                }
            }
            if (IsCommandTriggered("WeaponChangeLeft")) {
                int prev = FindOwnedWeapon(-1);
                if (prev >= 0) {
                    currentWeaponType = prev;
                    Log("WeaponType" + currentWeaponType);
                }
            }
        }

        velocity.y -= gravity * GetDeltaTime();

        Vector2 movement = velocity * GetDeltaTime();
        if(controller !is null) {
            controller.Move(movement);
        } else {
            tf.SetTranslate(tf.GetTranslate() + Vector3(movement.x, movement.y, 0.0f));
        }

        bool hasWeapon = weapons !is null && currentWeaponType >= 0
            && uint(currentWeaponType) < weapons.length() && weapons[currentWeaponType] !is null;

        // 攻撃クールダウンタイマーの更新
        if (swordCooldownTimer > 0.0f) {
            swordCooldownTimer -= GetDeltaTime();
        }
        if (syurikenCooldownTimer > 0.0f) {
            syurikenCooldownTimer -= GetDeltaTime();
        }
        if (axeCooldownTimer > 0.0f) {
            axeCooldownTimer -= GetDeltaTime();
        }
        // Ball用に追加
        if (ballCooldownTimer > 0.0f) {
            ballCooldownTimer -= GetDeltaTime();
        }

        // 攻撃入力の検知とタイマーリセット
        if (IsCommandTriggered("Attack")) {
            bool canAttack = false;
            if (currentWeaponType == WeaponList::Katana && swordCooldownTimer <= 0.0f) {
                canAttack = true;
                swordCooldownTimer = swordAttackInterval;
            } else if (currentWeaponType == WeaponList::Shuriken && syurikenCooldownTimer <= 0.0f) {
                canAttack = true;
                syurikenCooldownTimer = syurikenAttackInterval;
            } else if (currentWeaponType == WeaponList::Axe && axeCooldownTimer <= 0.0f) {
                canAttack = true;
                axeCooldownTimer = axeAttackInterval;
            // Ballの攻撃判定を追加
            } else if (currentWeaponType == WeaponList::Ball && ballCooldownTimer <= 0.0f) {
                canAttack = true;
                ballCooldownTimer = ballAttackInterval;
            }

            // 攻撃実行処理
            if (canAttack) {
                attackTimer = attackDuration;

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
                        case WeaponList::Katana:
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

                        case WeaponList::Axe:
                            {
                                Object@ cloneAxe = GetScene().CloneObject(weapons[currentWeaponType], "CloneAxe");
                                if (cloneAxe !is null) {
                                    cloneAxe.SetActive(true);

                                    Transform@ cloneTf = cloneAxe.GetTransform();
                                    if (cloneTf !is null) {
                                        cloneTf.SetTranslate(tf.GetTranslate());
                                    }
                                    
                                    ScriptComponent@ cloneSc;
                                    if (cloneAxe.GetComponent(@cloneSc)) {
                                        cloneSc.SetVariable("pos", tf.GetTranslate());
                                        cloneSc.CallMethod("Attack", syurikenMoveX);
                                    }

                                    axeClones.insertLast(cloneAxe);
                                    axeTimers.insertLast(0.0f);
                                }
                            }
                            break;

                        // Ball用の射出処理を追加
                        case WeaponList::Ball:
                            {
                                Object@ cloneBall = GetScene().CloneObject(weapons[currentWeaponType], "CloneBall");
                                if (cloneBall !is null) {
                                    cloneBall.SetActive(true);

                                    Transform@ cloneTf = cloneBall.GetTransform();
                                    if (cloneTf !is null) {
                                        cloneTf.SetTranslate(tf.GetTranslate());
                                    }
                                    
                                    ScriptComponent@ cloneSc;
                                    if (cloneBall.GetComponent(@cloneSc)) {
                                        cloneSc.SetVariable("pos", tf.GetTranslate());
                                        cloneSc.CallMethod("Attack", syurikenMoveX); // 向きはそのまま流用
                                    }

                                    ballClones.insertLast(cloneBall);
                                    ballTimers.insertLast(0.0f);
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        // 武器の座標制御
        uint swordIndex = uint(WeaponList::Katana);
        if (weapons !is null && swordIndex < weapons.length() && weapons[swordIndex] !is null) {
            ScriptComponent@ sc;
            if (weapons[swordIndex].GetComponent(@sc)) {
                Vector3 targetPos;
                if (currentWeaponType == WeaponList::Katana) {
                    targetPos = tf.GetTranslate();
                } else {
                    targetPos = Vector3(-1000.0f, 0.0f, 0.0f);
                }

                Vector3 pos;
                if (sc.GetVariable("pos", pos)) {
                    sc.SetVariable("pos", targetPos);
                }
            }
        }

        bool isAttacking = false;
        if(attackTimer > 0.0f){
            attackTimer -= GetDeltaTime();
            isAttacking = true;
        }

        if(hp <= vignetteHp){
            VignetteEffect@ vf;
            if(gameScreen.GetComponent(@vf)){
                vf.SetIntensity(1.0f);
            }
        }

        if(hp <= 0.0f){
            isAlive = false;
        }

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

        if (nextState != state) {
            state = nextState;

            array<ScriptComponent@>@ scripts;
            if (GetComponents(@scripts)) {
                for(int i = 0; i < scripts.length(); ++i){
                    if(scripts[i].GetTag() == "AnimatorSC"){
                        scripts[i].CallMethod("PlayRow", int(state));
                        if (state == State::Walk || state == State::WalkAttack) {
                            scripts[i].CallMethod("SetFrameCount", 3);
                        } else {
                            scripts[i].CallMethod("SetFrameCount", 1);
                        }
                    } 
                }
            }
        }

        if (isInvincible) {
            invincibleTimer += GetDeltaTime();
            bool isVisible = (int(invincibleTimer / blinkInterval) % 2 == 0);
            if (sprite !is null) {
                sprite.SetActive(isVisible);
            }

            if (invincibleTimer >= invincibleDuration) {
                invincibleTimer = 0.0f;
                isInvincible = false;
                if (sprite !is null) {
                    sprite.SetActive(true);
                }
            }
        }

        if(invincibleTimer >= invincibleDuration){
            invincibleTimer = 0.0f;
            isInvincible = false;
        }

        // 手裏剣クローンの更新および削除処理
        for (uint i = 0; i < syurikenClones.length(); ) {
            Object@ clone = syurikenClones[i];
            if (clone !is null) {
                syurikenTimers[i] += GetDeltaTime();
                if (!clone.IsActive() || syurikenTimers[i] >= syurikenLifeTime) {
                    clone.SetActive(false);
                    syurikenClones.removeAt(i);
                    syurikenTimers.removeAt(i);
                    continue;
                }
            } else {
                syurikenClones.removeAt(i);
                syurikenTimers.removeAt(i);
                continue;
            }
            i++;
        }

        // 斧クローンの更新および削除処理
        for (uint i = 0; i < axeClones.length(); ) {
            Object@ clone = axeClones[i];
            if (clone !is null) {
                axeTimers[i] += GetDeltaTime();
                if (!clone.IsActive() || axeTimers[i] >= axeLifeTime) {
                    clone.SetActive(false);
                    axeClones.removeAt(i);
                    axeTimers.removeAt(i);
                    continue;
                }
            } else {
                axeClones.removeAt(i);
                axeTimers.removeAt(i);
                continue;
            }
            i++;
        }

        // ボールクローンの更新および削除処理
        for (uint i = 0; i < ballClones.length(); ) {
            Object@ clone = ballClones[i];
            if (clone !is null) {
                ballTimers[i] += GetDeltaTime();
                if (!clone.IsActive() || ballTimers[i] >= ballLifeTime) {
                    clone.SetActive(false);
                    ballClones.removeAt(i);
                    ballTimers.removeAt(i);
                    continue;
                }
            } else {
                ballClones.removeAt(i);
                ballTimers.removeAt(i);
                continue;
            }
            i++;
        }
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if(hit.otherCollider.GetTag() == "Enemy" && !isInvincible){
            Damage(1.0f);
            isInvincible = true;
        }

        if(hit.otherCollider.GetTag() == "Heart"){
            Heal(1.0f);
            healItem.SetActive(false);
        }
    }

    void AddWeaponByName(const string &in name) {
        if (allWeapons is null) return;

        int targetIndex = -1;
        if (name == "Katana") {
            targetIndex = int(WeaponList::Katana);
        } else if (name == "Shuriken") {
            targetIndex = int(WeaponList::Shuriken);
        } else if (name == "Axe") {
            targetIndex = int(WeaponList::Axe);
        // Ballを受け取る処理を追加
        } else if (name == "Ball") {
            targetIndex = int(WeaponList::Ball);
        }

        if (targetIndex >= 0 && uint(targetIndex) < allWeapons.length()) {
            Object@ weaponObj = allWeapons[targetIndex];
            if (weaponObj !is null) {
                AddWeapon(targetIndex, weaponObj);
            }
        }
    }

    void AddWeapon(int weaponType, Object@ newWeapon) {
        if (newWeapon is null || weaponType < 0) return;

        if (weapons is null) {
            @weapons = array<Object@>();
        }

        while (weapons.length() <= uint(weaponType)) {
            weapons.insertLast(null);
        }

        if (weapons[weaponType] !is null) {
            Log("すでに所持している武器です");
            return;
        }

        @weapons[weaponType] = newWeapon;
        currentWeaponType = weaponType;
        Log("新しい武器を獲得！ 現在の武器タイプ: " + currentWeaponType);
    }

    int FindOwnedWeapon(int step) {
        if (weapons is null || weapons.length() == 0) return -1;

        int count = int(weapons.length());
        int start = (currentWeaponType >= 0 && uint(currentWeaponType) < weapons.length())
            ? currentWeaponType : 0;

        for (int i = 1; i <= count; ++i) {
            int idx = ((start + step * i) % count + count) % count;
            if (weapons[idx] !is null) return idx;
        }
        return -1;
    }

    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherCollider.GetTag() == "Chest" && IsCommandTriggered("Bottom")) {
            Object@ chestObj = hit.otherObject;
            if (chestObj !is null) {
                ScriptComponent@ chestSc;
                if (chestObj.GetComponent(@chestSc)) {
                    bool isOpen = false;
                    chestSc.GetVariable("isOpen", isOpen);

                    if (!isOpen) {
                        chestSc.CallMethod("Open");

                        string itemName = "";
                        if (chestSc.GetVariable("itemName", itemName)) {
                            AddWeaponByName(itemName);
                        }
                    }
                }
            }
        }
    }

    void Damage(float amount) {
        hp = Clamp(hp - amount, 0.0f, maxHp);
        Log("Damage! HP:" + hp);
        PlayTaggedAudio("Damage");
    }

    void Heal(float amount) {
        hp = Clamp(hp + amount, 0.0f, maxHp);
        Log("Heal! HP:" + hp);
    }

    void End() {
        Log("Player End");
    }

    void AddExp(float expAmount) {
        if (weapons is null || uint(currentWeaponType) >= weapons.length()) return;

        Object@ currentWeapon = weapons[currentWeaponType];
        if (currentWeapon !is null) {
            ScriptComponent@ sc;
            if (currentWeapon.GetComponent(@sc)) {
                sc.CallMethod("AddExp", expAmount);
            }
        }
    }
}