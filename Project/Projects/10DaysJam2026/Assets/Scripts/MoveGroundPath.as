// 複数の通過点（ウェイポイント）を、任意のイージング・任意の所要時間で区間ごとに辿りながら
// 移動する地面。MoveGround（2点間の往復のみ、等速）よりも複雑な軌道を組みたい場合はこちらを使う。
//
// points[i] は「開始位置からのオフセット」「その点での回転（初期回転からのオフセット、度）」
// 「その区間（ひとつ前の点からこの点まで）の所要時間」「その区間のイージング」
// 「その点に到達してから次の区間へ進むまでの待ち時間」を持つ。位置・回転は同じ区間タイミング
// （duration/easeType/waitDuration）を共有し、常に同じタイミングで一緒に目的の点/向きへ到達する。
// points[0]の設定は「開始位置 → points[0]」の区間に使われる
// （points[i]の設定は常に「ひとつ前の点 → points[i]」の区間に対応する、という決まり）。
//
// MoveGroundと異なりVelocity/Rotationコンポーネントには依存せず、Transformへ直接位置・回転を
// 書き込む（イージングは区間内で瞬間速度が変化するため、速度コンポーネント経由で近似するより
// 直接指定した方が軌道を正確に再現できる）。回転の補間はオイラー角の単純な線形補間ではなく
// Math::Slerpで行う（軸のねじれや角度をまたぐ際の大回りを避けるため）。
// Player/Enemyをこの地面の動きに追従させたい場合は、MoveGroundと同様にこのオブジェクトへ
// PreTransform コンポーネントを追加しておくこと。

// 通過点1件分の設定（Inspectorの points 配列の要素として編集する）
[System.Serializable]
class MoveGroundPathPoint {
    [Tooltip("開始位置からのオフセット（ワールド軸基準）")]
    Vector3 offset = Vector3(0.0f, 0.0f, 0.0f);
    [Tooltip("ひとつ前の点（要素0なら開始位置）からこの点までの所要時間（秒）")]
    float duration = 1.0f;
    [Tooltip("ひとつ前の点からこの点までの区間で使うイージング")]
    EaseType easeType = EaseType::Linear;
    [Tooltip("この点に到達してから次の区間へ進むまでの待ち時間（秒）")]
    float waitDuration = 0.0f;
    [Tooltip("この点での回転（初期回転からのオフセット、度）")]
    Vector3 rotateOffset = Vector3(0.0f, 0.0f, 0.0f);
}

class MoveGroundPath : ScriptComponentBehavior {
    [SerializeField, Tooltip("順に辿る通過点。要素0の設定は「開始位置→要素0」の区間に使われる")]
    array<MoveGroundPathPoint@>@ points = null;
    [SerializeField, Tooltip("最後の点まで着いたら開始位置へ戻ってループするか")]
    bool loop = false;
    [SerializeField, Tooltip("ループ時、最後の点から開始位置へ戻る区間の所要時間（秒）")]
    float loopBackDuration = 1.0f;
    [SerializeField, Tooltip("ループ時、最後の点から開始位置へ戻る区間のイージング")]
    EaseType loopBackEaseType = EaseType::Linear;
    [SerializeField, Tooltip("ループ時、開始位置に到達してから最初の区間へ進むまでの待ち時間（秒）")]
    float loopBackWaitDuration = 0.0f;

    // --- 実行時状態（保存不要） ---
    Vector3 startPosition = Vector3(0.0f, 0.0f, 0.0f);
    // 開始時点の回転（ラジアン）。各点のrotateOffsetはこれを基準にしたオフセット
    Vector3 initialRotation = Vector3(0.0f, 0.0f, 0.0f);
    // 現在進行中の区間インデックス（0 = 開始位置→points[0]）。
    // 非ループでsegmentCount以上になったら全区間完了（以後Updateは何もしない）
    int currentSegment = 0;
    float segmentElapsed = 0.0f;
    // 区間移動完了後、waitDurationぶん待機中かどうか
    bool isWaiting = false;
    float waitTimer = 0.0f;

    void Start() {
        Transform@ tf = GetTransform();
        if (tf is null) return;
        startPosition = tf.GetTranslate();
        initialRotation = tf.GetRotate();
    }

    int GetPointCount() {
        return points is null ? 0 : int(points.length());
    }

    // index番目区間（ひとつ前の点→points[index]。ループ時、index==count は「最後の点→開始位置」の
    // 戻り区間を表す）の始点・終点・所要時間・イージングを取得する
    Vector3 GetSegmentStartPoint(int index) {
        if (index <= 0) return startPosition;
        return GetSegmentEndPoint(index - 1);
    }
    Vector3 GetSegmentEndPoint(int index) {
        const int count = GetPointCount();
        if (count == 0) return startPosition;
        if (index >= count) return startPosition; // ループの戻り区間の終点
        if (index < 0 || points[index] is null) return startPosition;
        return startPosition + points[index].offset;
    }
    float GetSegmentDuration(int index) {
        const int count = GetPointCount();
        if (count == 0) return 0.0f;
        if (index >= count) return loopBackDuration;
        if (index < 0 || points[index] is null) return 0.0f;
        return points[index].duration;
    }
    EaseType GetSegmentEaseType(int index) {
        const int count = GetPointCount();
        if (count == 0) return EaseType::Linear;
        if (index >= count) return loopBackEaseType;
        if (index < 0 || points[index] is null) return EaseType::Linear;
        return points[index].easeType;
    }
    // index番目区間の終点（points[index]。ループ時、index==count なら開始位置）に到達してから
    // 次の区間へ進むまでの待ち時間を取得する
    float GetSegmentWaitDuration(int index) {
        const int count = GetPointCount();
        if (count == 0) return 0.0f;
        if (index >= count) return loopBackWaitDuration;
        if (index < 0 || points[index] is null) return 0.0f;
        return points[index].waitDuration;
    }
    // index番目区間の始点・終点の回転を取得する（GetSegmentStartPoint/GetSegmentEndPointの回転版）。
    // ループの戻り区間（index==count）は必ず初期回転へ戻る（位置がstartPositionへ戻るのと同じ扱い）
    Quaternion GetSegmentStartRotation(int index) {
        if (index <= 0) return Math::MakeRotateEuler(initialRotation);
        return GetSegmentEndRotation(index - 1);
    }
    Quaternion GetSegmentEndRotation(int index) {
        const int count = GetPointCount();
        if (count == 0) return Math::MakeRotateEuler(initialRotation);
        if (index >= count) return Math::MakeRotateEuler(initialRotation); // ループの戻り区間の終点
        if (index < 0 || points[index] is null) return Math::MakeRotateEuler(initialRotation);
        return Math::MakeRotateEuler(initialRotation + ToRadians(points[index].rotateOffset));
    }

    void Update() {
        Transform@ tf = GetTransform();
        if (tf is null) return;
        const int count = GetPointCount();
        if (count == 0) return;

        // ループ時は「最後の点→開始位置」の戻り区間ぶん、区間数が count+1 になる
        const int segmentCount = loop ? count + 1 : count;
        if (currentSegment >= segmentCount) return; // 非ループで全区間完了済み

        float dt = GetDeltaTime() * GetGameSpeed();

        // 区間移動完了後の待機中は、位置を動かさずタイマーだけ進める
        if (isWaiting) {
            waitTimer -= dt;
            if (waitTimer <= 0.0f) {
                isWaiting = false;
                currentSegment++;
                segmentElapsed = 0.0f;
                if (loop && currentSegment >= segmentCount) currentSegment = 0;
            }
            return;
        }

        segmentElapsed += dt;
        const float duration = GetSegmentDuration(currentSegment);
        const float t = (duration > 0.0f) ? Clamp(segmentElapsed / duration, 0.0f, 1.0f) : 1.0f;

        const Vector3 segStart = GetSegmentStartPoint(currentSegment);
        const Vector3 segEnd = GetSegmentEndPoint(currentSegment);
        const Quaternion segStartRot = GetSegmentStartRotation(currentSegment);
        const Quaternion segEndRot = GetSegmentEndRotation(currentSegment);
        const EaseType easeType = GetSegmentEaseType(currentSegment);
        // 位置・回転は同じイージング済みt（easedT）を共有することで、同じタイミングで到達する
        // （回転はオイラー角の単純な線形補間だと軸がねじれたり角度をまたぐ際に大回りしたりするため、
        // Math::Slerpで補間する）
        const float easedT = Easing::Apply(t, easeType);
        tf.SetTranslate(Math::Lerp(segStart, segEnd, easedT));
        tf.SetRotateQuaternion(Math::Slerp(segStartRot, segEndRot, easedT));

        if (t >= 1.0f) {
            const float waitDuration = GetSegmentWaitDuration(currentSegment);
            if (waitDuration > 0.0f) {
                // 到達済みの点で待機してから次の区間へ進む
                isWaiting = true;
                waitTimer = waitDuration;
            } else {
                currentSegment++;
                segmentElapsed = 0.0f;
                if (loop && currentSegment >= segmentCount) currentSegment = 0;
            }
        }
    }
}
