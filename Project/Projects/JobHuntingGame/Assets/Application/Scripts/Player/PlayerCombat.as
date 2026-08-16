// 他オブジェクトのEnemy.asへ撃破を要求する（ScriptComponent.SetVariableでrequestDefeatを
// 立てるだけの共通処理。Box形態の踏みつけとCone形態の敵への突進で使用する）
void RequestEnemyDefeat(Object@ enemyObject) {
    ScriptComponent@ enemyScript;
    if (!enemyObject.GetComponent(@enemyScript)) return;
    bool value = true;
    enemyScript.SetVariable("requestDefeat", value);
}

// 敵接触の検知、被ダメージ、ノックバック、死亡コライダーを担当する
class PlayerCombat {
    Player@ owner;
    PlayerMovement@ movement;
    PlayerDamageFlash@ damageFlash;
    PlayerTransformation@ transformation;

    bool isCollidingWithEnemy = false;
    bool enemyBounceRequested = false;
    Vector3 enemyHitNormal = Vector3(0.0f, 0.0f, 0.0f);
    // isCollidingWithEnemyと同時に更新する、接触相手の敵オブジェクト（Box形態の踏みつけ撃破用）
    Object@ collidingEnemyObject;
    float damageCooldownTimer = 0.0f;

    Tag enemyColliderTag = Tag("EnemySphere");
    Tag deathLineColliderTag = Tag("DeathLine");
    Tag legacyDeathColliderTag = Tag("Death");

    PlayerCombat(Player@ inOwner, PlayerMovement@ inMovement, PlayerDamageFlash@ inDamageFlash, PlayerTransformation@ inTransformation) {
        @owner = inOwner;
        @movement = inMovement;
        @damageFlash = inDamageFlash;
        @transformation = inTransformation;
    }

    void HandleCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider.GetTag() == enemyColliderTag) {
            if (!isCollidingWithEnemy) {
                isCollidingWithEnemy = true;
                enemyHitNormal = hit.normal;
                @collidingEnemyObject = hit.otherObject;
                if (transformation.CanImpactDefeatEnemy()) {
                    RequestEnemyDefeat(collidingEnemyObject);
                    enemyBounceRequested = true;
                }
            }
        } else if (hit.otherCollider.GetTag() == deathLineColliderTag ||
                   hit.otherCollider.GetTag() == legacyDeathColliderTag) {
            // 死亡判定のコライダーに触れたら即座にHPを0にする
            owner.currentHp = 0;
        }
    }

    void UpdateDamageCooldown(float dt) {
        if (damageCooldownTimer > 0.0f) {
            damageCooldownTimer -= dt;
        }
    }

    // 敵の上以外から触れた場合にダメージを受ける（上から踏んだ場合はPlayerMovement::UpdateVerticalMotion()のバウンド処理に任せる）
    void CheckEnemyDamage(float moveDirection) {
        if (!isCollidingWithEnemy) return;

        if (enemyHitNormal.y > owner.enemyCollisionThreshold) {
            // 上からの接触＝踏みつけ。Box形態はここで敵を撃破する。
            // Sphere/ConeはPlayerSphereタグを使うため、敵自身の踏まれ判定で撃破される。
            if (transformation.CanStompDefeatEnemy() && collidingEnemyObject !is null) {
                RequestEnemyDefeat(collidingEnemyObject);
            }
            return; // 踏みつけ自体では自分はダメージを受けない
        }

        if (!transformation.TakesContactDamage()) return;
        if (damageCooldownTimer > 0.0f) return;

        TakeDamage(owner.damageAmount, enemyHitNormal, moveDirection);
    }

    void TakeDamage(int amount, const Vector3 &in hitNormal, float moveDirection) {
        owner.currentHp -= amount;
        damageCooldownTimer = owner.damageCooldown;
        damageFlash.StartFlash(owner.damageCooldown);

        // 敵と反対方向へノックバック（横方向は接触方向、縦方向は常に上向きへポップさせる）
        float knockbackDirX = (hitNormal.x != 0.0f) ? Sign(hitNormal.x) : -Sign(moveDirection);
        movement.velocity.x = knockbackDirX * owner.knockbackPower.x;
        movement.velocity.y = owner.knockbackPower.y;
    }

    // リスポーン時、無敵猶予として damageCooldown をそのまま流用する
    void ResetAfterRespawn() {
        damageCooldownTimer = owner.damageCooldown;
        enemyBounceRequested = false;
    }
}
