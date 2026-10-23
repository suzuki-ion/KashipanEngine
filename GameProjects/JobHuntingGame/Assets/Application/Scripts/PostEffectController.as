// ゲーム中のポストエフェクト制御用スクリプト
// プレイヤーの状態やカメラの位置に応じて、ポストエフェクトのパラメータを調整する

class PostEffectController : ScriptComponentBehavior {
    [SerializeField, Tooltip("プレイヤーのオブジェクト")]
    Object@ playerObject;
    [SerializeField, Tooltip("カメラのオブジェクト")]
    Object@ cameraObject;
    [SerializeField, Tooltip("ポストエフェクトが付与されているスクリーン用オブジェクト")]
    Object@ screenObject;
    [SerializeField, Tooltip("画面端の選択セルにも正確に合焦させるためカメラ空間の奥行きを使用する")]
    bool useViewDepth = false;
    [SerializeField, Tooltip("焦点補間の追従速度。0は即時反映")]
    float focusResponse = 0;
    float currentFocus = 0;
    bool focusInitialized = false;

    float BlendFocus(float current, float target, float dt) {
        if (focusResponse <= 0) return target;
        if (dt <= 0) return current;
        return current + (target - current) * (1 - Exp(-focusResponse * dt));
    }

    void Update() {
        DoFController();
    }

    void DoFController() {
        if (playerObject is null || cameraObject is null || screenObject is null) return;

        // プレイヤーの位置とカメラの位置を取得
        Transform@ playerTransform = playerObject.GetTransform();
        Transform@ cameraTransform = cameraObject.GetTransform();
        if (playerTransform is null || cameraTransform is null) return;

        Vector3 playerPos = playerTransform.GetWorldPosition();
        Vector3 cameraPos = cameraTransform.GetWorldPosition();

        // カメラからプレイヤーまでの距離を計算
        float distance = (playerPos - cameraPos).Length();
        if (useViewDepth) {
            // DoFシェーダーの線形深度に合わせ、斜めの距離ではなく視線方向の奥行きを使う。
            Matrix4x4 inverse = cameraTransform.GetWorldMatrix().Inverse();
            Vector3 inView = playerPos.Transform(inverse);
            distance = inView.z;
            if (distance < 0.1f) distance = 0.1f;
        }

        // ポストエフェクトのパラメータを設定
        DepthOfFieldEffect@ dof = null;
        if (screenObject.GetComponent(@dof)) {
            if (!focusInitialized) { currentFocus = distance; focusInitialized = true; }
            else currentFocus = BlendFocus(currentFocus, distance, GetDeltaTime());
            dof.SetFocusDistance(currentFocus);
        }
    }
}
