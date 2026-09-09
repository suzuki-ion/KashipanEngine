// ゲームオーバー画面を制御するコンポーネント。
// Player.as側で死亡が確定し、一定時間が経過するとシーン変数"gameOverRequested"がtrueになる。
// これを監視してゲームオーバー画面を表示し、入力コマンドが押されたら現在のシーンをリロードする。
// リロード後はPlayer.LoadProgress()の座標復元処理により、直近でセーブしたSavePointの位置から
// 再開する(未セーブならシーン読み込み時の初期位置になる)。
// Playerへの直接参照は持たない(SavePoint.as等と同じ疎結合の設計方針)。
//
// 前提(エディター側で設定が必要):
//   - gameOverUIObjectに、ゲームオーバー表示用オブジェクト(背景/テキスト等をぶら下げた親)を指定しておくこと
//     (このスクリプト自身はSetActive(true/false)の切り替えのみを行う)
class GameOverMenu : ScriptComponentBehavior {
    [Header("表示制御")]
    [SerializeField, Tooltip("ゲームオーバー画面のルートオブジェクト(この下にBG/テキスト等をぶら下げておく)")]
    Object@ gameOverUIObject;

    [Header("入力")]
    [SerializeField, Tooltip("ゲームオーバー画面を閉じてやり直すための入力コマンド名")]
    string confirmCommandName = "Bottom";

    [SerializeField, Tooltip("表示直後の誤入力で即決定してしまわないための、入力を受け付け始めるまでの遅延秒数")]
    float inputDelay = 0.3f;

    // --- 実行時状態(保存不要) ---
    bool isShowing = false;
    float shownTimer = 0.0f;

    void Start() {
        if (gameOverUIObject !is null) {
            gameOverUIObject.SetActive(false);
        }
    }

    // シーン終了時、表示中のままだった場合に備えてシーン変数を消費しておく
    // (次にこのシーンを読み込んだ際、古いgameOverRequestedが残らないようにする)
    void End() {
        if (isShowing) {
            GetScene().SetVariable("gameOverRequested", false);
        }
    }

    void Update() {
        bool requested = false;
        GetScene().GetVariable("gameOverRequested", requested);

        if (requested && !isShowing) {
            isShowing = true;
            shownTimer = 0.0f;
            if (gameOverUIObject !is null) {
                gameOverUIObject.SetActive(true);
            }
        }

        if (!isShowing) return;

        shownTimer += GetDeltaTime();
        if (shownTimer < inputDelay) return;

        if (IsCommandTriggered(confirmCommandName)) {
            Restart();
        }
    }

    // 現在のシーンをリロードして、セーブ地点の位置からやり直す
    void Restart() {
        isShowing = false;
        GetScene().SetVariable("gameOverRequested", false);
        if (gameOverUIObject !is null) {
            gameOverUIObject.SetActive(false);
        }

        Scene@ scene = GetScene();
        if (scene is null) return;

        scene.SetNextSceneName(scene.GetName());
        scene.ChangeToNextScene();
    }
}
