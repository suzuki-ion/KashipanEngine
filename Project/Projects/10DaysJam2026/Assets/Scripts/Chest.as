class Chest : ScriptComponentBehavior {

    [SerializeField, Tooltip("宝箱に入っている武器の名前 (例: Katana, Shuriken)")]
    string itemName = "Katana";

    [SerializeField, Tooltip("すでに開いているか")]
    bool isOpen = false;

    void Start() {
        if (!ShouldPersistProgress()) return;

        // セーブデータのファイルからの読み込みは、シーン内のどのスクリプトが最初に
        // Start()を迎えても一度だけ行われるようにする(Player.as参照)
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

    // シーン名+オブジェクトのUUIDを使ったセーブデータキー
    // (UUIDだけだとシーンを複製した際に複製元と同じUUIDを持つオブジェクトが存在しうるため、
    //  シーン名も含めて衝突を避ける)
    string GetSaveKey() const {
        return "chest_" + GetScene().GetName() + "_" + GetOwnerObject().GetUUID() + "_opened";
    }

    // 宝箱を開ける処理
    void Open() {
        if (isOpen) return;

        isOpen = true;
        Log("宝箱を開けた！");

        if (ShouldPersistProgress()) {
            GetScene().SetGlobalVariable(GetSaveKey(), true);
        }

        PlayOpenAnimation();
    }

    // 開封用のアニメーション再生(Open()時・セーブデータから開封済み状態を復元した時の両方で使う)
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
