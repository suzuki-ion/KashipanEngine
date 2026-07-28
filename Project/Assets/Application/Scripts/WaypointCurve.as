// 指定したオブジェクト群を順に通る滑らかな曲線を自動計算するスクリプト。
// Catmull-Romスプライン（各点を必ず通過し、C1連続になる補間曲線）で、waypointsのTransform位置を
// 順番に結ぶ曲線を構築する。他のスクリプトはGetComponent(@curve)でこのコンポーネントを取得し、
// Evaluate(t)（t: 0〜1、始点から終点）やGetPositionAtDistance()を呼んで曲線上の座標を求められる。
//
// 使用例（別のスクリプトから）:
//   WaypointCurve@ curve;
//   if (someObject.GetComponent(@curve)) {
//       Vector3 pos = curve.Evaluate(0.5f); // 曲線の中間地点
//   }

class WaypointCurve : ScriptComponentBehavior {
    [SerializeField, Tooltip("曲線が順に通るオブジェクト群（Transformの位置を使用する）")]
    array<Object@>@ waypoints = null;
    [SerializeField, Tooltip("始点と終点をつないで閉じた曲線にするか")]
    bool loop = false;
    [SerializeField, Range(2, 64), Tooltip("区間ごとの分割数。大きいほど曲線の精度・滑らかさが上がる")]
    uint samplesPerSegment = 16;

    // --- 実行時状態（保存不要） ---
    array<Vector3>@ controlPoints = null;
    array<Vector3>@ sampledPoints = null;
    float totalLength = 0.0f;

    void Start() {
        RebuildCurve();
    }

    // waypointsやその位置を実行時に変更した場合、変更後に呼ぶと曲線を再計算できる
    void RebuildCurve() {
        if (controlPoints is null) @controlPoints = array<Vector3>();
        if (sampledPoints is null) @sampledPoints = array<Vector3>();
        controlPoints.resize(0);
        sampledPoints.resize(0);
        totalLength = 0.0f;

        for (uint i = 0; i < waypoints.length(); i++) {
            if (waypoints[i] is null) continue;
            Transform@ tf = waypoints[i].GetTransform();
            if (tf is null) continue;
            controlPoints.insertLast(tf.GetTranslate());
        }

        const uint count = controlPoints.length();
        if (count == 0) return;
        if (count == 1) {
            sampledPoints.insertLast(controlPoints[0]);
            return;
        }

        // ループ時はcount区間（最後の点から最初の点へも繋ぐ）、そうでなければcount-1区間
        const uint segmentCount = loop ? count : count - 1;
        for (uint seg = 0; seg < segmentCount; seg++) {
            Vector3 p0 = GetControlPoint(int(seg) - 1);
            Vector3 p1 = GetControlPoint(int(seg));
            Vector3 p2 = GetControlPoint(int(seg) + 1);
            Vector3 p3 = GetControlPoint(int(seg) + 2);

            // 最後の区間（非ループ時のみ）は終点ちょうどのt=1.0も含めてサンプルする
            const bool isLastSegment = (seg == segmentCount - 1) && !loop;
            const uint sampleCount = isLastSegment ? samplesPerSegment + 1 : samplesPerSegment;
            for (uint s = 0; s < sampleCount; s++) {
                const float t = float(s) / float(samplesPerSegment);
                sampledPoints.insertLast(CatmullRom(p0, p1, p2, p3, t));
            }
        }

        for (uint i = 1; i < sampledPoints.length(); i++) {
            totalLength += (sampledPoints[i] - sampledPoints[i - 1]).Length();
        }
    }

    // 制御点をインデックスで取得する（範囲外は、loop時は周回・非loop時は端の点を複製して扱う）
    Vector3 GetControlPoint(int index) {
        const int count = int(controlPoints.length());
        if (loop) {
            const int wrapped = ((index % count) + count) % count;
            return controlPoints[wrapped];
        }
        const int clamped = index < 0 ? 0 : (index >= count ? count - 1 : index);
        return controlPoints[clamped];
    }

    Vector3 CatmullRom(const Vector3 &in p0, const Vector3 &in p1, const Vector3 &in p2, const Vector3 &in p3, float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return 0.5f * (
            (2.0f * p1) +
            (p2 - p0) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
            (3.0f * p1 - p0 - 3.0f * p2 + p3) * t3
        );
    }

    //==================================================
    // 曲線の評価
    //==================================================

    /// @brief 曲線全体をt(0〜1、始点から終点)で評価した座標を取得する
    Vector3 Evaluate(float t) {
        const uint pointCount = sampledPoints.length();
        if (pointCount == 0) return Vector3(0.0f, 0.0f, 0.0f);
        if (pointCount == 1) return sampledPoints[0];

        t = Clamp(t, 0.0f, 1.0f);
        const float scaled = t * float(pointCount - 1);
        uint index = uint(scaled);
        if (index >= pointCount - 1) index = pointCount - 2;
        const float localT = scaled - float(index);
        return Math::Lerp(sampledPoints[index], sampledPoints[index + 1], localT);
    }

    /// @brief 曲線の総距離（サンプル点列に沿った弧長の近似）を取得する
    float GetLength() const { return totalLength; }

    /// @brief 始点からの距離で曲線上の座標を取得する（等速で曲線上を移動させたい場合に使う）
    Vector3 GetPositionAtDistance(float distance) {
        const uint pointCount = sampledPoints.length();
        if (pointCount == 0) return Vector3(0.0f, 0.0f, 0.0f);
        if (pointCount == 1 || totalLength <= 0.0f) return sampledPoints[0];

        distance = Clamp(distance, 0.0f, totalLength);
        float accumulated = 0.0f;
        for (uint i = 1; i < pointCount; i++) {
            const float segLength = (sampledPoints[i] - sampledPoints[i - 1]).Length();
            if (accumulated + segLength >= distance || i == pointCount - 1) {
                const float localT = segLength > 0.0f ? (distance - accumulated) / segLength : 0.0f;
                return Math::Lerp(sampledPoints[i - 1], sampledPoints[i], Clamp(localT, 0.0f, 1.0f));
            }
            accumulated += segLength;
        }
        return sampledPoints[pointCount - 1];
    }

    /// @brief サンプル済みの曲線上の点数を取得する（デバッグ表示や独自の分割処理に使う）
    uint GetSampledPointCount() const { return sampledPoints.length(); }
    /// @brief サンプル済みの曲線上の点を取得する
    Vector3 GetSampledPoint(uint index) const { return sampledPoints[index]; }
}
