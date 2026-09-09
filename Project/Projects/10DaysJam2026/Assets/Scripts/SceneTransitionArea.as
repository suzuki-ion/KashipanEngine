// シーン遷移用のトリガー領域にアタッチするコンポーネント。
// playerTagを持つコライダーが領域へ進入するとDitherFadeで画面を覆い、
// フェードアウト完了後に指定シーンへ切り替える。
//
// 前提（エディター側で設定が必要）:
//   - このスクリプトと同じオブジェクトに、範囲を表すColliderを配置してisTrigger=trueにする
//   - nextSceneNameに遷移先のシーン名を指定する
//   - ditherFadeObjectにDitherFade.asがアタッチされたオブジェクトを指定する
//   - プレイヤー側の接触コライダーにplayerTag（既定値: Player）を設定する
//
// 遷移先でも画面を覆った状態からフェードインさせる場合は、遷移先シーンにも
// fadeInOnStart=trueのDitherFadeを配置する。
class SceneTransitionArea : ScriptComponentBehavior {
    [Header("シーン遷移")]
    [SerializeField, Tooltip("遷移先のシーン名")]
    string nextSceneName = "";

    [SerializeField, Tooltip("DitherFadeスクリプトがアタッチされたオブジェクト。未設定または取得不能の場合は即時遷移する")]
    Object@ ditherFadeObject;

    [Header("判定設定")]
    [SerializeField, Tooltip("接触相手(プレイヤー)のコライダーに付いているタグ")]
    string playerTag = "Player";

    // --- 参照コンポーネント(保存不要) ---
    ScriptComponent@ ditherFadeScript;

    // --- 遷移状態(保存不要) ---
    bool isTransitionRequested = false;
    bool isWaitingForFade = false;
    int fadeWaitFrames = 0;

    void Start() {
        if (ditherFadeObject !is null) {
            ditherFadeObject.GetComponent(@ditherFadeScript);
        }
    }

    void Update() {
        if (!isWaitingForFade) return;
        UpdateSceneTransition();
    }

    // プレイヤーがトリガー領域へ進入した瞬間に、1回だけ遷移処理を開始する。
    void OnCollisionEnter(const HitInfo &in hit) {
        if (isTransitionRequested) return;
        if (hit.otherCollider is null || hit.otherCollider.GetTag() != playerTag) return;
        if (nextSceneName.length() == 0) return;

        isTransitionRequested = true;
        BeginSceneTransition();
    }

    void BeginSceneTransition() {
        // TitleMenuControllerと同じく、DitherFadeが利用できる場合は
        // フェードアウト完了までシーン切り替えを待つ。
        if (ditherFadeScript !is null && ditherFadeScript.CallMethod("FadeOut")) {
            isWaitingForFade = true;
            fadeWaitFrames = 0;
            return;
        }

        ChangeSceneNow();
    }

    // FadeOut()とDitherFade.Update()の実行順に依存しないよう、最低1フレーム待機する。
    // fadeOutDuration=0の場合も2回目の確認で遷移を完了できる。
    void UpdateSceneTransition() {
        ++fadeWaitFrames;

        if (ditherFadeScript is null) {
            ChangeSceneNow();
            return;
        }

        bool isFading = false;
        if (!ditherFadeScript.CallMethod("IsFading") || !ditherFadeScript.GetLastReturnValue(isFading)) {
            ChangeSceneNow();
            return;
        }

        if (!isFading && fadeWaitFrames >= 2) {
            ChangeSceneNow();
        }
    }

    void ChangeSceneNow() {
        Scene@ scene = GetScene();
        if (scene is null || nextSceneName.length() == 0) return;

        isWaitingForFade = false;
        scene.SetNextSceneName(nextSceneName);
        scene.ChangeToNextScene();
    }
}
