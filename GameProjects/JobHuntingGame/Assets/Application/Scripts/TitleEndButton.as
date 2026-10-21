// タイトルシーンの「ゲーム終了」ボタン用スクリプト。
// 指定したUIButtonがクリックされた際、アプリケーションを終了する。
//
// 前提（エディター側で設定が必要）:
//   - buttonObject に UIButton コンポーネントを持つオブジェクトを指定しておくこと

class TitleEndButton : ScriptComponentBehavior {
    [SerializeField, Tooltip("クリックを監視するUIButtonを持つオブジェクト")]
    Object@ buttonObject;

    void Update() {
        if (buttonObject is null) return;

        UIButton@ button;
        if (!buttonObject.GetComponent(@button)) return;
        if (!button.IsClicked()) return;

        Scene@ scene = GetScene();
        if (scene is null) return;

        scene.RequestExitGameLoop();
    }
}
