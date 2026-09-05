enum MoveDirection{
    Right, // 右移動
    Left // 左移動
}

class Green : ScriptComponentBehavior {

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 1.0f;

    [SerializeField, Tooltip("HP")]
    float hp = 1.0f;

    [SerializeField, Tooltip("撃破時にもらえる経験値")]
    float expValue = 5.0f;

    [SerializeField, Tooltip("重力")]
    float gravity = 12.0f;

    [SerializeField, Tooltip("無敵時間(秒)")]
    float invincibleDuration = 0.5f;

    [SerializeField, Tooltip("点滅の切り替え間隔(秒)")]
    float blinkInterval = 0.08f;

    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    [SerializeField, Tooltip("死亡エフェクト")]
    Object@ deathEffect;

    [SerializeField, Tooltip("セルフオブジェ")]
    Object@ self;

    // エフェクトのクローン
    Object@ cloneEffect;

    // 進行方向
    MoveDirection moveDir = MoveDirection::Left;

    bool isAlive = true;
    bool isAnimation = false;
    float deathEffectTimer = 0.0f;

    // 無敵時間・点滅管理用
    bool isInvincible = false;
    float invincibleTimer = 0.0f;
    SpriteRenderer@ sprite;

    Vector2 velocity;
    float groundedThreshold = 0.5f;

    Box2DCollider@ col;

    void Start() {
        GetComponent(@col);
        GetComponent(@sprite);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 生存時の無敵時間タイマー更新および点滅処理
        if (isAlive && isInvincible) {
            invincibleTimer += GetDeltaTime();

            // blinkIntervalごとにスプライトの表示・非表示を交互に切替
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

        // moveDirに応じてY軸の回転を設定
        float rotY = (moveDir == MoveDirection::Left) ? 0.0f : 3.14159f;
        tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

        // 左右移動
        float dir = (moveDir == MoveDirection::Left) ? -1.0f : 1.0f;
        velocity.x = dir * moveSpeed;

        // 重力
        velocity.y -= gravity * GetDeltaTime();

        // 位置の更新
        tf.SetTranslate(tf.GetTranslate() + Vector3(velocity.x, velocity.y, 0.0f) * GetDeltaTime());

        // HPが0になったら
        if(hp <= 0.0f){
            isAlive = false;
        }

        if(!isAlive){
            if(!isAnimation){
                // 経験値付与処理
                if (player !is null) {
                    ScriptComponent@ playerSc;
                    if (player.GetComponent(@playerSc)) {
                        playerSc.CallMethod("AddExp", expValue);
                    }
                }

                @cloneEffect = GetScene().CloneObject(deathEffect, "CloneDeathEffect");
                
                if(cloneEffect !is null){
                    Transform@ cloneTf = cloneEffect.GetTransform();
                    if(cloneTf !is null){
                        cloneTf.SetScale(Vector3(32.0f, 32.0f, 1.0f));
                    }

                    ScriptComponent@ sc;
                    if(cloneEffect.GetComponent(@sc)){
                        sc.CallMethod("StartAnimation");
                        sc.SetVariable("pos", tf.GetTranslate());
                    }
                }
    
                // 自身の描画と当たり判定を無効化
                self.SetComponentsActiveExceptTransformAndScript(false);
    
                isAnimation = true;
            }

            // アニメーション更新処理
            if(cloneEffect !is null){
                deathEffectTimer += GetDeltaTime();
                float deathEffectDuration = 0.6f;
                ScriptComponent@ sc;
                if(cloneEffect.GetComponent(@sc)){
                    sc.CallMethod("UpdateAnimation");

                    // アニメーションが終了したら非アクティブ化
                    if(deathEffectTimer >= deathEffectDuration){
                        cloneEffect.SetActive(false);
                    }
                }
            }
        }
    }

    // ダメージ受けて無敵状態を開始するメソッド
    void Damage(float amount) {
        if (!isAlive || isInvincible) return;

        hp -= amount;
        isInvincible = true;
        invincibleTimer = 0.0f;
    }

    void End() {
        Log("GoombaEnd");
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        // めり込み分を押し戻す
        ResolvePenetration(hit);

        // 進行方向の切り替え
        if(hit.selfCollider.GetTag() == "Direction"){
            if (moveDir == MoveDirection::Left) {
                moveDir = MoveDirection::Right;
                Log("Right");
            } else {
                moveDir = MoveDirection::Left;
                Log("Left");
            }
        }
    }

    void OnCollisionStay(const HitInfo &in hit){
        // めり込み分を押し戻す
        ResolvePenetration(hit);

        // 床からの接触かつ上昇中でない時だけ落下速度をリセット
        if (hit.normal.y > groundedThreshold && velocity.y <= 0.0f) {
            velocity.y = 0.0f;
        }
    }

    void ResolvePenetration(const HitInfo &in hit) {
        if(hit.penetration <= 0.0f) return;

        Transform@ tf = GetTransform();
        if(tf is null) return;

        tf.SetTranslate(tf.GetTranslate() + hit.normal * hit.penetration);
    }
}