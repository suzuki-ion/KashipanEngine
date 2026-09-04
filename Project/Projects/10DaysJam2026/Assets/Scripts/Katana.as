class Katana : ScriptComponentBehavior {
    [SerializeField, Tooltip("攻撃力")] 
    int power = 10;

    [SerializeField, Tooltip("クールダウン(秒)")] 
    float cooldown = 0.4f;

    [SerializeField, Tooltip("ダメージ値")] 
    float damageAmount = 1.0f;

    [SerializeField, Tooltip("レベル")] 
    int level = 1;

    [SerializeField, Tooltip("現在の経験値")] 
    float exp = 0.0f;
    
    [SerializeField, Tooltip("次のレベルまでの必要経験値")] 
    float nextExp = 10.0f;

    [SerializeField, Tooltip("座標")]
    Vector3 pos;

    [SerializeField, Tooltip("テクスチャ名リスト")]
    array<string>@ textureNames;

    [SerializeField, Tooltip("レベルごとの拡大率(X)")]
    array<float>@ scaleX = {15.0f, 20.0f, 21.0f};

    [SerializeField, Tooltip("レベルごとの拡大率(Y)")]
    array<float>@ scaleY = {6.0f, 8.0f, 26.0f};

    [SerializeField, Tooltip("攻撃演出の表示時間(秒)")]
    float activeDuration = 0.4f;

    [SerializeField, Tooltip("攻撃演出の縦方向オフセット")]
    float effectOffsetY = 3.5f;

    float cooldownTimer = 0.0f;
    Box2DCollider@ col;
    bool isActive = false;
    float activeTimer = 0.0f;

    // 攻撃時の横方向オフセット
    float attackOffsetX = 0.0f;

    // 現在のテクスチャ
    TextureSource@ currentTexture;

    // 攻撃演出用スプライト
    SpriteRenderer@ sprite;

    void Start(){
        GetComponent(@col);
        GetComponent(@currentTexture);
        GetComponent(@sprite);

        // 非攻撃時は演出を非表示にしておく
        if (sprite !is null) {
            sprite.SetActive(false);
        }
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        if (cooldownTimer > 0.0f) {
            cooldownTimer -= GetDeltaTime();
        }

        // 攻撃中は方向に応じたオフセットを加えて表示・当たり判定を配置
        Vector3 drawPos = pos;
        if (isActive) {
            drawPos += Vector3(attackOffsetX, effectOffsetY, 0.0f);
        }
        tf.SetTranslate(drawPos);

        // アクティブ時間の制御
        if(isActive){
            activeTimer += GetDeltaTime();
            if(activeTimer >= activeDuration){
                activeTimer = 0.0f;
                isActive = false;
                col.SetTrigger(true);

                // 攻撃演出を終了
                if (sprite !is null) {
                    sprite.SetActive(false);
                }
            }
        }

        // レベルに応じたテクスチャの切り替え
        string textureName = "App/Sprite/Effect/" + textureNames[level - 1] + ".png";
        currentTexture.SetTextureAssetPath(textureName);

        // レベルに応じたスケールの切り替え
        tf.SetScale(Vector3(scaleX[level - 1], scaleY[level - 1], 1.0f));

        // レベルに応じたコマ数・UVスケールの切り替え
        array<ScriptComponent@>@ scripts;
        if (GetComponents(@scripts)) {
            for (int i = 0; i < scripts.length(); ++i) {
                // AnimatorSCタグを持つSpriteAnimatorに指示を出す
                if (scripts[i].GetTag() == "AnimatorSC") {
                    if (level == 3) {
                        scripts[i].CallMethod("SetFrameCount", 3);
                        sprite.SetInstanceUvScale(Vector2(0.333f, 1.0f));
                    } else {
                        // レベル3以外は1コマに設定
                        scripts[i].CallMethod("SetFrameCount", 1);
                        sprite.SetInstanceUvScale(Vector2(1.0f, 1.0f));
                    }
                }
            }
        }
    }

    void Attack(float margin) {
        if (cooldownTimer > 0.0f) return;
        cooldownTimer = cooldown;

        // 攻撃方向に応じてKatana自身を反転させる
        bool facingRight = (margin >= 0.0f);
        Transform@ tf = GetTransform();
        if (tf !is null) {
            float rotY = facingRight ? 0.0f : 3.14159f;
            tf.SetRotate(Vector3(0.0f, rotY, 0.0f));
        }

        attackOffsetX = margin;
        col.SetTrigger(false);
        isActive = true;

        // 攻撃演出を表示
        if (sprite !is null) {
            sprite.SetActive(true);
        }

        // レベル3のコマアニメーションを攻撃開始時に頭から再生する
        if (level == 3) {
            array<ScriptComponent@>@ scripts;
            if (GetComponents(@scripts)) {
                for (int i = 0; i < scripts.length(); ++i) {
                    if (scripts[i].GetTag() == "AnimatorSC") {
                        scripts[i].CallMethod("PlayRowForce", 0);
                    }
                }
            }
        }

        Log("剣で攻撃");
    }

    void End() {
    }

    void OnCollisionEnter(const HitInfo &in hit){
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
        }
    }

    // 経験値の加算とレベルアップ処理
    void AddExp(float amount) {
        exp += amount;
        Log("Katana EXP: " + exp + " / " + nextExp);

        while (exp >= nextExp) {
            exp -= nextExp;
            level++;
            level = Clamp(level, 0, 3);
            nextExp *= 1.5f; // 次の必要経験値を増加
            damageAmount += 1.0f; // レベルアップで攻撃力を強化
            Log("Katana Level Up Lv." + level + " (攻撃力: " + damageAmount + ")");
        }
    }
}
