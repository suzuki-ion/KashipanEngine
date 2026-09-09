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
        // 会話中(isDialogueActive)は移動などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

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

            GetOwnerObject().SetActive(false);
        }
        // Tilemapに当たった際の反射処理
        else if(hit.otherObject.GetTag() == "Tilemap"){
            Vector3 normal = hit.normal;

            // 法線のX/Y成分のうち絶対値が大きい方を採用し、
            // 床・天井(Y方向の面)か壁(X方向の面)かを判定する
            if (Abs(normal.y) >= Abs(normal.x)) {
                // 法線が上向き(床)で落下中、または下向き(天井)で上昇中なら
                // 進行方向と面がぶつかっているのでYを反転
                if ((normal.y > 0.0f && currentSpeedY < 0.0f) || (normal.y < 0.0f && currentSpeedY > 0.0f)) {
                    currentSpeedY = -currentSpeedY * bounceFactorY;
                }
            } else {
                // 法線が左向き(右側の壁)で右移動中、または右向き(左側の壁)で左移動中なら
                // 進行方向と面がぶつかっているのでXを反転
                if ((normal.x < 0.0f && currentSpeedX > 0.0f) || (normal.x > 0.0f && currentSpeedX < 0.0f)) {
                    currentSpeedX = -currentSpeedX * bounceFactorX;
                }
            }
        }
    }

    void SetPos(Vector3 playerPos){
        pos = playerPos;
    }

    // 経験値の加算とレベルアップ処理
    void AddExp(float amount) {
        // 最大レベル(3)に達している場合は経験値を加算しない
        if (level >= 3) return;

        exp += amount;
        Log("Ball EXP: " + exp + " / " + nextExp);

       while (exp >= nextExp) {
            exp -= nextExp;
            level++;
            level = Clamp(level, 0, 3);
            nextExp *= 1.5f; // 次の必要経験値を増加
            damageAmount += 1.0f; // レベルアップで攻撃力を強化
            Log("Ball Level Up Lv." + level + " (攻撃力: " + damageAmount + ")");

            // 最大レベルに達したら以降の加算・レベルアップを止める
            if (level >= 3) {
                exp = 0.0f;
                break;
            }
        }
    }

    // 経験値の減少とレベルダウン処理
    void RemoveExp(float amount) {
        exp -= amount;
        Log("Ball EXP: " + exp + " / " + nextExp);

        while (exp < 0.0f && level > 1) {
            level--;
            nextExp /= 1.5f; // AddExpと逆算し、1つ前のレベルの必要経験値に戻す
            exp += nextExp;
            damageAmount -= 1.0f; // レベルアップ時に強化した分の攻撃力を戻す
            Log("Ball Level Down Lv." + level + " (攻撃力: " + damageAmount + ")");
        }

        // レベル1未満には下げないので、経験値も0未満にはしない
        if (level <= 1 && exp < 0.0f) {
            exp = 0.0f;
        }
    }
}