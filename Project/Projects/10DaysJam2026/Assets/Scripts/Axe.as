class Axe : ScriptComponentBehavior {
    [SerializeField, Tooltip("X方向の初速度")]
    float initialSpeedX = 60.0f;

    [SerializeField, Tooltip("Y方向の初速度(打ち上げ力)")]
    float initialSpeedY = 150.0f;

    [SerializeField, Tooltip("重力")]
    float gravity = 300.0f;

    [SerializeField, Tooltip("威力")]
    float damageAmount = 2.0f;

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
    
    // 現在の速度
    float currentSpeedX = 0.0f;
    float currentSpeedY = 0.0f;

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

        // レベルに応じたコマ数・UVスケールの切り替え
        array<ScriptComponent@>@ scripts;
        if (GetComponents(@scripts)) {
            for (int i = 0; i < scripts.length(); ++i) {
                // AnimatorSCタグを持つSpriteAnimatorに指示を出す
                if (scripts[i].GetTag() == "AnimatorSC") {
                    if (level == 1) {
                        scripts[i].CallMethod("SetFrameCount", 4);
                        sprite.SetInstanceUvScale(Vector2(0.25f, 1.0f));
                        // uvStepのVector2型での更新
                        scripts[i].SetVariable("uvStep", Vector2(0.25f, 1.0f));
                    } else {
                        // レベル1以外は2コマに設定
                        scripts[i].CallMethod("SetFrameCount", 2);
                        sprite.SetInstanceUvScale(Vector2(0.5f, 1.0f));
                        scripts[i].SetVariable("uvStep", Vector2(0.5f, 1.0f));
                    }
                }
            }
        }

        if (scales !is null && scales.length() >= uint(level)) {
            tf.SetScale(Vector3(scales[level - 1], scales[level - 1], 1.0f));
        }
    }

    void End() {
    }

    void Attack(float moveX){
        currentSpeedX = moveX * initialSpeedX; // プレイヤーの向きに合わせてX速度を決定
        currentSpeedY = initialSpeedY;         // 上方向に打ち上げる
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
        // Tilemapに当たったら消滅させる
        else if(hit.otherObject.GetTag() == "Tilemap"){
            hit.selfCollider.SetActive(false);
            pos = Vector3(-1000.0f, 0.0f, 0.0f);
            currentSpeedX = 0.0f;
            currentSpeedY = 0.0f;
        }
    }

    void SetPos(Vector3 playerPos){
        pos = playerPos;
    }

    // 経験値の加算とレベルアップ処理
    void AddExp(float amount) {
        exp += amount;
        Log("Axe EXP: " + exp + " / " + nextExp);

        while (exp >= nextExp) {
            exp -= nextExp;
            level++;
            nextExp *= 1.5f; // 次の必要経験値を増加
            damageAmount += 1.0f; // レベルアップで攻撃力を強化
            Log("Axe Level Up Lv." + level + " (攻撃力: " + damageAmount + ")");
        }
    }
}