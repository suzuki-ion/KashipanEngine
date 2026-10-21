// 床の上に乗っているオブジェクトのZ座標を、床の中心Zへ滑らかに寄せる。
// このスクリプトを床オブジェクトへ追加し、床側のコライダーを通常の衝突判定として使用する。
class CenterRiderOnFloor : ScriptComponentBehavior {
    [SerializeField, Tooltip("床の中心Zへ加算するワールド座標オフセット")]
    float centerZOffset = 0.0f;
    [SerializeField, Tooltip("中心へ補間する速さ。大きいほど素早く中心へ寄る")]
    float interpolationSpeed = 5.0f;
    [SerializeField, Tooltip("上面への接触と判定する法線Yの閾値")]
    float riderNormalThreshold = 0.4f;
    // TagはSerializeFieldの対応型に含まれないため、文字列配列として保持し比較時にTag化する
    [SerializeField, Tooltip("この処理を行わない相手オブジェクトのタグ名（複数指定可）")]
    array<string>@ excludedTagNames = null;

    void OnCollisionStay(const HitInfo &in hit) {
        CenterRider(hit);
    }

    void CenterRider(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (IsExcludedTag(hit.otherObject.GetTag())) return;
        // HitInfo.normalは相手から自分へ向く。床から見て上にいる相手との接触法線は下向きになる
        if (hit.normal.y > -riderNormalThreshold) return;

        Transform@ floorTransform = GetTransform();
        Transform@ riderTransform;
        if (floorTransform is null || !hit.otherObject.GetComponent(@riderTransform)) return;

        const float dt = GetDeltaTime() * GetGameSpeed();
        const float interpolationAmount = Clamp(interpolationSpeed * dt, 0.0f, 1.0f);
        if (interpolationAmount <= 0.0f) return;

        const float targetWorldZ = floorTransform.GetWorldPosition().z + centerZOffset;
        const float currentWorldZ = riderTransform.GetWorldPosition().z;
        Vector3 riderLocalPosition = riderTransform.GetTranslate();

        // ワールドZの差分をローカル座標へ加算する。通常のルート階層にあるゲームプレイ
        // オブジェクトではローカル軸とワールド軸が一致するため、親の平行移動があっても追従できる
        riderLocalPosition.z += (targetWorldZ - currentWorldZ) * interpolationAmount;
        riderTransform.SetTranslate(riderLocalPosition);
    }

    // 相手のタグがexcludedTagNamesに含まれるかどうか
    bool IsExcludedTag(Tag otherTag) {
        for (uint i = 0; i < excludedTagNames.length(); i++) {
            if (Tag(excludedTagNames[i]) == otherTag) return true;
        }
        return false;
    }
}
