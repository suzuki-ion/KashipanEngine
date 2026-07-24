// プレイヤー用チェックポイント。
// プレイヤーが触れた際、このオブジェクトの位置をシーン変数「CheckpointPosition」へ保存する。
// 優先順位（priority）を持ち、既に保存されているチェックポイントの優先順位（CheckpointPriority）
// の方が高い場合は上書きしない（先に進んだチェックポイントを、間違って手前のチェックポイントに
// 触れ直しても巻き戻さないようにするためのもの）。

class Checkpoint : ScriptComponentBehavior {
    [SerializeField, Tooltip("このチェックポイントの優先順位。数値が大きいほど優先される。既に保存されている優先順位の方が高い場合は上書きしない")]
    int priority = 0;

    Tag playerColliderTag = Tag("PlayerSphere");

    void OnCollisionEnter(const HitInfo &in hit) {
        TrySave(hit);
    }

    void TrySave(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherCollider is null || hit.otherCollider.GetTag() != playerColliderTag) return;

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
