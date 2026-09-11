class MaxHpChest : ScriptComponentBehavior {

    [SerializeField, Tooltip("増加させる最大HP量")]
    float hpIncreaseAmount = 2.0f;

    [SerializeField, Tooltip("すでに開いているか")]
    bool isOpen = false;

    bool restoreOpenVisualPending = false;

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
            // SpriteAnimatorのStart()後に適用しないと、初期表示で上書きされる
            restoreOpenVisualPending = true;
        }
    }

    void Update() {
        if (restoreOpenVisualPending) {
            restoreOpenVisualPending = false;
            PlayOpenAnimation();
        }
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

        PlayTaggedAudio("Open");
        PlayOpenAnimation();
        StartAttachedDialogue();
    }

    // チェストに設定された会話は、開封に成功したこの瞬間だけ開始する
    void StartAttachedDialogue() {
        array<ScriptComponent@>@ scripts;
        if (!GetComponents(@scripts)) return;

        for (uint i = 0; i < scripts.length(); ++i) {
            Object@ dialogueBoxObject;
            if (scripts[i] !is null && scripts[i].GetVariable("dialogueBoxObject", @dialogueBoxObject)) {
                scripts[i].CallMethod("StartDialogue");
                return;
            }
        }
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
