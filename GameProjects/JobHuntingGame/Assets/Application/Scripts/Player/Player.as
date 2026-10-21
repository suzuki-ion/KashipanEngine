#include "PlayerInput.as"
#include "PlayerTransformation.as"
#include "PlayerMovement.as"
#include "PlayerDamageFlash.as"
#include "PlayerCombat.as"
#include "PlayerEmbedding.as"
#include "PlayerMovementTarget.as"

class Player : ScriptComponentBehavior {
    [Header("プレイヤーの基本設定")]

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 0.1f;
    [SerializeField, Tooltip("ジャンプ力")]
    float jumpPower = 16.0f;
    [SerializeField, Tooltip("重力")]
    float gravity = 0.2f;
    [SerializeField, Tooltip("横方向の減速")]
    float lateralDeceleration = 0.1f;
    [SerializeField, Tooltip("最小速度")]
    Vector3 minVelocity = Vector3(-8.0f, -16.0f, -8.0f);
    [SerializeField, Tooltip("最大速度")]
    Vector3 maxVelocity = Vector3(8.0f, 16.0f, 8.0f);

    [Header("移動方向追従オブジェクト")]

    [SerializeField, Tooltip("プレイヤーの移動方向の先へ配置するオブジェクト")]
    Object@ movementTargetObject;
    [SerializeField, Tooltip("追従オブジェクトの基準位置に加算するオフセット")]
    Vector3 movementTargetOffset = Vector3(0.0f, 0.0f, 0.0f);
    [SerializeField, Tooltip("プレイヤーの水平速度に掛ける距離倍率")]
    float movementTargetDistanceMultiplier = 1.0f;

    [Header("接地・滑りの設定")]

    [SerializeField, Tooltip("地面との接触判定閾値")]
    float groundedThreshold = 0.4f;
    [SerializeField, Tooltip("敵との接触判定閾値")]
    float enemyCollisionThreshold = 0.4f;
    [SerializeField, Tooltip("プレイヤーが滑り始める地面の傾き閾値（接地面の法線Yがこの値未満なら滑る）")]
    float slideThreshold = 0.5f;
    [SerializeField, Tooltip("接地状態を維持する猶予時間（秒）。坂道や動く床で接触が瞬間的に途切れても着地判定が誤爆しないようにする")]
    float groundedGraceTime = 0.1f;
    [SerializeField, Tooltip("接地中に地面へ押し付ける速度。下り坂や下降する床から離れないようにする")]
    float groundStickSpeed = 2.0f;
    [SerializeField, Tooltip("急斜面を滑り落ちる基本加速度（斜面の角度によらず常にかかる分）")]
    float slideAcceleration = 0.5f;
    [SerializeField, Tooltip("斜面の角度に対する滑り加速度の上がり値（sin(斜面角度)にこの値を掛けた分が基本加速度へ加算される。急な斜面ほど速く滑る）")]
    float slideAngleAcceleration = 2.0f;
    [SerializeField, Tooltip("急斜面を滑り落ちる最大速度")]
    float maxSlideSpeed = 10.0f;

    [Header("Box形態の傾き設定")]

    [SerializeField, Tooltip("Box形態が接地面の法線へ傾く速さ。大きいほど素早く追従する")]
    float boxGroundTiltFollowSpeed = 8.0f;

    [Header("Cone形態の刺さり解除設定")]

    [SerializeField, Tooltip("Coneが壁から刺さり解除する際に加える上向き速度")]
    float coneWallReleaseUpwardPower = 6.0f;

    [Header("プレイヤーの状態")]

    [SerializeField, Tooltip("最大HP")]
    int maxHp = 3;
    [SerializeField, Tooltip("現在のHP")]
    int currentHp = 3;
    [SerializeField, Tooltip("敵との接触で受けるダメージ量")]
    int damageAmount = 1;
    [SerializeField, Tooltip("被ダメージ後、連続でダメージを受けないようにする無敵時間（秒）")]
    float damageCooldown = 1.5f;
    [SerializeField, Tooltip("被ダメージ時のノックバック速度（X: 敵と反対方向の横速度, Y: 上方向のポップ速度）")]
    Vector3 knockbackPower = Vector3(4.0f, 6.0f, 0.0f);
    [SerializeField, Tooltip("被ダメージ時の点滅色")]
    Vector4 damageFlashColor = Vector4(1.0f, 0.2f, 0.2f, 1.0f);
    [SerializeField, Tooltip("被ダメージ時に点滅させる間隔（秒）")]
    float damageFlashInterval = 0.1f;

    // --- 責務ごとに分割したサブモジュール（Awake()で生成） ---
    PlayerInput@ input;
    PlayerTransformation@ transformation;
    PlayerMovement@ movement;
    PlayerDamageFlash@ damageFlash;
    PlayerCombat@ combat;
    PlayerEmbedding@ embedding;
    PlayerMovementTarget@ movementTarget;

    void Awake() {
        @input = PlayerInput();
        @transformation = PlayerTransformation(this);
        @movement = PlayerMovement(this, transformation);
        @damageFlash = PlayerDamageFlash(this);
        @combat = PlayerCombat(this, movement, damageFlash, transformation);
        @embedding = PlayerEmbedding(this, transformation);
        @movementTarget = PlayerMovementTarget(this);

        transformation.Awake();
    }

    void Update() {
        const float dt = GetDeltaTime() * GetGameSpeed();

        input.Update();
        embedding.Update(dt);

        if (input.transformationTriggered) {
            transformation.CycleForm();
            // 刺さっている最中に別の形態へ変わった場合は、刺さり状態を強制的に解除する
            if (!transformation.CanEmbedOnImpact()) {
                embedding.Release();
            }
        }

        if (embedding.IsEmbedded()) {
            // 刺さっている間は通常の移動・戦闘処理を一切行わず、相手への追従とジャンプ解除だけを見る
            embedding.FollowSurface();
            Vector3 popVelocity;
            if (embedding.TryRelease(input.jumpTriggered, popVelocity)) {
                movement.ApplyExternalVelocity(popVelocity);
                // 通常ジャンプと同様、速度を設定したフレームから移動へ反映する。
                // 次フレームまで待つと先に重力が適用され、通常ジャンプより到達高度が低くなる。
                movement.ApplyVelocity(dt);
            }
        } else {
            movement.isJumping = input.jumpTriggered;

            movement.UpdateGroundState(dt);
            // Cone形態は接地中の左右移動ができない（ジャンプと空中移動のみ許可する）
            const bool allowGroundInput = transformation.CanWalkOnGround() || !movement.IsGrounded();
            movement.LateralMovement(allowGroundInput ? input.moveDirection : 0.0f);
            movement.UpdateSlideVelocity(dt, input.moveDirection);
            movement.UpdateVerticalMotion(dt, combat.isCollidingWithEnemy, combat.enemyHitNormal, combat.enemyBounceRequested);
            combat.UpdateDamageCooldown(dt);
            combat.CheckEnemyDamage(input.moveDirection);
            movement.ApplyVelocity(dt);
        }

        // 刺さっている間は面に固定されているため、移動方向への回転は更新しない。
        // 刺さり解除ジャンプ後はApplyExternalVelocityで設定された自機速度に合わせて回転する。
        if (!embedding.IsEmbedded()) {
            movement.UpdateBoxGroundTilt(dt);
            movement.UpdateConeFacing();
        }

        damageFlash.Update(dt);
        CheckDeath();
        movementTarget.Update(movement);

        // 生の接触情報は毎フレームリセットする（次フレームの衝突コールバックで再設定される）
        movement.hasGroundContact = false;
        combat.isCollidingWithEnemy = false;
        combat.enemyBounceRequested = false;
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        embedding.HandleCollisionContact(hit);
        combat.HandleCollisionEnter(hit);
    }

    void OnCollisionStay(const HitInfo &in hit) {
        embedding.HandleCollisionContact(hit);
        // 刺さっている間は通常の接地処理（押し戻し等）を行わない。Transformの制御はembedding側に任せる
        if (embedding.IsEmbedded()) return;
        // 解除直後に元の面との接触が残っていても、Coneの角度依存の接触法線で
        // ジャンプ速度が打ち消されないよう、短い猶予時間中は通常衝突処理へ渡さない
        if (embedding.ShouldIgnoreReleasedSurface(hit)) return;
        movement.HandleCollisionStay(hit);
    }

    // HPが0以下になった場合、シーン変数に保存されたチェックポイント座標からリスポーンする
    void CheckDeath() {
        if (currentHp > 0) return;

        Transform@ tf = GetTransform();
        Scene@ scene = GetScene();
        Vector3 respawnPosition;
        bool hasCheckpoint = (scene !is null) && scene.GetVariable("CheckpointPosition", respawnPosition);
        if (tf !is null && hasCheckpoint) {
            tf.SetTranslate(respawnPosition);
        }

        movement.ResetVelocities();
        embedding.Release();
        currentHp = maxHp;
        combat.ResetAfterRespawn();
        damageFlash.Stop();
    }
}
