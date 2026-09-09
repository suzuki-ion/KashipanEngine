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

    [SerializeField, Tooltip("現在のオフセット値保持")]
    float currentMargin = 16.0f;

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

    void PlayTaggedAudio(const string &in tagName) {
        // Start()時点のキャッシュだと、クローン直後同フレームで呼ばれた場合等に
        // まだ取得できていないことがあるため、呼び出す都度取得し直す
        array<AudioSource@>@ sources;
        if (!GetComponents(@sources)) return;
        for(uint i = 0; i < sources.length(); ++i) {
            if(sources[i] !is null && sources[i].GetTag() == tagName) {
                sources[i].SetActive(true);
                sources[i].Play();
            }
        }
    }

    void Update() {
        // 会話中(isDialogueActive)は攻撃演出などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        Transform@ tf = GetTransform();
        if(tf is null) return;

        if (cooldownTimer > 0.0f) {
            cooldownTimer -= GetDeltaTime();
        }

        if(level == 3){
            effectOffsetY = 12.5f;
        }else {
            effectOffsetY = 3.5f;
        }

        // 攻撃中はプレイヤーの向きに合わせて毎フレーム回転とオフセットを追従
        ApplyFollowTransform();

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
                        // UVのずれ(Translate)も初期位置にリセットする
                        sprite.SetInstanceUvTranslate(Vector2(0.0f, 0.0f));
                    }
                }
            }
        }
    }

    // pos/attackOffsetX/effectOffsetY/currentMarginを元に、実際にTransformへ反映する処理。
    // Update()と、Playerから直接呼ばれるSyncFollowPositionの両方から使う共通処理
    void ApplyFollowTransform() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        if (isActive) {
            attackOffsetX = currentMargin;
            bool facingRight = (currentMargin >= 0.0f);
            float rotY = facingRight ? 0.0f : 3.14159f;
            tf.SetRotate(Vector3(0.0f, rotY, 0.0f));
        }

        Vector3 drawPos;
        if (isActive) {
            drawPos = pos + Vector3(attackOffsetX, effectOffsetY, 0.0f);
        } else {
            drawPos = Vector3(-1000.0f, 0.0f, 0.0f);
        }
        tf.SetTranslate(drawPos);
    }

    // Playerが毎フレーム、自身の移動処理が終わった直後に直接呼び出す。
    // これまではSetVariable("pos", ...)でpos/currentMarginを渡すだけで、実際に
    // Transformへ反映するのはKatana自身のUpdate()任せだった。そのため
    // 「Player.Update()」と「Katana.Update()」のどちらが先に呼ばれるかという
    // スクリプト実行順序に結果が依存してしまい、Katana側が先に呼ばれるフレームでは
    // 1フレーム前のプレイヤー座標のまま描画されてしまう。移動量が小さい時は誤差として
    // 目立たないが、ジャンプ中など1フレームあたりの移動量が大きい場面でズレが顕著になる。
    // この関数を呼び出し元(Player.Update)から直接実行することで、実行順序に関係なく
    // 「プレイヤーが動いた直後」に必ずKatanaのTransformを同じフレーム内で即座に
    // 更新できるようにしている
    void SyncFollowPosition(Vector3 playerPos, float margin, bool equipped) {
        pos = equipped ? playerPos : Vector3(-1000.0f, 0.0f, 0.0f);
        currentMargin = margin;
        ApplyFollowTransform();
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

        // 刀のレベルに応じた斬撃音を鳴らす(Lv1:Attack1, Lv2:Attack2, Lv3:Attack3)
        PlayTaggedAudio("Attack" + level);

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
        // 最大レベル(3)に達している場合は経験値を加算しない
        if (level >= 3) return;

        exp += amount;
        Log("Katana EXP: " + exp + " / " + nextExp);

        while (exp >= nextExp) {
            exp -= nextExp;
            level++;
            level = Clamp(level, 0, 3);
            nextExp *= 1.5f; // 次の必要経験値を増加
            damageAmount += 1.0f; // レベルアップで攻撃力を強化
            Log("Katana Level Up Lv." + level + " (攻撃力: " + damageAmount + ")");

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
        Log("Katana EXP: " + exp + " / " + nextExp);

        while (exp < 0.0f && level > 1) {
            level--;
            nextExp /= 1.5f; // AddExpと逆算し、1つ前のレベルの必要経験値に戻す
            exp += nextExp;
            damageAmount -= 1.0f; // レベルアップ時に強化した分の攻撃力を戻す
            Log("Katana Level Down Lv." + level + " (攻撃力: " + damageAmount + ")");
        }

        // レベル1未満には下げないので、経験値も0未満にはしない
        if (level <= 1 && exp < 0.0f) {
            exp = 0.0f;
        }
    }
}
