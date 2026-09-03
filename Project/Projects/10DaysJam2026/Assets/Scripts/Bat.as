enum Direction {
    Right,
    Left
}

class Bat : ScriptComponentBehavior {

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 10.0f;

    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    float uvStep = 0.5f;

    [SerializeField, Tooltip("HP")]
    float hp = 1.0f;

    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    [SerializeField, Tooltip("死亡エフェクト")]
    Object@ deathEffect;

    // エフェクトのクローン
    Object@ cloneEffect;

    SpriteRenderer@ sprite;
    Box2DCollider@ col;
    float animTimer = 0.0f;
    bool isAlive = true;
    bool isAnimation = false;
    float deathEffectTimer = 0.0f;

    void Start() {
        GetComponent(@sprite);
        GetComponent(@col);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // アニメーション処理
        animTimer += GetDeltaTime();
        int frame = int(animTimer / frameInterval) % 2;

        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(Vector2(frame * uvStep, 0.0f));
        }

        // プレイヤー追従処理
        if (player is null) return;
        Transform@ playerTf = player.GetTransform();
        if (playerTf is null) return;

        Vector3 currentPos = tf.GetTranslate();
        Vector3 targetPos = playerTf.GetTranslate();

        // プレイヤーへのベクトルと距離を計算
        Vector3 diff = targetPos - currentPos;
        float dist = diff.Length();

        if (dist > 0.01f) {
            // プレイヤーの位置に応じて向きを切り替え
            float rotY = (diff.x < 0.0f) ? 0.0f : 3.14159f;
            tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

            // プレイヤーに向かって移動
            Vector3 dir = diff / dist;
            tf.SetTranslate(currentPos + dir * moveSpeed * GetDeltaTime());
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
    }
}