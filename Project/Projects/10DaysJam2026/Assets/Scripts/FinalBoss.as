enum FinalBossState {
    // Boss3上空に出現した直後、点滅しながら浮遊する演出
    IntroFloat,

    // 浮遊演出のあと、重力で落下してくる演出
    Intro,

    Idle,

    // 後ろに跳んでプレイヤーのいた場所に針を出す
    RetreatJump,
    NeedleTelegraph,
    NeedleAttack,

    // 下段攻撃
    LowAttackTelegraph,
    LowAttackExtend,

    // 上段攻撃
    HighAttackTelegraph,
    HighAttackExtend,

    // タメ→(ジャンプ→空中静止→落下→着地)を指定回数繰り返す
    HoverCharge,
    HoverJump,
    HoverPause,
    HoverDive,
    HoverLanding,

    // タメ→プレイヤー方向へ突進
    DashCharge,
    DashRush,

    Dead
}

class FinalBoss : ScriptComponentBehavior {
    [Header("基本設定")]

    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    [SerializeField, Tooltip("攻撃と攻撃の間の待機時間(秒)")]
    float idleDuration = 1.0f;

    [Header("登場演出(重力落下)")]

    [SerializeField, Tooltip("出現直後に「浮く」状態を維持する時間(秒)")]
    float floatDuration = 2.0f;

    [SerializeField, Tooltip("浮遊中の点滅切り替え間隔(秒)")]
    float floatBlinkInterval = 0.08f;

    [SerializeField, Tooltip("重力")]
    float gravity = 150.0f;

    [SerializeField, Tooltip("接地とみなす法線Y成分のしきい値")]
    float groundedThreshold = 0.5f;

    [Header("HP・ダメージ演出・死亡演出")]

    [SerializeField, Tooltip("最大HP")]
    float maxHp = 100.0f;

    [SerializeField, Tooltip("HP")]
    float hp = maxHp;

    [SerializeField, Tooltip("ダメージを受けた際の色変化時間(秒)")]
    float damageFlashDuration = 0.1f;

    [SerializeField, Tooltip("ダメージ時の色")]
    Vector4 damageColor = Vector4(10.0f, 10.0f, 10.0f, 1.0f);

    [SerializeField, Tooltip("通常時の色")]
    Vector4 normalColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    [SerializeField, Tooltip("死亡エフェクトのプレハブ")]
    Object@ deathEffect;

    [SerializeField, Tooltip("死亡エフェクトの発生数")]
    int deathEffectCount = 5;

    [SerializeField, Tooltip("死亡エフェクトの散らばる範囲(半径)")]
    float deathEffectSpread = 50.0f;

    [SerializeField, Tooltip("死亡エフェクトの生成間隔(秒)")]
    float deathEffectSpawnInterval = 0.15f;

    [Header("撃破後のシーン遷移")]

    [SerializeField, Tooltip("撃破演出後に画面を覆うDitherFadeを持つオブジェクト")]
    Object@ ditherFadeObject;

    [SerializeField, Tooltip("撃破演出とフェードアウトの完了後に遷移するシーン名")]
    string endCreditSceneName = "EndCredit";

    [Header("1: 後退ジャンプ+針")]

    [SerializeField, Tooltip("針のオブジェクト")]
    Object@ needle;

    [SerializeField, Tooltip("プレイヤーと逆方向へ後退する距離")]
    float retreatDistance = 80.0f;

    [SerializeField, Tooltip("後退ジャンプの所要時間(秒)")]
    float retreatJumpDuration = 0.4f;

    [SerializeField, Tooltip("後退ジャンプの最大高さ")]
    float retreatJumpHeight = 30.0f;

    [SerializeField, Tooltip("針が出現するまでの予兆時間(秒)。この間にプレイヤーが動けば回避できる")]
    float needleTelegraphDuration = 0.6f;

    [SerializeField, Tooltip("針が出現してから消えるまでの時間(秒)")]
    float needleActiveDuration = 0.5f;

    [SerializeField, Tooltip("針が出現してから伸びきるまでの時間(秒)")]
    float needleExtendDuration = 0.2f;

    [Header("2: 下段攻撃(要ジャンプ回避)")]

    [SerializeField, Tooltip("下段攻撃のオブジェクト")]
    Object@ lowAttack;

    [SerializeField, Tooltip("下段攻撃の予備動作時間(秒)")]
    float lowAttackTelegraphDuration = 0.5f;

    [SerializeField, Tooltip("下段攻撃が伸びきるまでの時間(秒)")]
    float lowAttackExtendDuration = 0.3f;

    [SerializeField, Tooltip("下段攻撃が伸びきった状態を維持する時間(秒)")]
    float lowAttackHoldDuration = 0.3f;

    [SerializeField, Tooltip("下段攻撃の最大長さ")]
    float lowAttackMaxLength = 200.0f;

    [SerializeField, Tooltip("下段攻撃の位置(Xオフセット。向きに応じて前後します)")]
    float lowAttackOffsetX = 0.0f;

    [SerializeField, Tooltip("下段攻撃の高さ(足元付近。ボス基準のYオフセット)")]
    float lowAttackHeightOffset = -10.0f;

    [Header("3: 上段攻撃(ジャンプで被弾)")]

    [SerializeField, Tooltip("上段攻撃のオブジェクト")]
    Object@ highAttack;

    [SerializeField, Tooltip("上段攻撃の予備動作時間(秒)")]
    float highAttackTelegraphDuration = 0.5f;

    [SerializeField, Tooltip("上段攻撃が伸びきるまでの時間(秒)")]
    float highAttackExtendDuration = 0.3f;

    [SerializeField, Tooltip("上段攻撃が伸びきった状態を維持する時間(秒)")]
    float highAttackHoldDuration = 0.3f;

    [SerializeField, Tooltip("上段攻撃の最大長さ")]
    float highAttackMaxLength = 200.0f;

    [SerializeField, Tooltip("上段攻撃の位置(Xオフセット。向きに応じて前後します)")]
    float highAttackOffsetX = 0.0f;

    [SerializeField, Tooltip("上段攻撃の高さ(ジャンプ中のプレイヤーに当たる高さ。ボス基準のYオフセット)")]
    float highAttackHeightOffset = 40.0f;

    [Header("4: タメ→ホバー急降下の連続")]

    [SerializeField, Tooltip("この攻撃1セットで、ジャンプ→静止→落下を何回繰り返すか")]
    int hoverRepeatCount = 4;

    [SerializeField, Tooltip("最初の1回だけ行うタメ時間(秒)")]
    float hoverChargeDuration = 0.6f;

    [SerializeField, Tooltip("上昇にかかる時間(秒)")]
    float hoverJumpDuration = 0.35f;

    [SerializeField, Tooltip("上昇する高さ")]
    float hoverHeight = 80.0f;

    [SerializeField, Tooltip("空中で静止する時間(秒)。この間のプレイヤー位置へ落下する")]
    float hoverPauseDuration = 0.4f;

    [SerializeField, Tooltip("落下にかかる時間(秒)")]
    float hoverDiveDuration = 0.3f;

    [Header("5: タメ→突進")]

    [SerializeField, Tooltip("突進前のタメ時間(秒)")]
    float dashChargeDuration = 0.6f;

    [SerializeField, Tooltip("突進にかかる時間(秒)")]
    float dashDuration = 0.4f;

    [SerializeField, Tooltip("固定突進距離 (0より大きい場合、プレイヤー位置に関係なくこの距離を突進)")]
    float dashDistance = 0.0f;

    [SerializeField, Tooltip("プレイヤーの位置からどれだけ通り過ぎるか (dashDistanceが0の時のみ使用)")]
    float dashOvershoot = 30.0f;

    [Header("着地")]

    [SerializeField, Tooltip("ホバー急降下で着地してから次の行動に移るまでの時間(秒)")]
    float hoverLandDuration = 0.15f;

    [Header("アニメーション(行番号・コマ数)")]

    [SerializeField, Tooltip("「何もしない」の行番号/コマ数")]
    int animRowIdle = 0;
    [SerializeField]
    int animFramesIdle = 4;

    [SerializeField, Tooltip("「攻撃(上段)」の行番号/コマ数")]
    int animRowAttackHigh = 1;
    [SerializeField]
    int animFramesAttackHigh = 9;

    [SerializeField, Tooltip("「攻撃(下段)」の行番号/コマ数")]
    int animRowAttackLow = 2;
    [SerializeField]
    int animFramesAttackLow = 9;

    [SerializeField, Tooltip("「タメ」の行番号/コマ数")]
    int animRowCharge = 3;
    [SerializeField]
    int animFramesCharge = 2;

    [SerializeField, Tooltip("「ジャンプ(上昇)」の行番号/コマ数")]
    int animRowJumpUp = 4;
    [SerializeField]
    int animFramesJumpUp = 1;

    [SerializeField, Tooltip("「ジャンプ(下降)」の行番号/コマ数")]
    int animRowJumpDown = 5;
    [SerializeField]
    int animFramesJumpDown = 1;

    [SerializeField, Tooltip("「着地」の行番号/コマ数")]
    int animRowLanding = 6;
    [SerializeField]
    int animFramesLanding = 2;

    [SerializeField, Tooltip("「突進」の行番号/コマ数")]
    int animRowDash = 7;
    [SerializeField]
    int animFramesDash = 3;

    [SerializeField, Tooltip("「浮く」の行番号/コマ数")]
    int animRowFloat = 8;
    [SerializeField]
    int animFramesFloat = 1;

    [SerializeField, Tooltip("「死亡」の行番号/コマ数")]
    int animRowDead = 9;
    [SerializeField]
    int animFramesDead = 1;

    [Header("押し戻し設定")]
    [SerializeField, Tooltip("押し戻しを行わない相手のタグ一覧")]
    array<string>@ pushBackExcludeTags;

    Box2DCollider@ col;
    CharacterController2D@ controller;
    SpriteRenderer@ sprite;

    // ダメージ演出管理用
    float damageFlashTimer = 0.0f;
    bool isFlashing = false;

    // 死亡演出管理用
    bool isAnimation = false;
    int spawnedEffectCount = 0;
    float deathEffectTimer = 0.0f;
    array<Object@> cloneEffects;

    ScriptComponent@ ditherFadeScript;
    bool isEndCreditTransitionPending = false;
    bool hasStartedEndCreditTransition = false;
    int endCreditTransitionWaitFrames = 0;

    FinalBossState state = FinalBossState::Idle;
    float stateTimer = 0.0f;

    // 現在の移動速度
    Vector3 velocity;

    // 攻撃の抽選(バッグ方式): 未使用の攻撃番号を入れておき、引くたびに取り除く
    array<int> attackBag;

    // 直前に選ばれた攻撃(バッグを補充した直後に同じ攻撃が連続しないようにするため)
    int lastAttackIndex = -1;

    // 疑似乱数(Boss3と同じ方式)
    uint rngState = 54321;

    // 下段攻撃セット枠が選ばれた際、下段の終了後に上段へ直接つなげるためのフラグ
    bool isLowHighCombo = false;

    Vector3 initialBossPos;

    // 後退ジャンプ+針
    Vector3 retreatStartPos;
    Vector3 retreatTargetPos;
    Vector3 needleTargetPos;
    Object@ needleObj;
    bool retreatIsDescending = false; // 後退ジャンプ中、下降に転じたらジャンプ(下降)アニメへ切り替える

    // 伸びる攻撃
    Object@ currentExtendAttackObj;
    Vector3 extendAttackBasePos;
    float extendAttackMaxLength = 0.0f;
    int extendAttackDirection = 1;

    // ホバー急降下
    int hoverRepeatIndex = 0;
    Vector3 hoverStartPos;
    Vector3 hoverApexPos;
    Vector3 hoverDiveStartPos;
    Vector3 hoverDiveTargetPos;

    // 突進
    Vector3 dashStartPos;
    Vector3 dashTargetPos;

    void Start() {
        Transform@ tf = GetTransform();
        if (tf !is null) {
            initialBossPos = tf.GetTranslate();

            // ボスの座標を種として乱数にばらつきを持たせる
            rngState = uint(Abs(initialBossPos.x) * 1000.0f) + uint(Abs(initialBossPos.y) * 37.0f) + 54321;
            if (rngState == 0) rngState = 54321;
        }

        GetComponent(@col);
        GetComponent(@sprite);
        if (ditherFadeObject !is null) {
            ditherFadeObject.GetComponent(@ditherFadeScript);
        }
        if (sprite !is null) {
            sprite.SetInstanceColor(normalColor);
        }
        if (GetComponent(@controller)) {
            if (col !is null) {
                controller.SetSelectedCollider(col);
            }
            controller.SetGroundedThreshold(groundedThreshold);

            // 押し戻しの除外タグを設定
            controller.ClearIgnoredTags();
            if (pushBackExcludeTags !is null) {
                for (uint i = 0; i < pushBackExcludeTags.length(); ++i) {
                    controller.AddIgnoredTag(pushBackExcludeTags[i]);
                }
            }
        }

        // 起動直後はBoss3の上空で点滅しながら浮遊し、その後重力で落下してくる
        ChangeState(FinalBossState::IntroFloat);

        // セーブ済みの撃破状態を復元する(Chest.as等と同じ方式)
        if (ShouldPersistProgress()) {
            bool hasLoadedSaveThisSession = false;
            GetScene().GetGlobalVariable("hasLoadedSaveThisSession", hasLoadedSaveThisSession);
            if (!hasLoadedSaveThisSession) {
                GetScene().LoadGlobalVariables();
                GetScene().SetGlobalVariable("hasLoadedSaveThisSession", true);
            }

            bool savedDefeated = false;
            if (GetScene().GetGlobalVariable(GetSaveKey(), savedDefeated) && savedDefeated) {
                hp = 0.0f;
                state = FinalBossState::Dead;
                GetOwnerObject().SetComponentsActiveExceptTransformAndScript(false);
            }
        }

        GetScene().SetVariable("BossHPUI_HP", hp);
        GetScene().SetVariable("BossHPUI_MaxHP", maxHp);
    }

    bool ShouldPersistProgress() const {
        return GetScene().IsPlaying() || !IsEditorBuild();
    }

    // シーン名+オブジェクトのUUIDを使ったセーブデータキー(Chest.as等と同じ方式)
    string GetSaveKey() const {
        return "boss_" + GetScene().GetName() + "_" + GetOwnerObject().GetUUID() + "_defeated";
    }

    void Update() {
        if (isEndCreditTransitionPending) {
            UpdateEndCreditTransition();
            return;
        }

        // 会話中(isDialogueActive)は移動・攻撃などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        // ダメージ時の色点滅処理
        if (isFlashing) {
            damageFlashTimer -= GetDeltaTime();
            if (damageFlashTimer <= 0.0f) {
                isFlashing = false;
                if (sprite !is null) {
                    sprite.SetInstanceColor(normalColor);
                }
            }
        }

        // HPが尽きたら死亡状態へ移行
        if (state != FinalBossState::Dead && hp <= 0.0f) {
            ChangeState(FinalBossState::Dead);
        }

        if (state == FinalBossState::Dead) {
            UpdateDeathAnimation();
            return;
        }

        // プレイヤーが後ろに回り込んでも向きを追従させない
        if (state == FinalBossState::Idle || state == FinalBossState::Intro || state == FinalBossState::IntroFloat || 
            state == FinalBossState::HoverCharge || state == FinalBossState::HoverDive || state == FinalBossState::HoverJump || state == FinalBossState::HoverLanding || state == FinalBossState::HoverPause) {
            FacePlayer();
        }
        stateTimer += GetDeltaTime();

        switch (state) {
            case FinalBossState::IntroFloat:
                UpdateIntroFloat();
                break;

            case FinalBossState::Intro:
                UpdateIntro();
                break;

            case FinalBossState::Idle:
                if (stateTimer >= idleDuration) {
                    StartNextAttack();
                }
                break;

            // 後退ジャンプ+針
            case FinalBossState::RetreatJump:
                UpdateRetreatJump();
                break;

            case FinalBossState::NeedleTelegraph:
                if (stateTimer >= needleTelegraphDuration) {
                    ChangeState(FinalBossState::NeedleAttack);
                }
                break;

            case FinalBossState::NeedleAttack:
                // 針が伸びきる時間に達した瞬間にコライダーを有効化する
                if (needleObj !is null && stateTimer >= needleExtendDuration && (stateTimer - GetDeltaTime()) < needleExtendDuration) {
                    Box2DCollider@ needleCol;
                    if (needleObj.GetComponent(@needleCol)) {
                        needleCol.SetActive(true);
                    }
                }

                if (stateTimer >= needleActiveDuration) {
                    if (needleObj !is null) {
                        needleObj.SetActive(false);
                        @needleObj = null;
                    }
                    ChangeState(FinalBossState::Idle);
                }
                break;

            // 下段攻撃
            case FinalBossState::LowAttackTelegraph:
                if (stateTimer >= lowAttackTelegraphDuration) {
                    ChangeState(FinalBossState::LowAttackExtend);
                }
                break;

            case FinalBossState::LowAttackExtend:
                UpdateExtendAttackState(lowAttackExtendDuration, lowAttackHoldDuration);
                break;

            // 上段攻撃
            case FinalBossState::HighAttackTelegraph:
                if (stateTimer >= highAttackTelegraphDuration) {
                    ChangeState(FinalBossState::HighAttackExtend);
                }
                break;

            case FinalBossState::HighAttackExtend:
                UpdateExtendAttackState(highAttackExtendDuration, highAttackHoldDuration);
                break;

            // タメ→ホバー急降下の連続
            case FinalBossState::HoverCharge:
                if (stateTimer >= hoverChargeDuration) {
                    ChangeState(FinalBossState::HoverJump);
                }
                break;

            case FinalBossState::HoverJump:
                UpdateHoverJump();
                break;

            case FinalBossState::HoverPause:
                if (stateTimer >= hoverPauseDuration) {
                    ChangeState(FinalBossState::HoverDive);
                }
                break;

            case FinalBossState::HoverDive:
                UpdateHoverDive();
                break;

            case FinalBossState::HoverLanding:
                if (stateTimer >= hoverLandDuration) {
                    if (hoverRepeatIndex < hoverRepeatCount) {
                        ChangeState(FinalBossState::HoverJump); // 2回目以降はタメなしで再ジャンプ
                    } else {
                        ChangeState(FinalBossState::Idle);
                    }
                }
                break;

            // タメ→突進
            case FinalBossState::DashCharge:
                if (stateTimer >= dashChargeDuration) {
                    ChangeState(FinalBossState::DashRush);
                }
                break;

            case FinalBossState::DashRush:
                UpdateDashRush();
                break;
        }

        // 明示的にY軸の軌道を制御しているステート(登場演出・後退ジャンプ・ホバー攻撃の上昇/静止/落下)
        // 以外は常に重力を適用し、実際に立っている地面や足場の高さに追従させる。
        // これにより、空中の足場に着地した後に突進などで移動し、足場から外れた場合でも
        // ボスが空中に浮いたまま移動し続けることがなくなる。
        if (!UsesManualVerticalMotion(state)) {
            ApplyGravity();
        }

        // 毎フレームの速度による移動処理
        if (controller !is null) {
            controller.Move(Vector2(velocity.x * GetDeltaTime(), velocity.y * GetDeltaTime()));
        } else {
            // controllerがない場合の簡易フォールバック
            Transform@ tf = GetTransform();
            if (tf !is null) {
                tf.SetTranslate(tf.GetTranslate() + velocity * GetDeltaTime());
            }
        }

        GetScene().SetVariable("BossHPUI_HP", hp);
    }

    // 攻撃をバッグ方式でランダムに選ぶ(4種類を1周ぶんシャッフルして並べ、使い切ったら再抽選する)
    // ※「下段攻撃→上段攻撃」は1つのセット枠として扱い、必ずこの順番でセットで実行される
    void StartNextAttack() {
        if (attackBag.length() == 0) {
            RefillAttackBag();
        }

        // バッグの末尾から1つ取り出す
        uint lastIdx = attackBag.length() - 1;
        int nextAttack = attackBag[lastIdx];
        attackBag.removeAt(lastIdx);
        lastAttackIndex = nextAttack;

        switch (nextAttack) {
            case 0: ChangeState(FinalBossState::RetreatJump); break;
            case 1:
                // 下段→上段のセット枠。まず下段を開始し、下段終了後に上段へ直接つなげる
                isLowHighCombo = true;
                ChangeState(FinalBossState::LowAttackTelegraph);
                break;
            case 2: ChangeState(FinalBossState::HoverCharge); break;
            default: ChangeState(FinalBossState::DashCharge); break;
        }
    }

    // 攻撃番号(0〜3)を1つずつ入れたバッグをシャッフルして補充する
    // 0:後退ジャンプ+針 1:下段→上段セット 2:ホバー急降下 3:突進
    void RefillAttackBag() {
        attackBag.resize(0);
        for (int i = 0; i < 4; i++) {
            attackBag.insertLast(i);
        }

        // Fisher-Yatesシャッフル
        for (int i = int(attackBag.length()) - 1; i > 0; i--) {
            int j = NextRandomInt(i + 1);
            int tmp = attackBag[i];
            attackBag[i] = attackBag[j];
            attackBag[j] = tmp;
        }

        // バッグ補充直後、先頭(=次に引かれる末尾ではなく次回以降に引かれる側)が
        // 直前の攻撃と同じだと2連続してしまうため、末尾要素と入れ替えて回避する
        uint lastIdx = attackBag.length() - 1;
        if (lastAttackIndex >= 0 && attackBag.length() > 1 && attackBag[lastIdx] == lastAttackIndex) {
            uint swapIndex = uint(NextRandomInt(int(lastIdx))); // 0〜lastIdx-1のいずれか
            int tmp = attackBag[lastIdx];
            attackBag[lastIdx] = attackBag[swapIndex];
            attackBag[swapIndex] = tmp;
        }
    }

    // 0.0〜1.0未満の疑似乱数を返す
    float NextRandom01() {
        rngState = rngState * 1664525 + 1013904223;
        return float(rngState % 100000) / 100000.0f;
    }

    // 0〜maxExclusive-1の整数乱数を返す
    int NextRandomInt(int maxExclusive) {
        if (maxExclusive <= 1) return 0;
        int v = int(NextRandom01() * float(maxExclusive));
        if (v >= maxExclusive) v = maxExclusive - 1;
        if (v < 0) v = 0;
        return v;
    }

    void ChangeState(FinalBossState newState) {
        state = newState;
        stateTimer = 0.0f;
        velocity = Vector3(0.0f, 0.0f, 0.0f); // 状態遷移時に一旦速度をリセット

        PlayAnimForState(state);

        Transform@ tf = GetTransform();
        Vector3 curPos = (tf !is null) ? tf.GetTranslate() : Vector3(0.0f, 0.0f, 0.0f);

        switch (state) {
            case FinalBossState::RetreatJump: {
                retreatStartPos = curPos;
                retreatIsDescending = false;
                int dir = 1;
                if (player !is null) {
                    Transform@ pTf = player.GetTransform();
                    if (pTf !is null && pTf.GetTranslate().x > curPos.x) dir = -1; // プレイヤーと逆方向へ
                }
                retreatTargetPos = Vector3(curPos.x + float(dir) * retreatDistance, curPos.y, curPos.z);
                
                // X軸の等速移動速度を設定
                velocity.x = (retreatTargetPos.x - retreatStartPos.x) / retreatJumpDuration;
                break;
            }

            case FinalBossState::NeedleTelegraph: {
                // 棘のY座標は「現在のプレイヤーY」ではなく、
                // Player.asが記録している「最後に実際に接地していたY」を使う。
                //
                // これにより、
                //   ・プレイヤーがジャンプ中
                //   ・落下中
                //   ・高い足場の上からジャンプしている
                // といった状況でも、棘が空中に生成されず、その足場の高さへ出せる。
                //
                // PlayerGroundYがまだ取得できない場合だけ、ボスの初期地面高へフォールバックする。
                float groundY = initialBossPos.y;
                bool hasGroundY = GetScene().GetVariable("PlayerGroundY", groundY);
                if (!hasGroundY) {
                    groundY = initialBossPos.y;
                }

                needleTargetPos = Vector3(curPos.x, groundY, curPos.z);

                if (player !is null) {
                    Transform@ pTf = player.GetTransform();
                    if (pTf !is null) {
                        Vector3 playerPos = pTf.GetTranslate();

                        // Xだけは予兆開始時点のプレイヤー位置を使う。
                        // Yは常に最後の接地高さへ固定する。
                        needleTargetPos = Vector3(playerPos.x, groundY, playerPos.z);
                    }
                }
                break;
            }

            case FinalBossState::NeedleAttack:
                SpawnNeedle();
                break;

            case FinalBossState::LowAttackExtend:
                StartExtendAttack(lowAttack, lowAttackOffsetX, lowAttackHeightOffset, lowAttackMaxLength);
                break;

            case FinalBossState::HighAttackExtend:
                StartExtendAttack(highAttack, highAttackOffsetX, highAttackHeightOffset, highAttackMaxLength);
                break;

            case FinalBossState::HoverCharge:
                hoverRepeatIndex = 0;
                break;

            case FinalBossState::HoverJump:
                hoverStartPos = curPos;
                hoverApexPos = Vector3(curPos.x, curPos.y + hoverHeight, curPos.z);
                
                // 上昇速度を設定
                velocity.y = hoverHeight / hoverJumpDuration;
                break;

            case FinalBossState::HoverDive: {
                PlayTaggedAudio("Fall");
                hoverDiveStartPos = curPos;
                Vector3 targetPos = curPos;
                if (player !is null) {
                    Transform@ pTf = player.GetTransform();
                    if (pTf !is null) targetPos = pTf.GetTranslate();
                }
                // 着地の高さはボスの初期(地面)高さを基準にする
                hoverDiveTargetPos = Vector3(targetPos.x, initialBossPos.y, targetPos.z);
                
                // 落下先へ向かう速度を設定
                velocity.x = (hoverDiveTargetPos.x - hoverDiveStartPos.x) / hoverDiveDuration;
                velocity.y = (hoverDiveTargetPos.y - hoverDiveStartPos.y) / hoverDiveDuration;
                break;
            }

            case FinalBossState::DashRush: {
                PlayTaggedAudio("Dash");
                dashStartPos = curPos;
                int dir = 1;
                if (player !is null) {
                    Transform@ pTf = player.GetTransform();
                    if (pTf !is null && pTf.GetTranslate().x < curPos.x) dir = -1;
                }

                float targetX = curPos.x;
                // 固定突進距離が設定されている場合
                if (dashDistance > 0.0f) {
                    targetX = curPos.x + float(dir) * dashDistance;
                } else {
                    // プレイヤー位置を基準にする場合
                    if (player !is null) {
                        Transform@ pTf = player.GetTransform();
                        if (pTf !is null) targetX = pTf.GetTranslate().x;
                    }
                    targetX += float(dir) * dashOvershoot;
                }
                
                dashTargetPos = Vector3(targetX, curPos.y, curPos.z);
                
                // 突進速度を設定
                velocity.x = (dashTargetPos.x - dashStartPos.x) / dashDuration;
                break;
            }

            default:
                break;
        }
    }

    void FacePlayer() {
        Transform@ tf = GetTransform();
        if (player is null || tf is null) return;
        Transform@ playerTf = player.GetTransform();
        if (playerTf is null) return;

        if (playerTf.GetTranslate().x > tf.GetTranslate().x) {
            tf.SetRotate(Vector3(0.0f, 3.14159f, 0.0f));
        } else {
            tf.SetRotate(Vector3(0.0f, 0.0f, 0.0f));
        }
    }

    // プレイヤーがジャンプ中(接地していない)かどうかを、プレイヤー側のCharacterController2Dから判定する
    // コントローラが取得できない場合は判定できないため、地面にいるものとして扱う
    bool IsPlayerGrounded() {
        if (player is null) return true;
        CharacterController2D@ playerController;
        if (player.GetComponent(@playerController)) {
            return playerController.IsGrounded();
        }
        return true;
    }

    // Y軸の動きをステート側で明示的に計算しているかどうか
    // (登場演出の浮遊・落下、後退ジャンプの放物線、ホバー攻撃の上昇/静止/落下)。
    // これらのステート中はvelocity.yを自前で設定しているため、重力を二重に適用しない
    bool UsesManualVerticalMotion(FinalBossState s) {
        return s == FinalBossState::IntroFloat
            || s == FinalBossState::Intro
            || s == FinalBossState::RetreatJump
            || s == FinalBossState::HoverJump
            || s == FinalBossState::HoverPause
            || s == FinalBossState::HoverDive;
    }

    // 重力を適用し、接地していれば下向きの速度を0にクランプする
    // (足場や地面から外れた際に、実際に立っている高さへ自然に落下させるための共通処理)
    void ApplyGravity() {
        if (controller is null) return;
        velocity.y -= gravity * GetDeltaTime();
        if (controller.IsGrounded() && velocity.y < 0.0f) {
            velocity.y = 0.0f;
        }
    }

    // 後退ジャンプ
    void UpdateRetreatJump() {
        float progress = Clamp(stateTimer / retreatJumpDuration, 0.0f, 1.0f);
        
        // Y軸の速度を放物線(サイン波の微分=コサイン波)で計算して適用
        velocity.y = (3.14159f * retreatJumpHeight / retreatJumpDuration) * Cos(3.14159f * progress);

        // 軌道の折り返し(上昇→下降)に合わせてアニメーションを切り替える
        if (!retreatIsDescending && progress >= 0.5f) {
            retreatIsDescending = true;
            PlayAnim(animRowJumpDown, animFramesJumpDown);
        }

        if (progress >= 1.0f) {
            ChangeState(FinalBossState::NeedleTelegraph);
        }
    }

    void SpawnNeedle() {
        if (needle is null) return;

        PlayTaggedAudio("Needle");

        @needleObj = GetScene().CloneObject(needle, "CloneNeedle");
        if (needleObj is null) return;

        needleObj.SetActive(true);
        Transform@ cloneTf = needleObj.GetTransform();
        if (cloneTf !is null) {
            cloneTf.SetTranslate(needleTargetPos);
        }

        // 生成直後はコライダーを無効化しておく
        Box2DCollider@ needleCol;
        if (needleObj.GetComponent(@needleCol)) {
            needleCol.SetActive(false);
        }
    }

    // 伸びる攻撃
    void StartExtendAttack(Object@ prefab, float offsetX, float heightOffset, float maxLength) {
        if (prefab is null) return;

        PlayTaggedAudio("Extend");

        Transform@ tf = GetTransform();
        if (tf is null) return;
        Vector3 basePos = tf.GetTranslate();

        extendAttackDirection = 1;
        if (player !is null) {
            Transform@ pTf = player.GetTransform();
            if (pTf !is null && pTf.GetTranslate().x < basePos.x) extendAttackDirection = -1;
        }

        @currentExtendAttackObj = GetScene().CloneObject(prefab, "CloneExtendAttack");
        if (currentExtendAttackObj is null) return;
        currentExtendAttackObj.SetActive(true);

        extendAttackMaxLength = maxLength;
        // XオフセットとYオフセットを考慮したベース位置を算出
        extendAttackBasePos = Vector3(basePos.x + float(extendAttackDirection) * offsetX, basePos.y + heightOffset, basePos.z);

        UpdateExtendAttackScale(0.0f);
    }

    void UpdateExtendAttackScale(float progress) {
        if (currentExtendAttackObj is null) return;
        Transform@ cloneTf = currentExtendAttackObj.GetTransform();
        if (cloneTf is null) return;

        float length = extendAttackMaxLength * progress;
        cloneTf.SetScale(Vector3(length, 1.0f, 1.0f));

        float centerX = extendAttackBasePos.x + float(extendAttackDirection) * (length * 0.5f);
        cloneTf.SetTranslate(Vector3(centerX, extendAttackBasePos.y, extendAttackBasePos.z));
    }

    void EndExtendAttack() {
        if (currentExtendAttackObj !is null) {
            currentExtendAttackObj.SetActive(false);
            @currentExtendAttackObj = null;
        }
    }

    // 伸びる攻撃の進行更新。伸長→維持が終わったら次のステートへ
    // (下段→上段セットの下段側が終わった場合は、Idleを挟まず上段の予備動作へ直接つなげる)
    void UpdateExtendAttackState(float extendDuration, float holdDuration) {
        float progress = Clamp(stateTimer / extendDuration, 0.0f, 1.0f);
        UpdateExtendAttackScale(progress);

        if (stateTimer >= extendDuration + holdDuration) {
            EndExtendAttack();

            if (state == FinalBossState::LowAttackExtend && isLowHighCombo) {
                ChangeState(FinalBossState::HighAttackTelegraph);
            } else {
                isLowHighCombo = false;
                ChangeState(FinalBossState::Idle);
            }
        }
    }

    // 登場演出:出現直後にfloatDuration秒だけ点滅しながら浮遊し、その後落下(Intro)へ移行する
    void UpdateIntroFloat() {
        // blinkInterval毎にスプライトの表示・非表示を交互に切替
        bool isVisible = (int(stateTimer / floatBlinkInterval) % 2 == 0);
        if (sprite !is null) {
            sprite.SetActive(isVisible);
        }

        if (stateTimer >= floatDuration) {
            // 点滅終了。必ず表示状態に戻してから落下ステートへ
            if (sprite !is null) {
                sprite.SetActive(true);
            }
            ChangeState(FinalBossState::Intro);
        }
    }

    // 登場演出:Boss3の上空から重力で落下し、接地したらIdleへ移行する
    void UpdateIntro() {
        velocity.y -= gravity * GetDeltaTime();

        if (controller !is null && controller.IsGrounded() && velocity.y <= 0.0f) {
            velocity.y = 0.0f;
            velocity.x = 0.0f;

            // 以降のホバー攻撃などが参照する「地面の高さ」を、実際に着地した位置で更新する
            Transform@ tf = GetTransform();
            if (tf !is null) {
                initialBossPos = tf.GetTranslate();
            }

            ChangeState(FinalBossState::Idle);
        }
    }

    // タメ→ホバー急降下の連続
    void UpdateHoverJump() {
        // 座標指定をやめ、時間経過のみを管理
        if (stateTimer >= hoverJumpDuration) {
            ChangeState(FinalBossState::HoverPause);
        }
    }

    void UpdateHoverDive() {
        if (stateTimer >= hoverDiveDuration) {
            hoverRepeatIndex++;
            ChangeState(FinalBossState::HoverLanding); // 着地後、次のジャンプへ進むかIdleへ戻るかはHoverLanding側で判定する
        }
    }

    // タメ→突進
    void UpdateDashRush() {
        if (stateTimer >= dashDuration) {
            ChangeState(FinalBossState::Idle);
        }
    }

    float LerpF(float a, float b, float t) {
        return a + (b - a) * t;
    }

    // ステートに応じたアニメーションを再生する(ChangeStateの中で1回だけ呼ばれる)
    void PlayAnimForState(FinalBossState s) {
        switch (s) {
            case FinalBossState::IntroFloat:
                PlayAnim(animRowFloat, animFramesFloat);
                break;

            case FinalBossState::Intro:
                PlayAnim(animRowJumpDown, animFramesJumpDown); // 落下中は降下ポーズを流用
                break;

            case FinalBossState::Idle:
                PlayAnim(animRowIdle, animFramesIdle);
                break;

            case FinalBossState::RetreatJump:
                PlayAnim(animRowJumpUp, animFramesJumpUp); // 下降への切り替えはUpdateRetreatJump内で行う
                break;

            case FinalBossState::NeedleAttack:
                // 針そのものの演出は針オブジェクト側で行うため、ボス本体はニュートラルな状態のままにする
                PlayAnim(animRowIdle, animFramesIdle);
                break;

            case FinalBossState::HoverCharge:
            case FinalBossState::DashCharge:
                PlayAnim(animRowCharge, animFramesCharge);
                break;

            case FinalBossState::LowAttackExtend:
                PlayAnim(animRowAttackLow, animFramesAttackLow);
                break;

            case FinalBossState::HighAttackExtend:
                PlayAnim(animRowAttackHigh, animFramesAttackHigh);
                break;

            case FinalBossState::HoverJump:
                PlayAnim(animRowJumpUp, animFramesJumpUp);
                break;

            case FinalBossState::HoverPause:
                PlayAnim(animRowFloat, animFramesFloat);
                break;

            case FinalBossState::HoverDive:
                PlayAnim(animRowJumpDown, animFramesJumpDown);
                break;

            case FinalBossState::HoverLanding:
                PlayAnim(animRowLanding, animFramesLanding);
                break;

            case FinalBossState::DashRush:
                PlayAnim(animRowDash, animFramesDash);
                break;

            case FinalBossState::Dead:
                PlayAnim(animRowDead, animFramesDead);
                break;

            default:
                break;
        }
    }

    // AnimatorSCタグの付いたSpriteAnimatorへ、指定した行・コマ数を反映する
    void PlayAnim(int row, int frameCount) {
        array<ScriptComponent@>@ scripts;
        if (GetComponents(@scripts)) {
            for (int i = 0; i < scripts.length(); ++i) {
                if (scripts[i].GetTag() == "AnimatorSC") {
                    scripts[i].CallMethod("PlayRow", row);
                    scripts[i].CallMethod("SetFrameCount", frameCount);
                }
            }
        }
    }

    void PlayTaggedAudio(const string &in tagName) {
        // Start()時点のキャッシュだと、クローン直後同フレームで呼ばれた場合等に
        // まだ取得できていないことがあるため、呼び出す都度取得し直す
        array<AudioSource@>@ sources;
        if (!GetComponents(@sources)) return;
        for(uint i = 0; i < sources.length(); ++i) {
            if(sources[i] !is null && sources[i].GetTag() == tagName) {
                sources[i].SetActive(true);
                sources[i].Play();
            }
        }
    }

    // ダメージを受けた際の処理(弾などのOnCollisionEnter側から CallMethod("Damage", amount) で呼ばれる想定)
    void Damage(float amount) {
        if (state == FinalBossState::Dead) return;
        hp -= amount;
        PlayTaggedAudio("Damage");

        // ダメージ演出の開始(色フラッシュ)
        isFlashing = true;
        damageFlashTimer = damageFlashDuration;
        if (sprite !is null) {
            sprite.SetInstanceColor(damageColor);
        }
    }

    // 死亡演出(死亡エフェクトを円状に順番に生成し、全て終わったらボスの姿を消す)
    void UpdateDeathAnimation() {
        if (!isAnimation) {
            isAnimation = true;
            deathEffectTimer = 0.0f;
            spawnedEffectCount = 0; // 生成数の初期化
            PlayTaggedAudio("Dead");
        }

        deathEffectTimer += GetDeltaTime();

        // エフェクトを順番に生成
        if (spawnedEffectCount < deathEffectCount) {
            // 経過時間が生成タイミングに達したら生成
            if (deathEffectTimer >= float(spawnedEffectCount) * deathEffectSpawnInterval) {
                Transform@ tf = GetTransform();
                Vector3 centerPos = (tf !is null) ? tf.GetTranslate() : initialBossPos;

                Object@ clone = GetScene().CloneObject(deathEffect, "CloneDeathEffect_" + spawnedEffectCount);
                if (clone !is null) {
                    Transform@ cloneTf = clone.GetTransform();
                    if (cloneTf !is null) {
                        cloneTf.SetScale(Vector3(16.0f, 16.0f, 1.0f));
                    }

                    ScriptComponent@ sc;
                    if (clone.GetComponent(@sc)) {
                        sc.CallMethod("StartAnimation");

                        float angle = 6.28318f * (float(spawnedEffectCount) / float(deathEffectCount));
                        float r = deathEffectSpread * (spawnedEffectCount % 2 == 0 ? 1.0f : 0.6f);

                        Vector3 spawnPos = Vector3(
                            centerPos.x + Cos(angle) * r,
                            centerPos.y + Sin(angle) * r,
                            centerPos.z - 0.1f
                        );

                        sc.SetVariable("pos", spawnPos);
                    }
                    cloneEffects.insertLast(clone);
                }
                spawnedEffectCount++;
            }
        }

        // アニメーション更新処理
        bool allEffectsFinished = true;
        if (cloneEffects.length() > 0) {
            float deathEffectDuration = 0.6f;

            for (uint i = 0; i < cloneEffects.length(); i++) {
                Object@ clone = cloneEffects[i];
                if (clone !is null && clone.IsActive()) {
                    ScriptComponent@ sc;
                    if (clone.GetComponent(@sc)) {
                        sc.CallMethod("UpdateAnimation");
                    }

                    // 各エフェクトが生成されてからの経過時間
                    float effectAge = deathEffectTimer - (float(i) * deathEffectSpawnInterval);

                    // アニメーションが終了したら非アクティブ化
                    if (effectAge >= deathEffectDuration) {
                        clone.SetActive(false);
                    } else {
                        allEffectsFinished = false; // まだ終わっていないエフェクトがある
                    }
                }
            }
        } else {
            allEffectsFinished = false; // まだ1つも生成されていない
        }

        // 全てのエフェクトが生成され、かつ全て終了した場合にボスの姿を消す
        if (spawnedEffectCount >= deathEffectCount && allEffectsFinished) {
            GetOwnerObject().SetComponentsActiveExceptTransformAndScript(false);

            // 撃破状態をセーブデータへ反映する(実際のファイル書き込みはSavePoint経由)
            if (ShouldPersistProgress()) {
                GetScene().SetGlobalVariable(GetSaveKey(), true);
            }

            BeginEndCreditTransition();
        }
    }

    void BeginEndCreditTransition() {
        if (hasStartedEndCreditTransition) return;
        hasStartedEndCreditTransition = true;

        if (ditherFadeScript !is null && ditherFadeScript.CallMethod("FadeOut")) {
            isEndCreditTransitionPending = true;
            endCreditTransitionWaitFrames = 0;
            return;
        }

        CompleteEndCreditTransition();
    }

    // FadeOut()を呼んだフレームの更新順に依存しないよう、最低1フレーム待って完了を確認する。
    void UpdateEndCreditTransition() {
        ++endCreditTransitionWaitFrames;

        if (ditherFadeScript is null) {
            CompleteEndCreditTransition();
            return;
        }

        bool isFading = false;
        if (!ditherFadeScript.CallMethod("IsFading") || !ditherFadeScript.GetLastReturnValue(isFading)) {
            CompleteEndCreditTransition();
            return;
        }

        if (!isFading && endCreditTransitionWaitFrames >= 2) {
            CompleteEndCreditTransition();
        }
    }

    void CompleteEndCreditTransition() {
        isEndCreditTransitionPending = false;

        Scene@ scene = GetScene();
        if (scene is null || endCreditSceneName.length() == 0) return;

        scene.SetNextSceneName(endCreditSceneName);
        scene.ChangeToNextScene();
    }

    void End() {
    }
}
