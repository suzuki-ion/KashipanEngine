// トリガー用の当たり判定オブジェクトにアタッチするコンポーネント。
// プレイヤーがこのオブジェクトの領域に進入した瞬間、入力コマンドを待たずに
// 指定した会話ボックス(DialogueBox)オブジェクトのisVisibleフラグを有効化して会話を開始する。
// ボス戦シーンの入り口に置いて、戦闘開始前の会話イベントを自動発生させる用途などを想定している。
class DialogueAreaTrigger : ScriptComponentBehavior {
    [Header("会話ボックス")]
    [SerializeField, Tooltip("DialogueBoxスクリプトがアタッチされたオブジェクト")]
    Object@ dialogueBoxObject;

    [Header("判定設定")]
    [SerializeField, Tooltip("接触相手(プレイヤー)のコライダーに付いているタグ")]
    string playerTag = "Player";

    [SerializeField, Tooltip("一度発動したら以降は再発動しないようにするか(ボスの会話イベント等、1回だけにしたい場合はtrue)")]
    bool triggerOnce = true;

    // 発動済みかどうか(triggerOnceがtrueの場合のみ使用)
    bool hasTriggered = false;

    void Start() {
        // triggerOnce=trueの場合、セーブ&ロードやシーン再訪問をまたいでも再発動しないよう、
        // 発動済みかどうかをオブジェクトのUUIDをキーにしてセーブデータから復元する
        if (!triggerOnce) return;
        if (!ShouldPersistProgress()) return;

        // セーブデータのファイルからの読み込みは、シーン内のどのスクリプトが最初に
        // Start()を迎えても一度だけ行われるようにする(Player.as参照)
        bool hasLoadedSaveThisSession = false;
        GetScene().GetGlobalVariable("hasLoadedSaveThisSession", hasLoadedSaveThisSession);
        if (!hasLoadedSaveThisSession) {
            GetScene().LoadGlobalVariables();
            GetScene().SetGlobalVariable("hasLoadedSaveThisSession", true);
        }

        bool savedTriggered = false;
        if (GetScene().GetGlobalVariable(GetSaveKey(), savedTriggered) && savedTriggered) {
            hasTriggered = true;
        }
    }

    void Update() {
    }

    bool ShouldPersistProgress() const {
        return GetScene().IsPlaying() || !IsEditorBuild();
    }

    // オブジェクトのUUIDを使ったセーブデータキー(シーン内に複数のトリガーがあっても衝突しない)
    string GetSaveKey() const {
        return "dialogueTrigger_" + GetOwnerObject().GetUUID() + "_fired";
    }

    // プレイヤーが領域に進入した瞬間に呼ばれる
    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherCollider.GetTag() != playerTag) return;
        if (triggerOnce && hasTriggered) return;

        hasTriggered = true;
        if (triggerOnce && ShouldPersistProgress()) {
            GetScene().SetGlobalVariable(GetSaveKey(), true);
        }
        StartDialogue();
    }

    // dialogueBoxObjectにアタッチされたDialogueBoxのisVisibleを有効化する
    void StartDialogue() {
        if (dialogueBoxObject is null) return;

        ScriptComponent@ sc;
        if (!dialogueBoxObject.GetComponent(@sc)) return;

        sc.SetVariable("isVisible", true);
    }
}
