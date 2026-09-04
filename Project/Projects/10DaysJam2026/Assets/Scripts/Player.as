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

    // 手裏剣クローン管理用
    array<Object@> syurikenClones;
    array<float> syurikenTimers;

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

    // タグの一致するAudioSourceの音を再生する
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

        // 前フレーム後半に確定した接触結果から、接触面へ向かう縦速度だけを消す。
        // 壁との接触では縦速度を変更しない。
        if(controller !is null) {
            if(controller.IsGrounded() && velocity.y <= 0.0f) {
                velocity.y = 0.0f;
                if(isJump) {
                    PlayTaggedAudio("Landing"); // 着地時の音を再生
                }
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
            PlayTaggedAudio("Jump"); // ジャンプ時の音を再生
        }

        // 武器の切り替え(所持している武器タイプの中だけを巡回する)
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

        // weapons未設定・currentWeaponTypeが範囲外/未所持(null)の場合の"Index out of bounds"を防ぐ
        bool hasWeapon = weapons !is null && currentWeaponType >= 0
            && uint(currentWeaponType) < weapons.length() && weapons[currentWeaponType] !is null;

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
            if (currentWeaponType == WeaponList::Katana && swordCooldownTimer <= 0.0f) {
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
                
                // 装備中がソードならプレイヤーの位置に、手裏剣などそれ以外なら退避座標へ移動
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

        // 攻撃中判定の更新
        bool isAttacking = false;
        if(attackTimer > 0.0f){
            attackTimer -= GetDeltaTime();
            isAttacking = true;
        }

        // HPが指定値以下になったらヴィネットをかける
        if(hp <= vignetteHp){
            VignetteEffect@ vf;
            if(gameScreen.GetComponent(@vf)){
                vf.SetIntensity(1.0f);
            }
        }

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

            array<ScriptComponent@>@ scripts;
            if (GetComponents(@scripts)) {
                for(int i = 0; i < scripts.length(); ++i){
                    if(scripts[i].GetTag() == "AnimatorSC"){
                        // Y位置の変更。Stateの数値をそのまま行番号として渡す
                        scripts[i].CallMethod("PlayRow", int(state));
        
                        // X位置の変更。歩きと歩き攻撃のみ3コマ、他は1コマ
                        if (state == State::Walk || state == State::WalkAttack) {
                            scripts[i].CallMethod("SetFrameCount", 3);
                        } else {
                            scripts[i].CallMethod("SetFrameCount", 1);
                        }
                    } 
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

    // 武器名から対応する武器を探して追加する
    void AddWeaponByName(const string &in name) {
        if (allWeapons is null) return;

        // WeaponListの定数と照合
        int targetIndex = -1;
        if (name == "Katana") {
            targetIndex = int(WeaponList::Katana);
        } else if (name == "Shuriken") {
            targetIndex = int(WeaponList::Shuriken);
        }

        // 対象の武器オブジェクトが存在すれば追加処理を行う
        if (targetIndex >= 0 && uint(targetIndex) < allWeapons.length()) {
            Object@ weaponObj = allWeapons[targetIndex];
            if (weaponObj !is null) {
                AddWeapon(targetIndex, weaponObj);
            }
        }
    }

    // 武器をweaponsに追加する処理
    // weaponsはallWeaponsと同じ並び(WeaponListの値=インデックス)で管理し、
    // 未所持のスロットはnullのままにする
    void AddWeapon(int weaponType, Object@ newWeapon) {
        if (newWeapon is null || weaponType < 0) return;

        if (weapons is null) {
            @weapons = array<Object@>();
        }

        // weaponTypeのスロットまで配列を広げる(未所持分はnullで埋める)
        while (weapons.length() <= uint(weaponType)) {
            weapons.insertLast(null);
        }

        // 重複チェック
        if (weapons[weaponType] !is null) {
            Log("すでに所持している武器です");
            return;
        }

        // 所持リストに追加し、取得した武器を自動装備
        @weapons[weaponType] = newWeapon;
        currentWeaponType = weaponType;
        Log("新しい武器を獲得！ 現在の武器タイプ: " + currentWeaponType);
    }

    // 現在位置からstep方向(+1/-1)に所持済みの武器タイプを探す。見つからなければ-1
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

    // 宝箱との接触・開ける処理
    void OnCollisionStay(const HitInfo &in hit) {
        // 宝箱に接しているときに下ボタンを押したら
        if (hit.otherCollider.GetTag() == "Chest" && IsCommandTriggered("Bottom")) {
            Object@ chestObj = hit.otherObject;
            if (chestObj !is null) {
                ScriptComponent@ chestSc;
                if (chestObj.GetComponent(@chestSc)) {
                    bool isOpen = false;
                    chestSc.GetVariable("isOpen", isOpen);

                    // 未開封の場合のみ開く処理を実行
                    if (!isOpen) {
                        chestSc.CallMethod("Open");

                        // string型で宝箱の中身の名前を取得
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
        PlayTaggedAudio("Damage"); // ダメージ時の音を再生
    }

    void Heal(float amount) {
        hp = Clamp(hp + amount, 0.0f, maxHp);
        Log("Heal! HP:" + hp);
    }

    void End() {
        Log("Player End");
    }

    // 経験値獲得処理
    void AddExp(float expAmount) {
        if (weapons is null || uint(currentWeaponType) >= weapons.length()) return;

        Object@ currentWeapon = weapons[currentWeaponType];
        if (currentWeapon !is null) {
            ScriptComponent@ sc;
            if (currentWeapon.GetComponent(@sc)) {
                // 装備中の武器に経験値を渡す
                sc.CallMethod("AddExp", expAmount);
            }
        }
    }
}
