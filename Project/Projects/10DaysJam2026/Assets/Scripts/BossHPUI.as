class BossHPUI : ScriptComponentBehavior {
    [Header("参照設定")]

    [SerializeField, Tooltip("HPひとつぶんのゲージを表す元オブジェクト")]
    Object@ gaugeSource;

    [SerializeField, Tooltip("通常時に参照するボスオブジェクト。Boss1など")]
    Object@ boss;

    [Header("配置設定")]

    [SerializeField, Tooltip("1ゲージあたりのHP量")]
    float hpPerGauge = 5.0f;

    [SerializeField, Tooltip("1ゲージ目の初期位置(このオブジェクトからの相対座標)")]
    Vector3 startPosition;

    [SerializeField, Tooltip("HPゲージの配置間隔")]
    Vector3 gaugeInterval;

    // 参照モード
    // 0 = bossオブジェクトのScriptComponentを直接参照
    // 1 = Boss3の合計HPをシーン変数から参照
    // 2 = FinalBossのHPをシーン変数から参照
    int sourceMode = 0;
    int lastSourceMode = -1;

    // ボスのスクリプトコンポーネント(直接参照モード用)
    ScriptComponent@ bossSc;

    // 複製したHPゲージオブジェクト一覧
    array<Object@> gauges;

    // 前フレームのHP(ゲージが減った瞬間の検出用)
    float previousHp = 0.0f;
    bool hasPreviousHp = false;

    void Start() {
        // 0以下の値が設定された場合の安全対策
        if (hpPerGauge <= 0.0f) {
            hpPerGauge = 1.0f;
        }

        // 通常はInspectorで指定したボスを直接参照する
        FindBossScript();

        // Boss3 / FinalBoss側が先に設定している場合はこちらを優先
        int sceneSource = 0;
        if (GetScene().GetVariable("BossHPUI_Source", sceneSource)) {
            if (sceneSource >= 0 && sceneSource <= 2) {
                sourceMode = sceneSource;
            }
        }

        if (sourceMode == 0) {
            CreateGaugesFromBoss();
        } else {
            // Boss3 / FinalBossのStart順によってはまだシーン変数が
            // 書き込まれていない可能性があるため、Update側で生成する
            hasPreviousHp = false;
        }

        lastSourceMode = sourceMode;
    }

    // Inspectorで指定されたボスからScriptComponentを取得
    void FindBossScript() {
        @bossSc = null;
        if (boss is null) return;

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
    }

    // 現在のボスHPを取得する
    bool GetCurrentHp(float &out hp, float &out maxHp) {
        hp = 0.0f;
        maxHp = 0.0f;

        // 0: Objectを直接参照
        if (sourceMode == 0) {
            if (bossSc is null) return false;
            if (!bossSc.GetVariable("hp", hp)) return false;
            if (!bossSc.GetVariable("maxHp", maxHp)) return false;
            return true;
        }

        // 1/2: Boss3 / FinalBossがシーン変数へ公開している数値を参照
        if (!GetScene().GetVariable("BossHPUI_HP", hp)) return false;
        if (!GetScene().GetVariable("BossHPUI_MaxHP", maxHp)) return false;

        return maxHp > 0.0f;
    }

    // 現在のHPからゲージを作り直す
    void CreateGauges(float maxHp) {
        Transform@ tf = GetTransform();
        if (tf is null || gaugeSource is null) return;
        if (maxHp <= 0.0f) return;

        Vector3 basePos = tf.GetTranslate() + startPosition;

        // ゲージを作り直す前に既存ゲージを非表示
        ClearGauges();

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

    void CreateGaugesFromBoss() {
        if (bossSc is null) return;

        float maxHp = 0.0f;
        if (!bossSc.GetVariable("maxHp", maxHp)) return;

        CreateGauges(maxHp);
    }

    void ClearGauges() {
        for (uint i = 0; i < gauges.length(); ++i) {
            Object@ gauge = gauges[i];
            if (gauge !is null) {
                gauge.SetActive(false);
            }
        }
        gauges.resize(0);
    }

    // Objectを渡さずに参照先を切り替える
    // Boss3.as / FinalBoss.asからCallMethod("SwitchToFinalBoss")などで呼ぶこともできる
    void SwitchToBoss3() {
        sourceMode = 1;
        hasPreviousHp = false;
        lastSourceMode = -1;
    }

    void SwitchToFinalBoss() {
        sourceMode = 2;
        hasPreviousHp = false;
        lastSourceMode = -1;
    }

    void Update() {
        // Boss3 / FinalBoss側が切り替えたモードを取得
        int sceneSource = sourceMode;
        if (GetScene().GetVariable("BossHPUI_Source", sceneSource)) {
            if (sceneSource >= 0 && sceneSource <= 2) {
                sourceMode = sceneSource;
            }
        }

        // 参照先が変わったらゲージを作り直す
        if (sourceMode != lastSourceMode) {
            ClearGauges();
            hasPreviousHp = false;

            if (sourceMode == 0) {
                FindBossScript();
                CreateGaugesFromBoss();
            }

            lastSourceMode = sourceMode;
        }

        float hp = 0.0f;
        float maxHp = 0.0f;
        if (!GetCurrentHp(hp, maxHp)) return;

        // Boss3 / FinalBossに切り替わった直後、最大HPが取得できてからゲージを生成
        if (gauges.length() == 0 && maxHp > 0.0f) {
            CreateGauges(maxHp);
        }

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
