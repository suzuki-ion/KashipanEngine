class Ball : ScriptComponentBehavior {
    [SerializeField, Tooltip("X方向の初速度")]
    float initialSpeedX = 100.0f;

    [SerializeField, Tooltip("Y方向の初速度(打ち上げ力)")]
    float initialSpeedY = 100.0f;

    [SerializeField, Tooltip("重力")]
    float gravity = 300.0f;

    [SerializeField, Tooltip("Y方向の反発係数(0〜1)")]
    float bounceFactorY = 0.8f;

    [SerializeField, Tooltip("X方向の反発係数(壁に当たった時の速度維持率)")]
    float bounceFactorX = 1.0f;

    [SerializeField, Tooltip("威力")]
    float damageAmount = 1.0f;

    [SerializeField, Tooltip("レベル")] 
    int level = 1;

    [SerializeField, Tooltip("現在の経験値")] 
    float exp = 0.0f;
    
    [SerializeField, Tooltip("次のレベルまでの必要経験値")] 
    float nextExp = 10.0f;

    [SerializeField, Tooltip("座標")]
    Vector3 pos;

    [SerializeField, Tooltip("スケール")]
    array<float>@ scales = {3.0f, 4.0f, 5.0f};

    [SerializeField, Tooltip("テクスチャ名リスト")]
    array<string>@ textureNames;
    
    // 現在の速度
    float currentSpeedX = 0.0f;
    float currentSpeedY = 0.0f;

    // 現在のテクスチャ
    TextureSource@ currentTexture;

    SpriteRenderer@ sprite;

    void Start() {
        GetComponent(@sprite);
        GetComponent(@currentTexture);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 重力によるY方向の速度減衰
        currentSpeedY -= gravity * GetDeltaTime();

        // 移動処理
        pos.x += currentSpeedX * GetDeltaTime();
        pos.y += currentSpeedY * GetDeltaTime();
        tf.SetTranslate(pos);

        // レベルに応じたテクスチャとスケールの切り替え
        if (textureNames !is null && textureNames.length() >= uint(level)) {
            string textureName = "App/Sprite/Player/" + textureNames[level - 1] + ".png";
            if (currentTexture !is null) {
                currentTexture.SetTextureAssetPath(textureName);
            }
        }

        if (scales !is null && scales.length() >= uint(level)) {
            tf.SetScale(Vector3(scales[level - 1], scales[level - 1], 1.0f));
        }
    }

    void End() {
    }

    // プレイヤーの向きに合わせてX速度を決定し、上方向に打ち上げる
    void Attack(float moveX){
        currentSpeedX = moveX * initialSpeedX;
        currentSpeedY = initialSpeedY;
    }

    void OnCollisionEnter(const HitInfo &in hit){
        // 敵のダメージ処理
        if(hit.otherCollider.GetTag() == "Enemy"){
            Object@ enemy = hit.otherObject;

            if (enemy !is null) {
                ScriptComponent@ sc;
                if (enemy.GetComponent(@sc)) {
                    sc.CallMethod("Damage", damageAmount);
                }
            }
        }
        // Tilemapに当たった際の反射処理
        else if(hit.otherObject.GetTag() == "Tilemap"){
            // ぶつかった自分自身のコライダーのタグを取得
            Tag selfTag = hit.selfCollider.GetTag();

            if (selfTag == "Bottom" && currentSpeedY < 0.0f) {
                // 下のコライダーが落下中に接触したら床としてYを反転
                currentSpeedY = -currentSpeedY * bounceFactorY;
            } 
            else if (selfTag == "Top" && currentSpeedY > 0.0f) {
                // 上のコライダーが上昇中に接触したら天井としてYを反転
                currentSpeedY = -currentSpeedY * bounceFactorY;
            } 
            else if (selfTag == "Right" && currentSpeedX > 0.0f) {
                // 右のコライダーが右移動中に接触したら右壁としてXを反転
                currentSpeedX = -currentSpeedX * bounceFactorX;
            } 
            else if (selfTag == "Left" && currentSpeedX < 0.0f) {
                // 左のコライダーが左移動中に接触したら左壁としてXを反転
                currentSpeedX = -currentSpeedX * bounceFactorX;
            }
        }
    }

    void SetPos(Vector3 playerPos){
        pos = playerPos;
    }

    // 経験値の加算とレベルアップ処理
    void AddExp(float amount) {
        exp += amount;
        Log("Ball EXP: " + exp + " / " + nextExp);

        while (exp >= nextExp) {
            exp -= nextExp;
            level++;
            nextExp *= 1.5f;
            damageAmount += 1.0f;
            Log("Ball Level Up Lv." + level + " (攻撃力: " + damageAmount + ")");
        }
    }
}