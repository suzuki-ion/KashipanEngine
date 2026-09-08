enum FinalBossState {
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

    [SerializeField, Tooltip("プレイヤーの位置からどれだけ通り過ぎるか")]
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

    [Header("押し戻し設定")]
    [SerializeField, Tooltip("押し戻しを行わない相手のタグ一覧")]
    array<string>@ pushBackExcludeTags;

    Box2DCollider@ col;
    CharacterController2D@ controller;

    FinalBossState state = FinalBossState::Idle;
    float stateTimer = 0.0f;

    // 現在の移動速度
    Vector3 velocity;

    // 攻撃の巡回順
    int attackIndex = 0;

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
        }

        GetComponent(@col);
        if (GetComponent(@controller)) {
            if (col !is null) {
                controller.SetSelectedCollider(col);
            }
            
            // 押し戻しの除外タグを設定
            controller.ClearIgnoredTags();
            if (pushBackExcludeTags !is null) {
                for (uint i = 0; i < pushBackExcludeTags.length(); ++i) {
                    controller.AddIgnoredTag(pushBackExcludeTags[i]);
                }
            }
        }

        // 起動直後は浮くアニメーションにしておく。
        PlayAnim(animRowFloat, animFramesFloat);
    }

    void Update() {
        // 会話中(isDialogueActive)は移動・攻撃などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        if (state == FinalBossState::Dead) return;

        FacePlayer();
        stateTimer += GetDeltaTime();

        switch (state) {
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
    }

    // 攻撃を巡回順で開始する
    void StartNextAttack() {
        switch (attackIndex) {
            case 0: ChangeState(FinalBossState::RetreatJump); break;
            case 1: ChangeState(FinalBossState::LowAttackTelegraph); break;
            case 2: ChangeState(FinalBossState::HighAttackTelegraph); break;
            case 3: ChangeState(FinalBossState::HoverCharge); break;
            default: ChangeState(FinalBossState::DashCharge); break;
        }
        attackIndex = (attackIndex + 1) % 5;
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
                if (player !is null) {
                    Transform@ pTf = player.GetTransform();
                    if (pTf !is null) needleTargetPos = pTf.GetTranslate();
                } else {
                    needleTargetPos = curPos;
                }
                break;
            }

            case FinalBossState::NeedleAttack:
                SpawnNeedle();
                break;

            case FinalBossState::LowAttackExtend:
                StartExtendAttack(lowAttack, lowAttackHeightOffset, lowAttackMaxLength);
                break;

            case FinalBossState::HighAttackExtend:
                StartExtendAttack(highAttack, highAttackHeightOffset, highAttackMaxLength);
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
                dashStartPos = curPos;
                float targetX = curPos.x;
                int dir = 1;
                if (player !is null) {
                    Transform@ pTf = player.GetTransform();
                    if (pTf !is null) {
                        targetX = pTf.GetTranslate().x;
                        dir = (targetX >= curPos.x) ? 1 : -1;
                    }
                }
                dashTargetPos = Vector3(targetX + float(dir) * dashOvershoot, curPos.y, curPos.z);
                
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

        @needleObj = GetScene().CloneObject(needle, "CloneNeedle");
        if (needleObj is null) return;

        needleObj.SetActive(true);
        Transform@ cloneTf = needleObj.GetTransform();
        if (cloneTf !is null) {
            cloneTf.SetTranslate(needleTargetPos);
        }
    }

    // 伸びる攻撃
    void StartExtendAttack(Object@ prefab, float heightOffset, float maxLength) {
        if (prefab is null) return;

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
        extendAttackBasePos = Vector3(basePos.x, basePos.y + heightOffset, basePos.z);

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

    // 伸びる攻撃の進行更新。伸長→維持が終わったらIdleへ戻る
    void UpdateExtendAttackState(float extendDuration, float holdDuration) {
        float progress = Clamp(stateTimer / extendDuration, 0.0f, 1.0f);
        UpdateExtendAttackScale(progress);

        if (stateTimer >= extendDuration + holdDuration) {
            EndExtendAttack();
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

    void End() {
    }
}