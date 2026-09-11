class HeartChest : ScriptComponentBehavior {

    [SerializeField, Tooltip("すでに開いているか")]
    bool isOpen = false;

    [SerializeField, Tooltip("放出する回復アイテムのオリジナルオブジェクト")]
    Object@ itemOriginal;

    [SerializeField, Tooltip("放出するアイテムの個数")]
    int spawnCount = 10;

    [SerializeField, Tooltip("左右方向への飛び出し力")]
    float spawnForceX = 80.0f;

    [SerializeField, Tooltip("上方への飛び出し力(最小)")]
    float minSpawnForceY = 100.0f;

    [SerializeField, Tooltip("上方への飛び出し力(最大)")]
    float maxSpawnForceY = 180.0f;

    [SerializeField, Tooltip("チェストの足元からの着地高さオフセット")]
    float floorOffset = -8.0f;

    void Start() {
        bool hasLoadedSaveThisSession = false;
        GetScene().GetGlobalVariable("hasLoadedSaveThisSession", hasLoadedSaveThisSession);
        if (!hasLoadedSaveThisSession) {
            GetScene().LoadGlobalVariables();
            GetScene().SetGlobalVariable("hasLoadedSaveThisSession", true);
        }
    }

    void Update() {
    }

    void End() {
    }

    // 宝箱を開ける処理
    void Open() {
        if (isOpen) return;

        isOpen = true;
        Log("回復宝箱を開けた！");

        PlayTaggedAudio("Open");
        PlayOpenAnimation();
        SpawnItems();
    }

    void PlayTaggedAudio(const string &in tagName) {
        array<AudioSource@>@ sources;
        if (!GetComponents(@sources)) return;
        for (uint i = 0; i < sources.length(); ++i) {
            if (sources[i] !is null && sources[i].GetTag() == tagName) {
                sources[i].SetActive(true);
                sources[i].Play();
            }
        }
    }

    // 回復アイテムを大量生成して飛び散らせる
    void SpawnItems() {
        if (itemOriginal is null) return;

        Transform@ chestTf = GetTransform();
        if (chestTf is null) return;

        Vector3 spawnPos = chestTf.GetTranslate();
        float targetMinY = spawnPos.y + floorOffset; // 着地高度の設定

        for (int i = 0; i < spawnCount; ++i) {
            Object@ cloneItem = GetScene().CloneObject(itemOriginal, "DroppedHeart");
            if (cloneItem !is null) {
                cloneItem.SetActive(true);

                Transform@ cloneTf = cloneItem.GetTransform();
                if (cloneTf !is null) {
                    // チェストの少し上から出現させる
                    cloneTf.SetTranslate(Vector3(spawnPos.x, spawnPos.y + 4.0f, spawnPos.z));
                }

                // 左右へ勢いよく振り分け
                float dir = (Random::Float(-1.0f, 1.0f) >= 0.0f) ? 1.0f : -1.0f;
                float vx = dir * Random::Float(20.0f, spawnForceX);
                float vy = Random::Float(minSpawnForceY, maxSpawnForceY);

                ScriptComponent@ itemSc;
                if (cloneItem.GetComponent(@itemSc)) {
                    itemSc.SetVariable("velocity", Vector2(vx, vy));
                    itemSc.SetVariable("minY", targetMinY);
                }
            }
        }
    }

    // 開封用アニメーション再生
    void PlayOpenAnimation() {
        array<ScriptComponent@>@ animScripts;
        if (GetComponents(@animScripts)) {
            for (int i = 0; i < animScripts.length(); ++i) {
                if (animScripts[i].GetTag() == "AnimatorSC") {
                    animScripts[i].CallMethod("PlayRow", 1);
                }
            }
        }
    }
}
