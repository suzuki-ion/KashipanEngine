// 触れた瞬間に、触れたオブジェクトを指定オブジェクトの位置へ瞬間移動させる床。
//
// 前提（エディター側で設定が必要）:
//   - このスクリプトを持つオブジェクトにコライダー（Box/Sphere等）を追加しておくこと
//   - 物理的に押し戻されないよう、コライダーの isTrigger を true にしておくことを推奨する
//   - targetObject にテレポート先のオブジェクトを指定しておくこと
//     （targetObject自身を動かせば、後からテレポート先を変更できる）
class TeleportFloor : ScriptComponentBehavior {
    [SerializeField, Tooltip("テレポート先のオブジェクト（このオブジェクトの現在位置へ移動させる）")]
    Object@ targetObject;
    [SerializeField, Tooltip("テレポート先座標（オブジェクト指定が無い場合はこの座標へ移動させる）")]
    Vector3 targetPosition;
    [SerializeField, Tooltip("テレポート座標のオフセット（targetObjectの座標に加算される）")]
    Vector3 offset;

    [SerializeField, Tooltip("テレポート除外タグ（このタグを持つオブジェクトはテレポートしない）")]
    array<string>@ excludeTags = null;

    array<Tag>@ excludeTagList = array<Tag>();
    [SerializeField, Tooltip("空欄の場合は制限なし")]
    string requiredObjectTag = "";
    [SerializeField, Tooltip("衝突通知後に転送し、転送した1フレームだけCCDを停止する（壁で囲まれた遠征向け）")]
    bool deferTeleport = false;
    Object@ pendingObject;
    Vector3 pendingPosition;
    array<Collider@>@ suspendedColliders = array<Collider@>();
    array<bool>@ previousCCD = array<bool>();

    void SuspendCCD(Collider@ collider) {
        if (collider is null) return;
        suspendedColliders.insertLast(collider); previousCCD.insertLast(collider.IsContinuousDetection());
        collider.SetContinuousDetection(false);
    }
    void Update() {
        for (uint i = 0; i < suspendedColliders.length(); ++i)
            suspendedColliders[i].SetContinuousDetection(previousCCD[i]);
        suspendedColliders.resize(0); previousCCD.resize(0);
        if (pendingObject is null) return;
        // 衝突通知中の移動は残りの接触応答に押し戻され、CCDは経路中の壁にも当たる。
        // 次の更新で移動し、到着後の位置を物理側へ一度同期してからCCDを戻す。
        SphereCollider@ sphere; BoxCollider@ box; MeshCollider@ mesh;
        if (pendingObject.GetComponent(@sphere)) SuspendCCD(sphere);
        if (pendingObject.GetComponent(@box)) SuspendCCD(box);
        if (pendingObject.GetComponent(@mesh)) SuspendCCD(mesh);
        pendingObject.GetTransform().SetTranslate(pendingPosition);
        @pendingObject = null;
    }

    void Start() {
        Log("TeleportFloor start: " + GetOwnerObject().GetName());
        excludeTagList.resize(0);
        if (excludeTags is null) return;
        for (uint i = 0; i < excludeTags.length(); ++i) {
            excludeTagList.insertLast(Tag(excludeTags[i]));
        }
    }
    
    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (requiredObjectTag != "" && hit.otherObject.GetTagName() != requiredObjectTag) return;

        for (uint i = 0; i < excludeTagList.length(); ++i) {
            if (hit.otherObject.GetTag() == excludeTagList[i]) {
                return;
            }
        }
        if (deferTeleport) {
            @pendingObject = hit.otherObject;
            pendingPosition = targetObject is null ? targetPosition + offset : targetObject.GetTransform().GetWorldPosition() + offset;
            return;
        }
        
        if (targetObject is null) {
            Transform@ otherTransform;
            if (!hit.otherObject.GetComponent(@otherTransform)) return;

            otherTransform.SetTranslate(targetPosition + offset);
            return;
        }
        
        Transform@ targetTransform;
        if (!targetObject.GetComponent(@targetTransform)) return;

        Transform@ otherTransform;
        if (!hit.otherObject.GetComponent(@otherTransform)) return;

        otherTransform.SetTranslate(targetTransform.GetWorldPosition() + offset);
    }
}
