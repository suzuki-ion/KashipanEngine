class Shuriken : ScriptComponentBehavior {
    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    float uvStep = 0.5f;

    [SerializeField, Tooltip("スピード")]
    float speed = 80.0f;

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
    array<float>@ scales = {6.0f, 8.0f, 12.0f};

    [SerializeField, Tooltip("テクスチャ名リスト")]
    array<string>@ textureNames;

    // アニメーション用タイマー
    float animTimer = 0.0f;
    
    // 進む方向
    float movingX = 0.0f;

    // 現在のテクスチャ
    TextureSource@ currentTexture;

    SpriteRenderer@ sprite;
    Box2DCollider@ col;

    void Start() {
        GetComponent(@sprite);
        GetComponent(@col);
        GetComponent(@currentTexture);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 移動処理
        pos.x += movingX * speed * GetDeltaTime();
        tf.SetTranslate(pos);

        // アニメーション処理
        animTimer += GetDeltaTime();
        int frame = int(animTimer / frameInterval) % 2;

        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(Vector2(frame * uvStep, 0.0f));
        }

        // レベルに応じたテクスチャの切り替え
        string textureName = "App/Sprite/Player/" + textureNames[level - 1] + ".png";
        currentTexture.SetTextureAssetPath(textureName);
        tf.SetScale(Vector3(scales[level - 1], scales[level - 1], 1.0f));
    }

    void End() {
    }

    void Attack(float moveX){
        //col.SetActive(true);
        movingX = moveX;
    }

    void OnCollisionEnter(const HitInfo &in hit){
        // 敵のダメージ処理
        if(hit.otherCollider.GetTag() == "Enemy"){
            Object@ enemy = hit.otherObject;

            if (enemy !is null) {
                ScriptComponent@ sc;
                if (enemy.GetComponent(@sc)) {
                    float hp;
                    if (sc.GetVariable("hp", hp)) {
                        Log("敵のHP: " + hp);
                        sc.SetVariable("hp", Clamp(hp - damageAmount, 0.0f, 100.0f));
                    }
                }
            }
        }else if(hit.otherCollider.GetTag() != "Player"){
            hit.selfCollider.SetActive(false);
            pos = Vector3(-100.0f, 0.0f, 0.0f);
            movingX = 0.0f;
        }
    }

    void SetPos(Vector3 playerPos){
        pos = playerPos;
    }

    // 経験値の加算とレベルアップ処理
    void AddExp(float amount) {
        exp += amount;
        Log("Sword EXP: " + exp + " / " + nextExp);

        while (exp >= nextExp) {
            exp -= nextExp;
            level++;
            nextExp *= 1.5f; // 次の必要経験値を増加
            damageAmount += 1.0f; // レベルアップで攻撃力を強化
            Log("Shuriken Level Up Lv." + level + " (攻撃力: " + damageAmount + ")");
        }
    }
}
