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

    [SerializeField, Tooltip("地上での加速度(1秒あたりの速度増加量。大きいほど最高速に達するまでが速い)")]
    float groundAcceleration = 450.0f;

    [SerializeField, Tooltip("地上での減速度(1秒あたりの速度減少量。大きいほど止まるまでが速い)")]
    float groundDeceleration = 600.0f;

    [SerializeField, Tooltip("ジャンプ力")]
    float jumpPower = 66.6667f;

    [SerializeField, Tooltip("ボタンを離した際の上昇速度カット率(小ジャンプ調整用)")]
    float jumpCutFactor = 0.5f;

    [SerializeField, Tooltip("接地でなくなってからもジャンプ入力を受け付ける猶予時間(秒)。いわゆるコヨーテタイム")]
    float coyoteTime = 0.1f;

    [SerializeField, Tooltip("重力")]
    float gravity = 120.0f;

    [SerializeField, Tooltip("落下速度の上限(ターミナルベロシティ)。際限なく加速し続けないようにする")]
    float maxFallSpeed = 200.0f;

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

    [SerializeField, Tooltip("ノックバック時の横方向の力")]
    float knockbackForceX = 100.0f;

    [SerializeField, Tooltip("ノックバック時の縦方向の力")]
    float knockbackForceY = 50.0f;

    [SerializeField, Tooltip("ノックバック中の制御不能時間(秒)")]
    float knockbackDuration = 0.2f;

    [SerializeField, Tooltip("被ダメージ時に装備中の武器から減少させる経験値量")]
    float weaponExpLossOnDamage = 1.0f;

    [SerializeField, Tooltip("死亡してからゲームオーバー要求(シーン変数gameOverRequested)を出すまでの秒数(死亡アニメーションを見せる時間)")]
    float deathToGameOverDelay = 1.0f;

    [SerializeField, Tooltip("獲得可能なすべての武器オブジェクト一覧")]
    array<Object@>@ allWeapons;

    [SerializeField, Tooltip("武器一覧")]
    array<Object@>@ weapons;

    [SerializeField, Tooltip("現在装備中の武器タイプ(WeaponListの値。-1は未所持=武器なし)")]
    int currentWeaponType = -1;

    [SerializeField, Tooltip("押し戻しを行わない相手のタグ一覧")]
    array<string>@ pushBackExcludeTags;

    [SerializeField, Tooltip("カメラ")]
    Object@ camera;

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
    bool jumpButtonReleased = true;

    // コヨーテタイム用。接地している間はcoyoteTimeへ毎フレーム戻し、接地でなくなってからは
    // 減っていく。0より大きい間だけジャンプ入力を受け付ける
    float coyoteTimer = 0.0f;
    // 今の空中滞在（接地〜次に接地するまでの間）で既にジャンプを使ったかどうか。
    // これが無いと、coyoteTimer>0の間ジャンプボタンを押すたび何度でも空中ジャンプできてしまう
    bool jumpConsumedThisAirtime = false;

    // 動く床への追従用。プレイヤー本体のコライダーとは別に、足元にトリガー用の
    // センサーコライダー（Tag: "GroundSensor"）を用意しておくこと。そのセンサーが
    // 床と接触している間だけ、床のスクリプトが公開している実測速度(currentVelocity)を
    // 直接自機のTransformへ加算する（詳細はOnCollisionStay参照）。
    // センサーは本体コライダーより上下に余裕を持たせられるため、床が速く動いて本体だけでは
    // 重力が追いつかず離れてしまう場合でも接触を保ちやすい。トリガーなので物理的な
    // 押し戻しも起きない。接触しなくなった瞬間から何も加算されなくなるため、
    // 慣性を持ち越すこともない

    // GroundSensorが動く床（currentVelocityを公開しているオブジェクト）に接触し、
    // 実際に追従できているかどうか。OnCollisionStayで立て、Updateの冒頭で
    // 読んだ直後にfalseへ戻す（次フレームの衝突コールバックで再設定される）。
    // 本体コライダーのcontroller.IsGrounded()は動く床の上では毎フレームのように
    // 一瞬途切れてしまい、その度に「着地」を検出してしまうため、より接触を保ちやすい
    // こちらの値もOR条件で接地判定に使う。静止した地面には一切関与させない
    // （センサーの厚みで通常の落下判定まで狂わせないため、対象を動く床に限定している）
    bool isSensorGrounded = false;

    // 直前フレームで（物理的な接触かセンサーかを問わず）接地扱いだったかどうか。
    // GroundSensorは本体コライダーより下に厚みを持たせているため、空中から動く床へ
    // 落下していく最中、本体がまだ物理的に触れていない段階でセンサーだけが先に反応して
    // しまうことがある。そこでisSensorGroundedは「元々接地していた状態を継続させる」
    // 場合にだけ信用することにし、空中からの最初の着地は必ず本体コライダーの物理的な
    // 接触（controller.IsGrounded()）でのみ判定させる（そうしないと、センサーの厚みぶん
    // だけ本来より高い位置で着地したことになり、重力が再蓄積して少しずつ沈み込むまでの間
    // プレイヤーが浮いて見えてしまう）
    bool wasGroundedPreviousFrame = false;

    State state = State::Idle;
    Direction lastDirection = Direction::Right;
    bool isInvincible = false;
    float invincibleDuration = 1.0f;
    float invincibleTimer = 0.0f;
    bool isAlive = true;
    // ゲームオーバー画面(GameOverMenu.as参照)へ要求を送出済みかどうか(二重送出防止用)
    bool gameOverRequested = false;
    // 死亡してからの経過時間(deathToGameOverDelayとの比較に使う)
    float deathTimer = 0.0f;

    // 落下開始時の最高到達Y座標
    float highestY = 0.0f;
    
    // 攻撃用タイマー
    float attackTimer = 0.0f;

    // ノックバックタイマー
    float knockbackTimer = 0.0f;

    float moveX = 0.0f;

    Transform@ tf;

    void Start() {
        hp = maxHp;

        // 足元のGroundSensor（トリガー）用に2つ目のBox2DColliderを追加している場合、
        // GetComponent(@col)は「最初に見つかった方」を返すため並び順次第で本体ではなく
        // センサーの方を掴んでしまうことがある。トリガーではない方を明示的に本体として選ぶ
        array<Box2DCollider@>@ box2DColliders;
        if (GetComponents(@box2DColliders)) {
            for (uint i = 0; i < box2DColliders.length(); ++i) {
                if (box2DColliders[i] !is null && !box2DColliders[i].IsTrigger()) {
                    @col = box2DColliders[i];
                    break;
                }
            }
        }
        if (col is null) {
            GetComponent(@col); // Box2DColliderが1つだけの場合のフォールバック
        }

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
        ApplyTransitionEntryPoint();

        // 未装備かつ武器が存在する場合、自動で最初の所持武器を装備
        if (currentWeaponType < 0 && weapons !is null && weapons.length() > 0) {
            currentWeaponType = FindOwnedWeapon(1);
        }

        SaveInitialProgressIfNewGame();
    }

    // タイトル画面で「はじめから」を選択した直後(TitleMenuController.StartNewGame()が
    // グローバル変数isNewGameStartを立てて遷移してきた場合)、この時点の状態(初期HP/
    // このシーン/武器未所持等)を一度だけセーブファイルへ書き込む。
    // こうしておかないと、最初のセーブ地点へ到達する前に死亡した場合、コンティニュー時に
    // 戻る先のセーブデータが存在せず(save_sceneNameが空)、死亡したシーンがそのまま
    // リロードされるだけになってしまう(ゲーム開始地点まで正しく戻れない)
    //
    // 通常のSaveProgress()は呼ばない(呼んではいけない)。SaveProgress()はシーン変数
    // saveRequestedPositionをそのままsave_positionとして保存するが、この変数は各シーンの
    // scene.jsonに「エディターで最後にSavePointをテストした際の座標」がそのまま残っており、
    // 実際にはSavePointに一度も触れていなくてもGetVariable()がtrueを返してしまう。
    // そのままSaveProgress()を使うと、はじめから直後にこの無関係な残留座標がsave_positionに
    // 書き込まれてしまい、セーブ地点に触れずに死亡してコンティニューした際、はじめからの
    // スタート地点とは全く違う場所へ飛ばされるバグになる。
    // そのため、ここではsave_positionには触れず(未設定のままにし)、LoadProgress()側の
    // 座標復元処理をスキップさせることで、シーンに配置された初期座標がそのまま使われるようにする
    void SaveInitialProgressIfNewGame() {
        bool isNewGameStart = false;
        GetScene().GetGlobalVariable("isNewGameStart", isNewGameStart);
        if (!isNewGameStart) return;

        // 消費(次にこのフラグが残ったまま別シーンへ入っても再セーブしないように)
        GetScene().SetGlobalVariable("isNewGameStart", false);

        if (!ShouldPersistProgress()) return;

        UpdateProgressVariables();
        GetScene().SetGlobalVariable("save_sceneName", GetScene().GetName());
        GetScene().SaveGlobalVariables();
    }

    // 現在のシーン実行状態でセーブデータの読み書きを行ってよいかどうか
    // (エディターでPlayを押さずに編集しているだけの間は保存/読込を行わない。
    //  エディターを持たないビルド(Release相当)ではIsPlaying()が常にfalseになる仕様のため、
    //  そちらは常に許可する)
    bool ShouldPersistProgress() {
        return GetScene().IsPlaying() || !IsEditorBuild();
    }

    // 死亡後、一定時間(deathToGameOverDelay)経過したらシーン変数gameOverRequestedをtrueにする。
    // GameOverMenu.as側がこれを監視してゲームオーバー画面を表示し、入力待ち後にシーンリロードを行う
    // (Playerはゲームオーバー画面のUIを直接知らない。SavePoint.as等と同じ疎結合の方針)
    void UpdateGameOverSequence() {
        if (gameOverRequested) return;

        deathTimer += GetDeltaTime();
        if (deathTimer >= deathToGameOverDelay) {
            gameOverRequested = true;
            GetScene().SetVariable("gameOverRequested", true);
        }
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

        float savedMaxHp;
        if (GetScene().GetGlobalVariable("save_maxHp", savedMaxHp)) {
            maxHp = savedMaxHp;
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

        // セーブ地点の位置復元。保存されたシーン名が現在のシーンと一致する場合のみ座標を反映する
        // (シーンをまたいだ通常の遷移中に、無関係な別シーンの座標へ誤ってテレポートしないため)
        string savedSceneName;
        if (GetScene().GetGlobalVariable("save_sceneName", savedSceneName) && savedSceneName == GetScene().GetName()) {
            Vector3 savedPosition;
            if (GetScene().GetGlobalVariable("save_position", savedPosition)) {
                Transform@ playerTf = GetTransform();
                if (playerTf !is null) {
                    playerTf.SetTranslate(savedPosition);
                }
            }
        }
    }

    // SceneTransitionArea経由でシーン遷移してきた場合、指定された入場ポイントオブジェクトの座標へ
    // プレイヤーを配置する。transitionEntrySceneNameが現在シーンと一致する場合のみ適用する
    // (LoadProgress()のsave_position復元より後に呼ぶことで、「今通ってきたドア」の座標を
    //  セーブ地点の座標より優先させる)。
    // 値は必ず読み取り直後にクリアする。消費せず残しておくと、死亡復帰や「つづきから」等の
    // 別経路で同じシーンへ入ったときにも誤って同じ入場ポイントへ飛ばされてしまうため
    void ApplyTransitionEntryPoint() {
        if (!ShouldPersistProgress()) return;

        string entrySceneName;
        bool hasEntry = GetScene().GetGlobalVariable("transitionEntrySceneName", entrySceneName);
        string entryPointName;
        GetScene().GetGlobalVariable("transitionEntryPointName", entryPointName);

        GetScene().SetGlobalVariable("transitionEntrySceneName", "");
        GetScene().SetGlobalVariable("transitionEntryPointName", "");

        if (!hasEntry || entrySceneName != GetScene().GetName() || entryPointName.length() == 0) return;

        Object@ entryObject = FindObject(entryPointName);
        if (entryObject is null) return;

        Transform@ entryTransform;
        if (!entryObject.GetComponent(@entryTransform)) return;

        Transform@ playerTf = GetTransform();
        if (playerTf !is null) {
            playerTf.SetTranslate(entryTransform.GetTranslate());
        }
    }

    // 現在の進行状況をグローバルシーン変数(メモリ上)へ書き出す。ファイルへは保存しない
    // (シーン切り替え時の引き継ぎ用。End()から毎回呼ばれる)
    void UpdateProgressVariables() {
        if (!ShouldPersistProgress()) return;

        GetScene().SetGlobalVariable("save_hp", hp);
        GetScene().SetGlobalVariable("save_maxHp", maxHp);
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

        // セーブ地点(SavePoint)がセーブ要求と合わせて渡した自身の座標を、プレイヤー自身の座標の
        // 代わりに記録する(ロード時の復帰位置に使う)。プレイヤーの座標だとセーブ地点から
        // わずかにずれた位置になりうるため、必ずセーブ地点そのものの座標を使う。
        // UpdateProgressVariables()側には含めない(あちらはシーン切り替えのたびにEnd()から
        // 毎回呼ばれるため、そこに含めると単なる通過シーンの座標で上書きされてしまう)
        Vector3 checkpointPosition;
        if (GetScene().GetVariable("saveRequestedPosition", checkpointPosition)) {
            GetScene().SetGlobalVariable("save_position", checkpointPosition);
        }
        GetScene().SetGlobalVariable("save_sceneName", GetScene().GetName());

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

    // currentをtargetへ向けてmaxDeltaぶんだけ近づけた値を返す(targetを飛び越えない)。
    // 加速度/減速度を使った速度のじわっとした変化に使用する
    float ApproachSpeed(float current, float target, float maxDelta) {
        if (current < target) {
            current += maxDelta;
            if (current > target) current = target;
        } else if (current > target) {
            current -= maxDelta;
            if (current < target) current = target;
        }
        return current;
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

        // 死亡している間は通常の移動・攻撃処理を行わず、ゲームオーバー要求の送出だけを行う
        // (実際の画面表示・入力待ち・シーンリロードはGameOverMenu.as側が担当する)
        if (!isAlive) {
            UpdateGameOverSequence();
            return;
        }

        @tf = GetTransform();
        if(tf is null) return;

        if(controller !is null) {
            float currentY = tf.GetTranslate().y;
            // 動く床の上ではcontroller.IsGrounded()単体だと一瞬接触が途切れやすいため、
            // GroundSensorの接触状態もOR条件で加味する。ただし空中からの最初の着地まで
            // センサーに任せると、本体コライダーが物理的に触れる前にセンサーの厚みぶんだけ
            // 早く「接地」扱いになってしまう（着地が高い位置で止まって浮いて見える）ため、
            // 前フレームで既に接地していた場合にだけセンサーの値を信用する
            bool grounded = controller.IsGrounded() || (isSensorGrounded && wasGroundedPreviousFrame);
            isSensorGrounded = false; // このフレーム分は使い終わったので、次の衝突コールバックまでは倒しておく
            wasGroundedPreviousFrame = grounded;

            if(grounded) {
                // 接地している間はコヨーテタイムを満タンに保ち、空中ジャンプの消費状態もリセットする
                coyoteTimer = coyoteTime;
                jumpConsumedThisAirtime = false;

                if(velocity.y <= 0.0f) {
                    velocity.y = 0.0f;
                    if(isJump) {
                        PlayTaggedAudio("Landing");
                    }
                    isJump = false;
                }
                highestY = currentY; // 地上にいる間は常にY座標を更新
            } else {
                isJump = true;
                coyoteTimer -= GetDeltaTime();
                // 空中で最高到達点を記録
                if (currentY > highestY) {
                    highestY = currentY;
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
            float rawMoveX = GetCommandValue("MoveX");
            
            // 入力値が一定(0.1)を超えたら、倒し具合にかかわらず 1.0 か -1.0 に固定する
            if (rawMoveX >= 0.1f) {
                moveX = 1.0f;
                lastDirection = Direction::Right;
            } else if (rawMoveX <= -0.1f) {
                moveX = -1.0f;
                lastDirection = Direction::Left;
            } else {
                moveX = 0.0f;
            }

            // SFC時代のアクションゲームらしく、瞬時に最高速へ切り替わるのではなく
            // 加速度/減速度を挟んで速度がじわっと変化するようにする
            // (groundAcceleration/groundDecelerationを0に近づければ従来の瞬間切り替えにも戻せる)
            float targetSpeedX = moveX * moveSpeed;
            float rateX = (moveX != 0.0f) ? groundAcceleration : groundDeceleration;
            velocity.x = ApproachSpeed(velocity.x, targetSpeedX, rateX * GetDeltaTime());
        }

        float rotY = (lastDirection == Direction::Left) ? 3.14159f : 0.0f;
        tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

        // ボタンを離した判定（入力が0以下になったら再入力を許可）
        if (GetCommandValue("Jump") <= 0.0f) {
            jumpButtonReleased = true;
        }
        
        // 再入力が許可されており、かつコヨーテタイム中（接地中を含む）でまだこの空中滞在で
        // ジャンプを使っていない場合のみジャンプを実行
        if (IsCommandTriggered("Jump") && jumpButtonReleased && coyoteTimer > 0.0f && !jumpConsumedThisAirtime) {
            velocity.y = jumpPower;
            isJump = true;
            jumpButtonReleased = false; // ジャンプ発動時にロックをかける
            jumpConsumedThisAirtime = true; // この空中滞在中はコヨーテタイムが残っていても再ジャンプさせない
            coyoteTimer = 0.0f; // 空中ジャンプ直後は猶予を打ち切る（着地するまで再ジャンプ不可）
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

        // 落下速度に上限を設ける(SFC時代のアクションゲームでよくある、際限なく加速しない落下)
        if (velocity.y < -maxFallSpeed) {
            velocity.y = -maxFallSpeed;
        }

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

        // カメラ情報の取得（画面外判定用）
        CameraController2D@ camController;
        Vector3 camPos;
        Vector2 targetSize;
        bool hasCameraData = false;

        if (camera !is null) {
            camera.GetComponent(@camController);
            if (camController !is null) {
                Transform@ camTf = camera.GetTransform();
                if (camTf !is null) {
                    camPos = tf.GetTranslate();
                    targetSize = camController.GetTargetSize();
                    hasCameraData = true;
                }
            }
        }

        // 斧クローンの更新および削除処理
        for (uint i = 0; i < axeClones.length(); ) {
            Object@ clone = axeClones[i];
            if (clone !is null) {
                axeTimers[i] += GetDeltaTime();

                // 画面外判定
                bool isOutOfBounds = false;
                // 発射後0.1秒経過してから判定を行う
                if (hasCameraData && clone.IsActive() && axeTimers[i] > 0.1f) {
                    Transform@ cloneTf = clone.GetTransform();
                    if (cloneTf !is null) {
                        Vector3 clonePos = cloneTf.GetTranslate();
                        
                        // 画面サイズの外側に少しマージンを持たせる
                        float margin = 64.0f; 
                        float halfW = targetSize.x * 0.5f + margin;
                        float halfH = targetSize.y * 0.5f + margin;

                        if (clonePos.x < camPos.x - halfW || clonePos.x > camPos.x + halfW ||
                            clonePos.y < camPos.y - halfH || clonePos.y > camPos.y + halfH) {
                            isOutOfBounds = true;
                        }
                    }
                }

                // 非アクティブ、寿命、または画面外に出た場合に削除
                if (!clone.IsActive() || axeTimers[i] >= axeLifeTime || isOutOfBounds) {
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
        ProcessDamageAndKnockback(hit);

        if(hit.otherCollider.GetTag() == "Heart"){
            Heal(1.0f);
            hit.otherObject.SetActive(false);
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

        // 動く床への追従。プレイヤーの足元に用意したセンサー用コライダー（Tag: "GroundSensor"、
        // トリガー）が床と接触している間だけ、床のスクリプトが公開している実測速度
        // (currentVelocity)をその場で即座に自機のTransformへ加算する。
        // controller.Move()は経由させない（掃引判定に床自体が引っかかって不要な
        // 押し戻し・振動が起きるのを避けるため）。センサーはトリガーなので物理的な干渉は
        // 起きず、また本体コライダーより上下に余裕を持たせられるため、床が速く動いて
        // 本体だけでは重力が追いつかず離れてしまう場合でも接触を保ちやすい。
        //
        // 接触方向はhit.normalでは判定できない（Box2Dのセンサー重なりイベントには
        // 法線・めり込み量の情報が無く、hit.normalは常に既定値のゼロになる）ため、
        // 方向チェックは行わず「currentVelocityを公開しているオブジェクトかどうか」だけで
        // 対象を絞っている。通常の静的な壁・地面はcurrentVelocityを持たないため、
        // 横から触れても以下のisSensorGroundedやTransformへの加算は発生しない
        if (tf !is null && hit.selfCollider !is null && hit.selfCollider.GetTag() == "GroundSensor"
        && hit.otherObject !is null && hit.otherObject !is hit.selfObject
        && hit.otherCollider !is null && !hit.otherCollider.IsTrigger()) {
            ScriptComponent@ groundSc;
            Vector2 groundVelocity;
            if (hit.otherObject.GetComponent(@groundSc) && groundSc.GetVariable("currentVelocity", groundVelocity)) {
                isSensorGrounded = true;

                Vector2 groundMovement = groundVelocity * GetDeltaTime();
                if (groundMovement.x != 0.0f || groundMovement.y != 0.0f) {
                    tf.SetTranslate(tf.GetTranslate() + Vector3(groundMovement.x, groundMovement.y, 0.0f));
                }
            }
        }

        if (hit.otherCollider.GetTag() == "Chest" && IsCommandTriggered("Decide")) {
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

        if (hit.otherCollider.GetTag() == "MaxHPChest" && IsCommandTriggered("Decide")) {
            Object@ chestObj = hit.otherObject;
            if (chestObj !is null) {
                array<ScriptComponent@>@ chestSc;
                if (chestObj.GetComponents(@chestSc)) {
                    for(int i = 0;i < chestSc.length(); ++i){
                        bool isOpen = false;
                        chestSc[i].GetVariable("isOpen", isOpen);
    
                        if (!isOpen) {
                            chestSc[i].CallMethod("Open");
                            maxHp += 2.0f;
                            hp += 2.0f;
                            Log("MaxHPChest Open");
                            break;
                        }
                    }
                }
            }
        }
    }

    void ProcessDamageAndKnockback(const HitInfo &in hit) {
        Tag tag = hit.otherCollider.GetTag();
        if ((tag == "Enemy" || tag == "Needle" || tag == "Bullet") && !isInvincible) {
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
        if(isInvincible) return;

        hp = Clamp(hp - amount, 0.0f, maxHp);
        Log("Damage! HP:" + hp);
        PlayTaggedAudio("Damage");

        // 被ダメージの瞬間、その時点で装備している武器の経験値を減少させる
        RemoveWeaponExp(weaponExpLossOnDamage);
    }

    void Heal(float amount) {
        hp = Clamp(hp + amount, 0.0f, maxHp);
        Log("Heal! HP:" + hp);
    }

    void End() {
        Log("Player End");
        // シーン切り替え時の引き継ぎ(メモリ上の更新のみ)。ファイルへの保存はSavePointから行う
        // (死亡中は呼ばない。呼ぶと0になったhpがsave_hpへ上書きされてしまい、コンティニューで
        //  シーンをリロードした直後にLoadProgress()がその0を読み込んで即座に再度ゲームオーバーへ
        //  戻ってしまう。生存中の通常のシーン遷移でのみ引き継ぎを更新する)
        if (isAlive) {
            UpdateProgressVariables();
        }
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

    // 現在装備中の武器の経験値を減少させる(被ダメージ時などに使用)
    void RemoveWeaponExp(float expAmount) {
        if (weapons is null || uint(currentWeaponType) >= weapons.length()) return;

        Object@ currentWeapon = weapons[currentWeaponType];
        if (currentWeapon !is null) {
            ScriptComponent@ sc;
            if (currentWeapon.GetComponent(@sc)) {
                sc.CallMethod("RemoveExp", expAmount);
            }
        }
    }

    void IncreaseMaxHp(float amount) {
        maxHp += amount;
        hp += amount; // 最大HP増加に合わせて現在HPも回復
        Log("最大HPアップ！ 現在の最大HP: " + maxHp + " / 現在HP: " + hp);
    }
}