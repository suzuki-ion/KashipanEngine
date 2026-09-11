// NPCにアタッチする会話トリガー用コンポーネント。
// プレイヤーと接触している間に指定した入力コマンドが押されたら、
// 指定した会話ボックス(DialogueBox)オブジェクトのisVisibleフラグを有効化して会話を開始する。
class NPCTalker : ScriptComponentBehavior {
    [Header("会話ボックス")]
    [SerializeField, Tooltip("DialogueBoxスクリプトがアタッチされたオブジェクト")]
    Object@ dialogueBoxObject;

    [Header("判定設定")]
    [SerializeField, Tooltip("接触相手(プレイヤー)のコライダーに付いているタグ")]
    string playerTag = "Player";

    [SerializeField, Tooltip("プレイヤーと接触している間に会話を開始するための入力コマンド名")]
    string talkCommandName = "Bottom";

    void Start() {
    }

    void Update() {
    }

    // プレイヤーと接触している間、毎フレーム呼ばれる
    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherCollider.GetTag() != playerTag) return;
        // チェストの会話は、開封に成功した瞬間だけChest側から開始する。
        // 接触入力から直接開始すると、開封済みでも繰り返し表示されてしまう。
        if (IsAttachedToChest()) return;
        if (!IsCommandTriggered(talkCommandName)) return;

        StartDialogue();
    }

    bool IsAttachedToChest() {
        array<ScriptComponent@>@ scripts;
        if (!GetComponents(@scripts)) return false;

        for (uint i = 0; i < scripts.length(); ++i) {
            bool isOpen = false;
            if (scripts[i] !is null && scripts[i].GetVariable("isOpen", isOpen)) {
                return true;
            }
        }
        return false;
    }

    // dialogueBoxObjectにアタッチされたDialogueBoxのisVisibleを有効化する
    void StartDialogue() {
        if (dialogueBoxObject is null) return;

        ScriptComponent@ sc;
        if (!dialogueBoxObject.GetComponent(@sc)) return;

        sc.SetVariable("isVisible", true);
    }
}
