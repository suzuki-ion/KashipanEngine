enum MoveDirection{
    Right, // 右移動
    Left // 左移動
}

class Green : ScriptComponentBehavior {

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 1.0f;

    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    float uvStep = 0.5f;

    [SerializeField, Tooltip("HP")]
    float hp = 1.0f;

    [SerializeField, Tooltip("死亡エフェクト")]
    Object@ deathEffect;

    // エフェクトのクローン
    Object@ cloneEffect;

    // 進行方向
    MoveDirection moveDir = MoveDirection::Left;

    bool isAlive = true;
    bool isAnimation = false;
    float deathEffectTimer = 0.0f;

    Vector2 velocity;
    // 元々はBox2D時代のrb.SetVelocity()にGetDeltaTime()無しで渡す値だったため、
    // 60fps相当のフレーム単位の量になっていた。位置積分にGetDeltaTime()を使うようになった
    // 今は「1秒あたりの変化量」として扱う必要があるため、当時の値の60倍にしている
    float gravity = 12.0f;
    float groundedThreshold = 0.5f;

    Box2DCollider@ col;
    SpriteRenderer@ sprite;

    // アニメーション用タイマー
    float animTimer = 0.0f;

    void Start() {
        GetComponent(@col);
        GetComponent(@sprite);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // moveDirに応じてY軸の回転を設定
        float rotY = (moveDir == MoveDirection::Left) ? 0.0f : 3.14159f;
        tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

        // 左右移動
        // velocityは「1秒あたりの移動量」なので、この時点ではGetDeltaTime()を掛けない
        // （掛けるのは下の位置積分の1箇所だけにする）
        float dir = (moveDir == MoveDirection::Left) ? -1.0f : 1.0f;
        velocity.x = dir * moveSpeed;

        // 重力（フレームをまたいで蓄積する値なので、ここはGetDeltaTime()を掛ける）
        velocity.y -= gravity * GetDeltaTime();

        // RigidBodyを使わず、速度を自前でTransformへ積分する
        tf.SetTranslate(tf.GetTranslate() + Vector3(velocity.x, velocity.y, 0.0f) * GetDeltaTime());

        // アニメーション処理
        animTimer += GetDeltaTime();
        int frame = int(animTimer / frameInterval) % 2;

        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(Vector2(frame * uvStep, 0.0f));
        }

        // HPが0になったら
        if(hp <= 0.0f){
            isAlive = false;
        }

        if(!isAlive){
            if(!isAnimation){
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
                if(col !is null) col.SetActive(false);
                if(sprite !is null) sprite.SetActive(false);
    
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

    void End() {
        Log("GoombaEnd");
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        // めり込み分を押し戻す
        ResolvePenetration(hit);

        // 進行方向の切り替え
        if(hit.otherObject.GetTag() == "Wall"){
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

        // 床（法線が上向き）からの接触で、かつ上昇中でない時だけ落下速度をリセットする
        // （無条件にすると、壁への接触中も重力による落下が止まってしまう）
        if (hit.normal.y > groundedThreshold && velocity.y <= 0.0f) {
            velocity.y = 0.0f;
        }
    }

    // hit.normal（自分を押し出す方向） * hit.penetration（めり込み量）だけ
    // Transformを移動させ、他コライダーとのめり込みを解消する
    void ResolvePenetration(const HitInfo &in hit) {
        if(hit.penetration <= 0.0f) return;

        Transform@ tf = GetTransform();
        if(tf is null) return;

        tf.SetTranslate(tf.GetTranslate() + hit.normal * hit.penetration);
    }
}