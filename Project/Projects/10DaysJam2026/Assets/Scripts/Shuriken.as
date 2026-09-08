class Shuriken : ScriptComponentBehavior {
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
        // 会話中(isDialogueActive)は移動などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 移動処理
        pos.x += movingX * speed * GetDeltaTime();
        tf.SetTranslate(pos);

        // レベルに応じたテクスチャの切り替え
        string textureName = "App/Sprite/Player/" + textureNames[level - 1] + ".png";
        currentTexture.SetTextureAssetPath(textureName);
        tf.SetScale(Vector3(scales[level - 1], scales[level - 1], 1.0f));
    }

    void End() {
    }

    void Attack(float moveX){
        movingX = moveX;
    }

    void OnCollisionEnter(const HitInfo &in hit){
        // 敵のダメージ処理
        if(hit.otherCollider.GetTag() == "Enemy"){
            Object@ enemy = hit.otherObject;

            if (enemy !is null) {
                ScriptComponent@ sc;
                if (enemy.GetComponent(@sc)) {
                    sc.CallMethod("Damage", damageAmount);
                    float hp;
                    if (sc.GetVariable("hp", hp)) {
                        Log("敵のHP: " + hp);
                    }
                }
            }
            GetOwnerObject().SetActive(false);
        }else if(hit.otherObject.GetTag() == "Tilemap"){
            hit.selfCollider.SetActive(false);
            pos = Vector3(-1000.0f, 0.0f, 0.0f);
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
            level = Clamp(level, 0, 3);
            nextExp *= 1.5f; // 次の必要経験値を増加
            damageAmount += 1.0f; // レベルアップで攻撃力を強化
            Log("Shuriken Level Up Lv." + level + " (攻撃力: " + damageAmount + ")");
        }
    }

    // 経験値の減少とレベルダウン処理
    void RemoveExp(float amount) {
        exp -= amount;
        Log("Shuriken EXP: " + exp + " / " + nextExp);

        while (exp < 0.0f && level > 1) {
            level--;
            nextExp /= 1.5f; // AddExpと逆算し、1つ前のレベルの必要経験値に戻す
            exp += nextExp;
            damageAmount -= 1.0f; // レベルアップ時に強化した分の攻撃力を戻す
            Log("Shuriken Level Down Lv." + level + " (攻撃力: " + damageAmount + ")");
        }

        // レベル1未満には下げないので、経験値も0未満にはしない
        if (level <= 1 && exp < 0.0f) {
            exp = 0.0f;
        }
    }
}