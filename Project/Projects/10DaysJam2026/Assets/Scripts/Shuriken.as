class Shuriken : ScriptComponentBehavior {
    [SerializeField, Tooltip("スピード")]
    float speed = 80.0f;

    [SerializeField, Tooltip("威力")]
    float damageAmount = 1.0f;

    [SerializeField, Tooltip("レベル")] 
    int level = 1;

    [SerializeField, Tooltip("現在の経験値")] 
    float exp = 0.0f;
    
    [SerializeField, Tooltip("次のレベルまでの必要経験値(セーブ/UI表示用にexpToNextLevelTableから自動反映される)")] 
    float nextExp = 10.0f;

    [SerializeField, Tooltip("レベルアップに必要な経験値テーブル(要素0:Lv1→2, 要素1:Lv2→3)")]
    array<float>@ expToNextLevelTable = {10.0f, 15.0f};

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

        // インスペクターで設定した必要経験値テーブルを現在のレベルに合わせて反映
        UpdateNextExpFromTable();
    }

    // expToNextLevelTableから現在のレベルに応じた必要経験値をnextExpへ反映する。
    // nextExpはセーブデータやLevelGauge(UI)がGetVariable("nextExp", ...)で参照するため、
    // フィールドとして保持しつつ、実際の値はインスペクターで設定するテーブル側で管理する
    void UpdateNextExpFromTable() {
        if (expToNextLevelTable !is null && level >= 1 && expToNextLevelTable.length() >= uint(level)) {
            nextExp = expToNextLevelTable[level - 1];
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
        PlayTaggedAudio("Attack");
    }

    void OnCollisionEnter(const HitInfo &in hit){
        // 敵のダメージ処理
        if(hit.otherCollider.GetTag() == "Enemy" || hit.otherCollider.GetTag() == "Ovary"){
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
        // 最大レベル(3)に達している場合は経験値を加算しない
        if (level >= 3) return;

        exp += amount;
        Log("Sword EXP: " + exp + " / " + nextExp);

        while (exp >= nextExp) {
            exp -= nextExp;
            level++;
            level = Clamp(level, 0, 3);
            UpdateNextExpFromTable(); // 次の必要経験値をテーブルから取得
            damageAmount += 1.0f; // レベルアップで攻撃力を強化
            Log("Shuriken Level Up Lv." + level + " (攻撃力: " + damageAmount + ")");

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
        Log("Shuriken EXP: " + exp + " / " + nextExp);

        while (exp < 0.0f && level > 1) {
            level--;
            UpdateNextExpFromTable(); // 1つ前のレベルの必要経験値をテーブルから取得
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