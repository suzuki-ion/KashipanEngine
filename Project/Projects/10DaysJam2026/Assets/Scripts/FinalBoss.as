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

    // タメ→(ジャンプ→空中静止→落下)を指定回数繰り返す
    HoverCharge,
    HoverJump,
    HoverPause,
    HoverDive,

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

    [SerializeField, Tooltip("針のプレハブ")]
    Object@ needlePrefab;

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

    [SerializeField, Tooltip("下段攻撃のプレハブ")]
    Object@ lowAttackPrefab;

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

    [SerializeField, Tooltip("上段攻撃のプレハブ")]
    Object@ highAttackPrefab;

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

    FinalBossState state = FinalBossState::Idle;
    float stateTimer = 0.0f;

    // 攻撃の巡回順
    int attackIndex = 0;

    Vector3 initialBossPos;

    // 後退ジャンプ+針
    Vector3 retreatStartPos;
    Vector3 retreatTargetPos;
    Vector3 needleTargetPos;
    Object@ needleObj;

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

        Transform@ tf = GetTransform();
        Vector3 curPos = (tf !is null) ? tf.GetTranslate() : Vector3(0.0f, 0.0f, 0.0f);

        switch (state) {
            case FinalBossState::RetreatJump: {
                retreatStartPos = curPos;
                int dir = 1;
                if (player !is null) {
                    Transform@ pTf = player.GetTransform();
                    if (pTf !is null && pTf.GetTranslate().x > curPos.x) dir = -1; // プレイヤーと逆方向へ
                }
                retreatTargetPos = Vector3(curPos.x + float(dir) * retreatDistance, curPos.y, curPos.z);
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
                StartExtendAttack(lowAttackPrefab, lowAttackHeightOffset, lowAttackMaxLength);
                break;

            case FinalBossState::HighAttackExtend:
                StartExtendAttack(highAttackPrefab, highAttackHeightOffset, highAttackMaxLength);
                break;

            case FinalBossState::HoverCharge:
                hoverRepeatIndex = 0;
                break;

            case FinalBossState::HoverJump:
                hoverStartPos = curPos;
                hoverApexPos = Vector3(curPos.x, curPos.y + hoverHeight, curPos.z);
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
        Transform@ tf = GetTransform();
        if (tf is null) return;

        float progress = Clamp(stateTimer / retreatJumpDuration, 0.0f, 1.0f);
        float x = LerpF(retreatStartPos.x, retreatTargetPos.x, progress);
        float y = retreatStartPos.y + Sin(progress * 3.14159f) * retreatJumpHeight; // 山なりの軌道

        tf.SetTranslate(Vector3(x, y, retreatStartPos.z));

        if (progress >= 1.0f) {
            ChangeState(FinalBossState::NeedleTelegraph);
        }
    }

    void SpawnNeedle() {
        if (needlePrefab is null) return;

        @needleObj = GetScene().CloneObject(needlePrefab, "CloneNeedle");
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
        Transform@ tf = GetTransform();
        if (tf is null) return;

        float progress = Clamp(stateTimer / hoverJumpDuration, 0.0f, 1.0f);
        float y = LerpF(hoverStartPos.y, hoverApexPos.y, progress);
        tf.SetTranslate(Vector3(hoverStartPos.x, y, hoverStartPos.z));

        if (progress >= 1.0f) {
            ChangeState(FinalBossState::HoverPause);
        }
    }

    void UpdateHoverDive() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        float progress = Clamp(stateTimer / hoverDiveDuration, 0.0f, 1.0f);
        float x = LerpF(hoverDiveStartPos.x, hoverDiveTargetPos.x, progress);
        float y = LerpF(hoverDiveStartPos.y, hoverDiveTargetPos.y, progress);
        tf.SetTranslate(Vector3(x, y, hoverDiveStartPos.z));

        if (progress >= 1.0f) {
            hoverRepeatIndex++;
            if (hoverRepeatIndex < hoverRepeatCount) {
                ChangeState(FinalBossState::HoverJump); // 2回目以降はタメなしで再ジャンプ
            } else {
                ChangeState(FinalBossState::Idle);
            }
        }
    }

    // タメ→突進
    void UpdateDashRush() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        float progress = Clamp(stateTimer / dashDuration, 0.0f, 1.0f);
        float x = LerpF(dashStartPos.x, dashTargetPos.x, progress);
        tf.SetTranslate(Vector3(x, dashStartPos.y, dashStartPos.z));

        if (progress >= 1.0f) {
            ChangeState(FinalBossState::Idle);
        }
    }

    float LerpF(float a, float b, float t) {
        return a + (b - a) * t;
    }

    void End() {
    }
}
