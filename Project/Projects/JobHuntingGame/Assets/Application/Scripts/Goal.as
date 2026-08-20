// ゴール用スクリプト。
// プレイヤーが触れた際、指定したシーンへ切り替える（現状は単純にシーン遷移するだけ）。
//
// 前提（エディター側で設定が必要）:
//   - このスクリプトを持つオブジェクトにコライダー（Box/Sphere等）を追加しておくこと
//   - プレイヤーを物理的に押し戻さないよう、コライダーの isTrigger を true にしておくことを推奨する
//   - nextSceneName に遷移先のシーン名を指定しておくこと

class Goal : ScriptComponentBehavior {
    [SerializeField, Tooltip("接触時に遷移するシーン名")]
    string nextSceneName = "";

    Tag playerColliderTag = Tag("PlayerSphere");

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider is null || hit.otherCollider.GetTag() != playerColliderTag) return;

        GoToNextScene();
    }

    void GoToNextScene() {
        if (nextSceneName.length() == 0) return;

        Scene@ scene = GetScene();
        if (scene is null) return;

        scene.SetNextSceneName(nextSceneName);
        scene.ChangeToNextScene();
    }
}
