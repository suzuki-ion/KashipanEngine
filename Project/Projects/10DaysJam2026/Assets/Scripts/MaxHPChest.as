class MaxHpChest : ScriptComponentBehavior {

    [SerializeField, Tooltip("増加させる最大HP量")]
    float hpIncreaseAmount = 2.0f;

    [SerializeField, Tooltip("すでに開いているか")]
    bool isOpen = false;

    void Start() {
        if (!ShouldPersistProgress()) return;

        bool hasLoadedSaveThisSession = false;
        GetScene().GetGlobalVariable("hasLoadedSaveThisSession", hasLoadedSaveThisSession);
        if (!hasLoadedSaveThisSession) {
            GetScene().LoadGlobalVariables();
            GetScene().SetGlobalVariable("hasLoadedSaveThisSession", true);
        }

        bool savedOpen = false;
        if (GetScene().GetGlobalVariable(GetSaveKey(), savedOpen) && savedOpen) {
            isOpen = true;
            PlayOpenAnimation();
        }
    }

    void Update() {
    }

    void End() {
    }

    bool ShouldPersistProgress() const {
        return GetScene().IsPlaying() || !IsEditorBuild();
    }

    // シーン名+オブジェクトUUIDによるセーブキー
    string GetSaveKey() const {
        return "max_hp_chest_" + GetScene().GetName() + "_" + GetOwnerObject().GetUUID() + "_opened";
    }

    // 宝箱を開ける処理
    void Open() {
        if (isOpen) return;

        isOpen = true;
        Log("最大HP上昇宝箱を開けた！");

        if (ShouldPersistProgress()) {
            GetScene().SetGlobalVariable(GetSaveKey(), true);
        }

        PlayOpenAnimation();
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