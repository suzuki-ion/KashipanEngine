class HPUI : ScriptComponentBehavior {
    [Header("参照設定")]

    [SerializeField, Tooltip("HPひとつぶんのゲージを表す元オブジェクト")]
    Object@ gaugeSource;

    [SerializeField, Tooltip("プレイヤーオブジェクト")]
    Object@ player;

    [Header("配置設定")]

    [SerializeField, Tooltip("1ゲージ目の初期位置(このオブジェクトからの相対座標)")]
    Vector3 startPosition;

    [SerializeField, Tooltip("HPゲージの配置間隔")]
    Vector3 gaugeInterval;

    // プレイヤーの"PlayerSC"タグ付きスクリプトコンポーネント
    ScriptComponent@ playerSc;

    // 複製したHPゲージオブジェクト一覧
    array<Object@> gauges;

    // 前フレームのHP(ゲージが減った瞬間の検出用)
    float previousHp = 0.0f;
    bool hasPreviousHp = false;

    void Start() {
        if (player is null || gaugeSource is null) return;

        // プレイヤーのオブジェクトから"PlayerSC"タグが付いたスクリプトコンポーネントを取得
        array<ScriptComponent@>@ scripts;
        if (player.GetComponents(@scripts)) {
            for (uint i = 0; i < scripts.length(); ++i) {
                if (scripts[i].GetTag() == "PlayerSC") {
                    @playerSc = scripts[i];
                    break;
                }
            }
        }

        if (playerSc is null) return;

        CreateGauges();
    }

    void CreateGauges() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        float maxHp = 0.0f;
        if (!playerSc.GetVariable("maxHp", maxHp)) return;

        Vector3 basePos = tf.GetTranslate() + startPosition;

        int gaugeCount = int(maxHp);
        for (int i = 0; i < gaugeCount; ++i) {
            Object@ gauge = GetScene().CloneObject(gaugeSource, "HPGauge" + i);
            if (gauge is null) continue;

            gauge.SetActive(true);

            Transform@ gaugeTf = gauge.GetTransform();
            if (gaugeTf !is null) {
                gaugeTf.SetTranslate(basePos + gaugeInterval * float(i));
            }

            gauges.insertLast(gauge);
        }
    }

    void Update() {
        if (playerSc is null) return;

        float hp = 0.0f;
        if (!playerSc.GetVariable("hp", hp)) return;

        // 初回はShake判定の基準を作るだけで、揺れは発生させない
        if (!hasPreviousHp) {
            previousHp = hp;
            hasPreviousHp = true;
        }

        // HPが減った瞬間は、減った特定のゲージだけでなく全ゲージアイコンを揺らす
        bool hpDecreased = hp < previousHp;

        for (uint i = 0; i < gauges.length(); ++i) {
            Object@ gauge = gauges[i];
            if (gauge is null) continue;

            SpriteRenderer@ sprite;
            if (gauge.GetComponent(@sprite)) {
                // 現在HPぶんだけUVをX座標0.0、それ以外は0.5に設定
                bool isFilled = float(i) < hp;
                sprite.SetInstanceUvTranslate(Vector2(isFilled ? 0.0f : 0.5f, 0.0f));
            }

            if (hpDecreased) {
                Shake@ shake;
                if (gauge.GetComponent(@shake)) {
                    shake.Play();
                }
            }
        }

        previousHp = hp;
    }

    void End() {
    }
}
