class BossHPUI : ScriptComponentBehavior {
    [Header("参照設定")]

    [SerializeField, Tooltip("HPひとつぶんのゲージを表す元オブジェクト")]
    Object@ gaugeSource;

    [SerializeField, Tooltip("ボスオブジェクト")]
    Object@ boss;

    [Header("配置設定")]

    [SerializeField, Tooltip("1ゲージあたりのHP量")]
    float hpPerGauge = 5.0f;

    [SerializeField, Tooltip("1ゲージ目の初期位置(このオブジェクトからの相対座標)")]
    Vector3 startPosition;

    [SerializeField, Tooltip("HPゲージの配置間隔")]
    Vector3 gaugeInterval;

    // ボスのスクリプトコンポーネント
    ScriptComponent@ bossSc;

    // SwitchBoss()呼び出し前に、切り替え先のボスオブジェクトをセットしておくための一時変数
    Object@ pendingBoss;

    // 複製したHPゲージオブジェクト一覧
    array<Object@> gauges;

    // 前フレームのHP(ゲージが減った瞬間の検出用)
    float previousHp = 0.0f;
    bool hasPreviousHp = false;

    void Start() {
        if (boss is null || gaugeSource is null) return;
        
        // 0以下の値が設定された場合の安全対策
        if (hpPerGauge <= 0.0f) {
            hpPerGauge = 1.0f;
        }

        // ボスオブジェクトからスクリプトコンポーネントを取得
        array<ScriptComponent@>@ scripts;
        if (boss.GetComponents(@scripts)) {
            for (uint i = 0; i < scripts.length(); ++i) {
                float dummyHp;
                if (scripts[i].GetVariable("maxHp", dummyHp)) {
                    @bossSc = scripts[i];
                    break;
                }
            }
        }

        if (bossSc is null) return;

        CreateGauges();
    }

    void CreateGauges() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        float maxHp = 0.0f;
        if (!bossSc.GetVariable("maxHp", maxHp)) return;

        Vector3 basePos = tf.GetTranslate() + startPosition;

        // 生成するゲージの総数を計算（余りが出る場合は1つ追加）
        int gaugeCount = int(maxHp / hpPerGauge);
        if (maxHp > float(gaugeCount) * hpPerGauge) {
            gaugeCount++;
        }

        for (int i = 0; i < gaugeCount; ++i) {
            Object@ gauge = GetScene().CloneObject(gaugeSource, "BossHPGauge" + i);
            if (gauge is null) continue;

            gauge.SetActive(true);

            Transform@ gaugeTf = gauge.GetTransform();
            if (gaugeTf !is null) {
                gaugeTf.SetTranslate(basePos + gaugeInterval * float(i));
            }

            gauges.insertLast(gauge);
        }
    }

    // 参照するボスをpendingBossへ切り替え、ゲージを作り直す
    // (例: Boss3撃破時にFinalBossへ切り替える際、Boss3側から
    //  SetVariable("pendingBoss", finalBossObj) → CallMethod("SwitchBoss") の順で呼び出す想定)
    void SwitchBoss() {
        if (pendingBoss is null || gaugeSource is null) return;

        @boss = pendingBoss;
        @pendingBoss = null;

        // 既存のゲージを非表示にして作り直す
        for (uint i = 0; i < gauges.length(); ++i) {
            Object@ gauge = gauges[i];
            if (gauge !is null) {
                gauge.SetActive(false);
            }
        }
        gauges.resize(0);

        // 新しいボスからスクリプトコンポーネントを探し直す
        @bossSc = null;
        array<ScriptComponent@>@ scripts;
        if (boss.GetComponents(@scripts)) {
            for (uint i = 0; i < scripts.length(); ++i) {
                float dummyHp;
                if (scripts[i].GetVariable("maxHp", dummyHp)) {
                    @bossSc = scripts[i];
                    break;
                }
            }
        }

        if (bossSc is null) return;

        // 新しいボスのHPで前フレームHPを取り直す
        hasPreviousHp = false;

        CreateGauges();
    }

    void Update() {
        if (bossSc is null) return;

        float hp = 0.0f;
        if (!bossSc.GetVariable("hp", hp)) return;

        if (!hasPreviousHp) {
            previousHp = hp;
            hasPreviousHp = true;
        }

        bool hpDecreased = hp < previousHp;

        for (uint i = 0; i < gauges.length(); ++i) {
            Object@ gauge = gauges[i];
            if (gauge is null) continue;

            SpriteRenderer@ sprite;
            if (gauge.GetComponent(@sprite)) {
                // 現在のHPが、このゲージが担当する下限HPを上回っているかを判定
                bool isFilled = hp > (float(i) * hpPerGauge);
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