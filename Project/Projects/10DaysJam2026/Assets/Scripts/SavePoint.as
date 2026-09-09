// セーブ地点にアタッチするコンポーネント。
// プレイヤーと接触している間に指定した入力コマンドが押されたら、シーン変数
// "saveRequested"をtrueにする。実際の保存処理(Player.as参照)はこのシーン変数を
// 監視している側が行う。プレイヤーへの直接参照を持たないため、セーブ処理を行う
// スクリプトが増えても(あるいは差し替わっても)このコンポーネント側の変更は不要。
class SavePoint : ScriptComponentBehavior {
    [Header("会話ボックス")]
    [SerializeField, Tooltip("「セーブしました」等を表示するDialogueBoxスクリプトがアタッチされたオブジェクト(未設定なら表示しない)")]
    Object@ dialogueBoxObject;

    [Header("判定設定")]
    [SerializeField, Tooltip("接触相手(プレイヤー)のコライダーに付いているタグ")]
    string playerTag = "Player";

    [SerializeField, Tooltip("セーブを実行するための入力コマンド名")]
    string saveCommandName = "Bottom";

    // セーブ演出用。自身にParticleSystem2Dが付いていれば、セーブ要求時に再生する
    ParticleSystem2D@ particle;

    void Start() {
        GetComponent(@particle);
    }

    void Update() {
    }

    // プレイヤーと接触している間、毎フレーム呼ばれる
    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherCollider.GetTag() != playerTag) return;

        // 会話中(isDialogueActive)はセーブ要求を出さない。DialogueBoxの「次へ進める」入力コマンドと
        // セーブ実行の入力コマンドが同じ(Bottom等)場合、会話送り操作で誤ってセーブ要求や
        // 会話ボックスの再表示が発生してしまうのを防ぐ(Player.as等と同じチェック)
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        if (!IsCommandTriggered(saveCommandName)) return;

        // セーブ処理側(Player.as等)がプレイヤー自身ではなくこのセーブ地点の座標を記録できるよう、
        // 要求と合わせて自身の座標もシーン変数で渡す
        Transform@ tf = GetTransform();
        if (tf !is null) {
            GetScene().SetVariable("saveRequestedPosition", tf.GetTranslate());
        }
        GetScene().SetVariable("saveRequested", true);

        if (particle !is null) {
            particle.Play();
        }

        StartDialogue();
    }

    // dialogueBoxObjectにアタッチされたDialogueBoxのisVisibleを有効化する(NPCTalker.asと同じ方式)。
    // 表示するテキスト自体はdialogueBoxObject側のdialogues配列(Inspector)で「セーブしました」等を設定しておく
    void StartDialogue() {
        if (dialogueBoxObject is null) return;

        ScriptComponent@ sc;
        if (!dialogueBoxObject.GetComponent(@sc)) return;

        sc.SetVariable("isVisible", true);
    }
}
