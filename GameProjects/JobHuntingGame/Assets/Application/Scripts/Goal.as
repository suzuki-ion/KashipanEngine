// ゴール用スクリプト。
// プレイヤーが触れた際、タイトルシーンへ戻る。
//
// 前提（エディター側で設定が必要）:
//   - このスクリプトを持つオブジェクトにコライダー（Box/Sphere等）を追加しておくこと
//   - プレイヤーを物理的に押し戻さないよう、コライダーの isTrigger を true にしておくことを推奨する
//   - nextSceneName に遷移先のシーン名を指定しておくこと

class Goal : ScriptComponentBehavior {
    [SerializeField, Tooltip("接触時に遷移するシーン名")]
    string nextSceneName = "TitleScene";

    bool sceneChangeRequested = false;

    Tag playerColliderTag = Tag("PlayerSphere");

    void OnCollisionEnter(const HitInfo &in hit) {
        if (sceneChangeRequested) return;
        if (hit.otherObject is null) return;
        if (hit.otherCollider is null) return;
        if (hit.otherCollider.GetTag() != playerColliderTag) {
            // Box形態ではGroundBoxタグになるため、Playerスクリプトでも判定する。
            ScriptComponent@ playerScript;
            if (!hit.otherObject.GetComponent(@playerScript)) return;
            string scriptPath = playerScript.GetScriptPath();
            if (scriptPath != "Assets\\Application\\Scripts\\Player\\Player.as"
                && scriptPath != "Assets/Application/Scripts/Player/Player.as") return;
        }

        GoToNextScene();
    }

    void GoToNextScene() {
        if (nextSceneName.length() == 0) return;

        Scene@ scene = GetScene();
        if (scene is null) return;

        scene.SetNextSceneName(nextSceneName);
        sceneChangeRequested = scene.ChangeToNextScene();
    }
}
