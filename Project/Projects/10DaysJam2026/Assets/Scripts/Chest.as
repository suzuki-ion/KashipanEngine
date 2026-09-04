class Chest : ScriptComponentBehavior {

    [SerializeField, Tooltip("宝箱に入っている武器の名前 (例: Katana, Shuriken)")]
    string itemName = "Katana";

    [SerializeField, Tooltip("すでに開いているか")]
    bool isOpen = false;

    void Start() {
    }

    void Update() {
    }

    void End() {
    }

    // 宝箱を開ける処理
    void Open() {
        if (isOpen) return;

        isOpen = true;
        Log("宝箱を開けた！");

        // 開封用のアニメーション再生
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