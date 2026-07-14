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

    array<Tag> excludeTagList;

    void Start() {
        Log("TeleportFloor start: " + GetOwnerObject().GetName());
        for (uint i = 0; i < excludeTags.length(); ++i) {
            excludeTagList.insertLast(Tag(excludeTags[i]));
        }
    }
    
    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (targetObject is null) return;

        for (uint i = 0; i < excludeTagList.length(); ++i) {
            if (hit.otherObject.GetTag() == excludeTagList[i]) {
                return;
            }
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

        otherTransform.SetTranslate(targetTransform.GetTranslate() + offset);
    }
}
