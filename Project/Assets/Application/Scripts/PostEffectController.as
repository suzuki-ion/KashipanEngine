// ゲーム中のポストエフェクト制御用スクリプト
// プレイヤーの状態やカメラの位置に応じて、ポストエフェクトのパラメータを調整する

class PostEffectController : ScriptComponentBehavior {
    [SerializeField, Tooltip("プレイヤーのオブジェクト")]
    Object@ playerObject;
    [SerializeField, Tooltip("カメラのオブジェクト")]
    Object@ cameraObject;
    [SerializeField, Tooltip("ポストエフェクトが付与されているスクリーン用オブジェクト")]
    Object@ screenObject;

    void Update() {
        DoFController();
    }

    void DoFController() {
        if (playerObject is null || cameraObject is null || screenObject is null) return;

        // プレイヤーの位置とカメラの位置を取得
        Transform@ playerTransform = playerObject.GetTransform();
        Transform@ cameraTransform = cameraObject.GetTransform();
        if (playerTransform is null || cameraTransform is null) return;

        Vector3 playerPos = playerTransform.GetTranslate();
        Vector3 cameraPos = cameraTransform.GetTranslate();

        // カメラからプレイヤーまでの距離を計算
        float distance = (playerPos - cameraPos).Length();

        // ポストエフェクトのパラメータを設定
        DepthOfFieldEffect@ dof = null;
        if (screenObject.GetComponent(@dof)) {
            Log("aaaa");
            dof.SetFocusDistance(distance);
        }
    }
}