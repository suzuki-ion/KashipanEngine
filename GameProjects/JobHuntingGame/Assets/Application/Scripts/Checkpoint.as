// プレイヤー用チェックポイント。
// プレイヤーが触れた際、このオブジェクトの位置をシーン変数「CheckpointPosition」へ保存する。
// 優先順位（priority）を持ち、既に保存されているチェックポイントの優先順位（CheckpointPriority）
// の方が高い場合は上書きしない（先に進んだチェックポイントを、間違って手前のチェックポイントに
// 触れ直しても巻き戻さないようにするためのもの）。
//
// ライトは開始時点では白色にしておき、プレイヤーが接触した瞬間に元々設定されていた色へ変化させる
// （lightObjectを指定した場合のみ）。

class Checkpoint : ScriptComponentBehavior {
    [SerializeField, Tooltip("このチェックポイントの優先順位。数値が大きいほど優先される。既に保存されている優先順位の方が高い場合は上書きしない")]
    int priority = 0;

    [SerializeField, Tooltip("接触時に角速度Yをアニメーションさせる、子オブジェクト（CheckPointタグの付いたRotationコンポーネントを持つオブジェクト）")]
    Object@ rotatingObject;

    [SerializeField, Tooltip("接触時に元の色へ変化させる、色付きのライトを持つオブジェクト。開始時はこのライトを白色にしておく")]
    Object@ lightObject;

    Tag playerColliderTag = Tag("PlayerSphere");

    // --- 実行時状態（保存不要） ---
    Vector4 originalLightColor;
    bool hasOriginalLightColor = false;

    // 角速度Yアニメーションの設定（接触のたびに 1→16 (InCubic) → 16→1 (OutCubic) を各0.5秒で行う）
    float kRotateAnimDuration = 0.5f;
    float kRotateSpeedMin = 1.0f;
    float kRotateSpeedMax = 16.0f;

    // 0: 停止中, 1: 1→16 (InCubic) 再生中, 2: 16→1 (OutCubic) 再生中
    int rotateAnimPhase = 0;
    float rotateAnimElapsed = 0.0f;

    void Start() {
        CacheAndWhitenLight();
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider is null || hit.otherCollider.GetTag() != playerColliderTag) return;

        TrySave();
        StartRotateAnimation();
        ActivateLightColor();
    }

    // ライトの現在の色を元の色として覚えておき、表示上は白色にしておく
    void CacheAndWhitenLight() {
        if (lightObject is null) return;

        Light@ light;
        if (!lightObject.GetComponent(@light)) return;

        originalLightColor = light.GetColor();
        hasOriginalLightColor = true;
        light.SetColor(Vector4(1.0f, 1.0f, 1.0f, originalLightColor.w));
    }

    // ライトの色を、覚えておいた元の色へ戻す
    void ActivateLightColor() {
        if (lightObject is null || !hasOriginalLightColor) return;

        Light@ light;
        if (!lightObject.GetComponent(@light)) return;

        light.SetColor(originalLightColor);
    }

    void StartRotateAnimation() {
        rotateAnimPhase = 1;
        rotateAnimElapsed = 0.0f;
    }

    void Update() {
        if (rotateAnimPhase == 0) return;
        if (rotatingObject is null) {
            rotateAnimPhase = 0;
            return;
        }

        Rotation@ rotation;
        if (!rotatingObject.GetComponent(@rotation)) {
            rotateAnimPhase = 0;
            return;
        }

        float dt = GetDeltaTime() * GetGameSpeed();
        if (dt < 0.0f) dt = 0.0f;
        rotateAnimElapsed += dt;

        float t = Easing::Normalize01(rotateAnimElapsed, 0.0f, kRotateAnimDuration);
        float newY = (rotateAnimPhase == 1)
            ? Easing::Eased(kRotateSpeedMin, kRotateSpeedMax, t, EaseType::EaseInCubic)
            : Easing::Eased(kRotateSpeedMax, kRotateSpeedMin, t, EaseType::EaseOutCubic);

        Vector3 angularVelocity = rotation.GetAngularVelocity();
        angularVelocity.y = newY;
        rotation.SetAngularVelocity(angularVelocity);

        if (rotateAnimElapsed >= kRotateAnimDuration) {
            if (rotateAnimPhase == 1) {
                rotateAnimPhase = 2;
                rotateAnimElapsed = 0.0f;
            } else {
                rotateAnimPhase = 0;
            }
        }
    }

    void TrySave() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        Scene@ scene = GetScene();
        if (scene is null) return;

        int savedPriority;
        bool hasSaved = scene.GetVariable("CheckpointPriority", savedPriority);
        // 既に保存されている優先順位の方が高い場合は上書きしない
        if (hasSaved && savedPriority > priority) return;

        scene.SetVariable("CheckpointPosition", tf.GetTranslate());
        scene.SetVariable("CheckpointPriority", priority);
    }
}
