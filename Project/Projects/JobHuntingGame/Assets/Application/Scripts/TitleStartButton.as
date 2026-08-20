// タイトルシーンの「ゲーム開始」ボタン用スクリプト。
// 指定したUIButtonがクリックされた際、指定したシーンへ切り替える。
//
// 前提（エディター側で設定が必要）:
//   - buttonObject に UIButton コンポーネントを持つオブジェクトを指定しておくこと
//   - nextSceneName に遷移先のシーン名を指定しておくこと

class TitleStartButton : ScriptComponentBehavior {
    [SerializeField, Tooltip("クリックを監視するUIButtonを持つオブジェクト")]
    Object@ buttonObject;

    [SerializeField, Tooltip("クリック時に遷移するシーン名")]
    string nextSceneName = "";

    void Update() {
        if (buttonObject is null) return;

        UIButton@ button;
        if (!buttonObject.GetComponent(@button)) return;
        if (!button.IsClicked()) return;

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
