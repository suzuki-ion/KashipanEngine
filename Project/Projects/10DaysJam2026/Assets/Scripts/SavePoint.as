// セーブ地点にアタッチするコンポーネント。
// プレイヤーと接触している間に指定した入力コマンドが押されたら、シーン変数
// "saveRequested"をtrueにする。実際の保存処理(Player.as参照)はこのシーン変数を
// 監視している側が行う。プレイヤーへの直接参照を持たないため、セーブ処理を行う
// スクリプトが増えても(あるいは差し替わっても)このコンポーネント側の変更は不要。
class SavePoint : ScriptComponentBehavior {
    [Header("判定設定")]
    [SerializeField, Tooltip("接触相手(プレイヤー)のコライダーに付いているタグ")]
    string playerTag = "Player";

    [SerializeField, Tooltip("セーブを実行するための入力コマンド名")]
    string saveCommandName = "Bottom";

    void Start() {
    }

    void Update() {
    }

    // プレイヤーと接触している間、毎フレーム呼ばれる
    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherCollider.GetTag() != playerTag) return;
        if (!IsCommandTriggered(saveCommandName)) return;

        GetScene().SetVariable("saveRequested", true);
    }
}
