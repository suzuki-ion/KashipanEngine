#include "WeaponList.as"

enum State {
    Idle,
    Walk,
    Jump,
    Attack,
    WalkAttack,
    Dead,
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

    [SerializeField, Tooltip("ボタンを離した際の上昇速度カット率(小ジャンプ調整用)")]
    float jumpCutFactor = 0.5f;

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

    [SerializeField, Tooltip("即死となる落下距離")]
    float fallDeathHeight = 150.0f;

    [SerializeField, Tooltip("即死となる最下限のY座標(落下死高度)")]
    float fallDeathY = -50.0f;

    [SerializeField, Tooltip("ノックバック時の横方向の力")]
    float knockbackForceX = 100.0f;

    [SerializeField, Tooltip("ノックバック時の縦方向の力")]
    float knockbackForceY = 50.0f;

    [SerializeField, Tooltip("ノックバック中の制御不能時間(秒)")]
    float knockbackDuration = 0.2f;

    [SerializeField, Tooltip("獲得可能なすべての武器オブジェクト一覧")]
    array<Object@>@ allWeapons;

    [SerializeField, Tooltip("武器一覧")]
    array<Object@>@ weapons;

    [SerializeField, Tooltip("現在装備中の武器タイプ(WeaponListの値。-1は未所持=武器なし)")]
    int currentWeaponType = -1;

    [SerializeField, Tooltip("押し戻しを行わない相手のタグ一覧")]
    array<string>@ pushBackExcludeTags;

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

    // 動く床への追従用（JobHuntingGameのPlayerMovementと同じ仕組み）。
    // 接地している床のTransformとPreTransform（前フレーム値）との差分をこのフレームの
    // 床の移動量として記録し（OnCollisionStay）、Update側でその移動量を自機の速度に加算する。
    // 離床した瞬間の値をそのまま慣性として使い続けるため、動く床から降りた直後の勢いも再現される
    Vector2 surfaceVelocity = Vector2(0.0f, 0.0f);
    Vector2 rawGroundDelta = Vector2(0.0f, 0.0f);
    bool hasGroundContact = false;

    State state = State::Idle;
    Direction lastDirection = Direction::Right;
    bool isInvincible = false;
    float invincibleDuration = 1.0f;
    float invincibleTimer = 0.0f;
    bool isAlive = true;

    // 落下開始時の最高到達Y座標
    float highestY = 0.0f;
    
    // 攻撃用タイマー
    float attackTimer = 0.0f;

    // ノックバックタイマー
    float knockbackTimer = 0.0f;

    float moveX = 0.0f;

    Transform@ tf;

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

        LoadProgress();

        // 未装備かつ武器が存在する場合、自動で最初の所持武器を装備
        if (currentWeaponType < 0 && weapons !is null && weapons.length() > 0) {
            currentWeaponType = FindOwnedWeapon(1);
        }
    }

    // 現在のシーン実行状態でセーブデータの読み書きを行ってよいかどうか
    // (エディターでPlayを押さずに編集しているだけの間は保存/読込を行わない。
    //  エディターを持たないビルド(Release相当)ではIsPlaying()が常にfalseになる仕様のため、
    //  そちらは常に許可する)
    bool ShouldPersistProgress() {
        return GetScene().IsPlaying() || !IsEditorBuild();
    }

    // WeaponListの値からセーブキーに使う武器名を返す(対応が無ければ空文字)
    string GetWeaponSaveKeyName(int weaponType) {
        if (weaponType == int(WeaponList::Katana)) return "Katana";
        if (weaponType == int(WeaponList::Shuriken)) return "Shuriken";
        if (weaponType == int(WeaponList::Axe)) return "Axe";
        if (weaponType == int(WeaponList::Ball)) return "Ball";
        return "";
    }

    // グローバルシーン変数(メモリ上。シーンを跨いでも値が残る)から進行状況を読み込み、
    // 自身と所持武器へ反映する
    // (ディスクのセーブファイルからの読込は、このプレイセッション中で最初の1回だけ行う。
    //  毎シーンでディスクから読み直してしまうと、セーブ地点をまだ通っていない状態変化
    //  ―別シーンで拾ったばかりの武器など―が、古いセーブファイルの内容で
    //  上書きされてしまうため)
    void LoadProgress() {
        if (!ShouldPersistProgress()) return;

        bool hasLoadedSaveThisSession = false;
        GetScene().GetGlobalVariable("hasLoadedSaveThisSession", hasLoadedSaveThisSession);
        if (!hasLoadedSaveThisSession) {
            GetScene().LoadGlobalVariables();
            GetScene().SetGlobalVariable("hasLoadedSaveThisSession", true);
        }

        float savedHp;
        if (GetScene().GetGlobalVariable("save_hp", savedHp)) {
            hp = savedHp;
        }

        if (allWeapons !is null) {
            for (uint i = 0; i < allWeapons.length(); ++i) {
                string keyName = GetWeaponSaveKeyName(int(i));
                if (keyName.length() == 0) continue;

                bool owned = false;
                GetScene().GetGlobalVariable("save_weapon_" + keyName + "_owned", owned);
                if (!owned) continue;

                Object@ weaponObj = allWeapons[i];
                if (weaponObj is null) continue;

                AddWeapon(int(i), weaponObj);

                ScriptComponent@ sc;
                if (weaponObj.GetComponent(@sc)) {
                    int level;
                    if (GetScene().GetGlobalVariable("save_weapon_" + keyName + "_level", level)) {
                        sc.SetVariable("level", level);
                    }
                    float exp;
                    if (GetScene().GetGlobalVariable("save_weapon_" + keyName + "_exp", exp)) {
                        sc.SetVariable("exp", exp);
                    }
                    float nextExp;
                    if (GetScene().GetGlobalVariable("save_weapon_" + keyName + "_nextExp", nextExp)) {
                        sc.SetVariable("nextExp", nextExp);
                    }
                    float weaponDamage;
                    if (GetScene().GetGlobalVariable("save_weapon_" + keyName + "_damageAmount", weaponDamage)) {
                        sc.SetVariable("damageAmount", weaponDamage);
                    }
                }
            }
        }

        int savedWeaponType;
        if (GetScene().GetGlobalVariable("save_currentWeaponType", savedWeaponType)) {
            if (weapons !is null && savedWeaponType >= 0 && uint(savedWeaponType) < weapons.length() && weapons[savedWeaponType] !is null) {
                currentWeaponType = savedWeaponType;
            }
        }
    }

    // 現在の進行状況をグローバルシーン変数(メモリ上)へ書き出す。ファイルへは保存しない
    // (シーン切り替え時の引き継ぎ用。End()から毎回呼ばれる)
    void UpdateProgressVariables() {
        if (!ShouldPersistProgress()) return;

        GetScene().SetGlobalVariable("save_hp", hp);
        GetScene().SetGlobalVariable("save_currentWeaponType", currentWeaponType);

        if (allWeapons !is null) {
            for (uint i = 0; i < allWeapons.length(); ++i) {
                string keyName = GetWeaponSaveKeyName(int(i));
                if (keyName.length() == 0) continue;

                bool owned = weapons !is null && i < weapons.length() && weapons[i] !is null;
                GetScene().SetGlobalVariable("save_weapon_" + keyName + "_owned", owned);
                if (!owned) continue;

                ScriptComponent@ sc;
                if (weapons[i].GetComponent(@sc)) {
                    int level = 0;
                    float exp = 0.0f;
                    float nextExp = 0.0f;
                    float weaponDamage = 0.0f;
                    sc.GetVariable("level", level);
                    sc.GetVariable("exp", exp);
                    sc.GetVariable("nextExp", nextExp);
                    sc.GetVariable("damageAmount", weaponDamage);

                    GetScene().SetGlobalVariable("save_weapon_" + keyName + "_level", level);
                    GetScene().SetGlobalVariable("save_weapon_" + keyName + "_exp", exp);
                    GetScene().SetGlobalVariable("save_weapon_" + keyName + "_nextExp", nextExp);
                    GetScene().SetGlobalVariable("save_weapon_" + keyName + "_damageAmount", weaponDamage);
                }
            }
        }
    }

    // セーブ地点(SavePoint等)からのシーン変数経由の要求で呼ばれる、実際にファイルへ書き込む処理。
    // メモリ上のグローバルシーン変数を最新化してから、まとめてセーブファイルへ保存する
    // @return 実際にファイルへの保存を行った場合はtrue(Playを押していないエディター編集中等は
    //         ShouldPersistProgress()がfalseになりそのままfalseを返す)
    bool SaveProgress() {
        if (!ShouldPersistProgress()) return false;

        UpdateProgressVariables();
        return GetScene().SaveGlobalVariables();
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
        // シーン変数経由のセーブ要求を監視する(SavePoint.as参照)。
        // 消費したら自分でfalseへ戻す(シーン変数は自動ではリセットされないため)
        bool saveRequested = false;
        GetScene().GetVariable("saveRequested", saveRequested);
        if (saveRequested) {
            GetScene().SetVariable("saveRequested", false);
            if (SaveProgress()) {
                Log("セーブしました");
            }
        }

        // 会話中(isDialogueActive)は移動・攻撃などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        @tf = GetTransform();
        if(tf is null) return;

        if(controller !is null) {
            float currentY = tf.GetTranslate().y;

            if(controller.IsGrounded()) {
                if(velocity.y <= 0.0f) {
                    // 着地時に落下距離を判定
                    float fallDistance = highestY - currentY;
                    if (isAlive && isJump && fallDistance >= fallDeathHeight) {
                        Damage(hp);
                    }

                    velocity.y = 0.0f;
                    if(isJump) {
                        PlayTaggedAudio("Landing");
                    }
                    isJump = false;
                }
                highestY = currentY; // 地上にいる間は常にY座標を更新
            } else {
                isJump = true;
                // 空中で最高到達点を記録
                if (currentY > highestY) {
                    highestY = currentY;
                }

                // 空中での落下死判定（設定した落下距離超過、または最下限Y座標を下回った場合）
                if (isAlive) {
                    float fallDistance = highestY - currentY;
                    if (fallDistance >= fallDeathHeight || currentY <= fallDeathY) {
                        Damage(hp);
                    }
                }
            }

            if(controller.IsTouchingCeiling() && velocity.y > 0.0f) {
                velocity.y = 0.0f;
            }
        }

        // ノックバック中のタイマー更新と移動入力制御
        if (knockbackTimer > 0.0f) {
            knockbackTimer -= GetDeltaTime();
        } else {
            moveX = GetCommandValue("MoveX");
            velocity.x = moveX * moveSpeed;

            if (moveX >= 0.01f) {
                lastDirection = Direction::Right;
            } else if (moveX <= -0.01f) {
                lastDirection = Direction::Left;
            }
        }

        float rotY = (lastDirection == Direction::Left) ? 3.14159f : 0.0f;
        tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

        if(IsCommandTriggered("Jump") && !isJump){
            velocity.y = jumpPower;
            isJump = true;
            PlayTaggedAudio("Jump");
        }

        // ボタンを押していない状態で上昇中の時、Y速度を削ってジャンプを中断させる
        if(GetCommandValue("Jump") <= 0.0f && velocity.y > 0.0f && isJump) {
            velocity.y *= jumpCutFactor;
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

        // 接地中の床の速度を実測する（動く床への追従・離床後の慣性の両方に使う）。
        // hasGroundContactがfalseの間（空中）は直前に測定した値をそのまま使い続ける
        if (hasGroundContact && GetDeltaTime() > 0.0f) {
            surfaceVelocity = rawGroundDelta / GetDeltaTime();
        }

        Vector2 movement = (velocity + surfaceVelocity) * GetDeltaTime();
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

                // プレイヤーの最新の向きに応じたmarginをKatanaに送信
                float margin = (lastDirection == Direction::Right) ? 16.0f : -16.0f;
                sc.SetVariable("currentMargin", margin);
            }
        }

        bool isAttacking = false;
        if(attackTimer > 0.0f){
            attackTimer -= GetDeltaTime();
            isAttacking = true;
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

        // 生の接触情報は毎フレームリセットする（次フレームの衝突コールバックで再設定される）
        hasGroundContact = false;
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        ProcessDamageAndKnockback(hit);

        if(hit.otherCollider.GetTag() == "Heart"){
            Heal(1.0f);
        }

        if(hit.otherCollider.GetTag() == "DeadArea"){
            Damage(100.0f);
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
        // 重複・長時間の接触中に無敵時間が切れた場合もダメージを受けるように処理
        ProcessDamageAndKnockback(hit);

        // 動く床への追従。地面のTransformとPreTransform（前フレームの値）との差分から
        // 実際の移動量を求めておき、Update側でsurfaceVelocityへ変換する
        // （velocity本体に加算すると接地中に蓄積し続けてしまうため）
        if (hit.otherObject !is null && hit.otherCollider !is null && !hit.otherCollider.IsTrigger()
        && hit.normal.y > groundedThreshold) {
            hasGroundContact = true;

            Transform@ groundTransform;
            PreTransform@ groundPreTransform;
            if (hit.otherObject.GetComponent(@groundTransform) && hit.otherObject.GetComponent(@groundPreTransform)) {
                Vector3 delta = groundTransform.GetTranslate() - groundPreTransform.GetPreviousTranslate();
                rawGroundDelta = Vector2(delta.x, delta.y);
            } else {
                rawGroundDelta = Vector2(0.0f, 0.0f);
            }
        }

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

    void ProcessDamageAndKnockback(const HitInfo &in hit) {
        Tag tag = hit.otherCollider.GetTag();
        if ((tag == "Enemy" || tag == "Needle") && !isInvincible) {
            Damage(1.0f);
            isInvincible = true;

            // 敵との位置関係からノックバック方向を決定
            if (hit.otherObject !is null) {
                Transform@ enemyTf = hit.otherObject.GetTransform();
                if (enemyTf !is null) {
                    float dirX = (tf.GetTranslate().x >= enemyTf.GetTranslate().x) ? 1.0f : -1.0f;
                    velocity.x = dirX * knockbackForceX;
                    velocity.y = knockbackForceY;
                    knockbackTimer = knockbackDuration;
                    isJump = true;
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
        // シーン切り替え時の引き継ぎ(メモリ上の更新のみ)。ファイルへの保存はSavePointから行う
        UpdateProgressVariables();
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