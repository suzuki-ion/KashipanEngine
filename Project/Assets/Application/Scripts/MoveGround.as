// 指定した範囲内を往復しながら移動・回転する地面。
// 実際に位置・回転を動かすのは Velocity / Rotation コンポーネントなので、
// このスクリプトを付けるオブジェクトには Velocity と Rotation を追加しておくこと。
// また、Player/Enemyはこのオブジェクトの PreTransform（前フレームの値）との差分を見て
// 地面の動きに追従するため、乗っているキャラクターを追従させたい場合は
// PreTransform コンポーネントも追加しておくこと

class MoveGround : ScriptComponentBehavior {
    [SerializeField, Tooltip("移動範囲 min（開始位置からのオフセット）")]
    Vector3 moveRangeMin = Vector3(0.0f, 0.0f, 0.0f);
    [SerializeField, Tooltip("移動範囲 max（開始位置からのオフセット）")]
    Vector3 moveRangeMax = Vector3(0.0f, 0.0f, 0.0f);
    [SerializeField, Tooltip("移動速度（単位/秒）")]
    Vector3 moveSpeed = Vector3(0.0f, 0.0f, 0.0f);
    [SerializeField, Tooltip("移動範囲の端に到達した際に停止し続ける時間（秒）")]
    float moveStopDuration = 0.0f;

    [SerializeField, Tooltip("回転範囲 min（開始角度からのオフセット、度）")]
    Vector3 rotateRangeMin = Vector3(0.0f, 0.0f, 0.0f);
    [SerializeField, Tooltip("回転範囲 max（開始角度からのオフセット、度）")]
    Vector3 rotateRangeMax = Vector3(0.0f, 0.0f, 0.0f);
    [SerializeField, Tooltip("回転速度（度/秒）")]
    Vector3 rotateSpeed = Vector3(0.0f, 0.0f, 0.0f);
    [SerializeField, Tooltip("回転範囲の端に到達した際に停止し続ける時間（秒）")]
    float rotateStopDuration = 0.0f;

    Vector3 initialPosition = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 initialRotation = Vector3(0.0f, 0.0f, 0.0f);
    // 各軸の移動/回転方向（+1 または -1）。範囲の端に着いたら反転する
    Vector3 moveDirection = Vector3(1.0f, 1.0f, 1.0f);
    Vector3 rotateDirection = Vector3(1.0f, 1.0f, 1.0f);
    // 範囲の端に到達してから停止し続けている残り時間（秒）。0以下なら停止していない
    float moveStopTimer = 0.0f;
    float rotateStopTimer = 0.0f;

    void Start() {
        Transform@ tf = GetTransform();
        if (tf is null) return;
        initialPosition = tf.GetTranslate();
        initialRotation = tf.GetRotate();
    }

    // 現在のオフセットが範囲の端に達していたら方向を反転する（往復移動用）。
    // min/max は大小関係を問わず正しく扱う（min > max という指定でも範囲として成立する）。
    // mn == mx の軸は「未設定」として扱い、常に方向は変えない（速度0なら動かない）
    //
    // dir はあくまで「速度に掛ける符号（+1/-1）」であり、実際に今どちらへ動いているか（速度の符号）は
    // speed の符号によって反転し得る（speedがマイナスなら、dirがプラスでも実際はマイナス方向に進む）。
    // そのためhi/lo境界の到達判定は「dirとspeedを合わせた実際の移動方向」で行い、反転が必要な場合は
    // 実際の移動方向を反転させる -dir を返す（speedの符号はそのまま維持されるので、speedがマイナスの
    // 設定であれば以後もマイナス方向を基準にした往復を続ける）
    float NextDirection(float offset, float mn, float mx, float dir, float speed) {
        if (mn == mx) return dir;
        float lo = mn < mx ? mn : mx;
        float hi = mn < mx ? mx : mn;
        float actualDir = (speed < 0.0f) ? -dir : dir;
        if (actualDir >= 0.0f && offset >= hi) return -dir;
        if (actualDir <= 0.0f && offset <= lo) return -dir;
        return dir;
    }

    // オフセットを範囲内（min/maxの大小関係を問わない）にクランプする。
    // 速度×dtの分だけ範囲を超えて進んでしまっても、ここで正確に境界へ戻す
    float ClampOffset(float offset, float mn, float mx) {
        if (mn == mx) return offset;
        float lo = mn < mx ? mn : mx;
        float hi = mn < mx ? mx : mn;
        if (offset < lo) return lo;
        if (offset > hi) return hi;
        return offset;
    }

    void Update() {
        Transform@ tf = GetTransform();
        if (tf is null) return;
        float dt = GetDeltaTime() * GetGameSpeed();

        // --- 移動 ---
        Velocity@ velocity;
        bool hasVelocity = GetComponent(@velocity);
        if (moveStopTimer > 0.0f) {
            moveStopTimer -= dt;
            if (hasVelocity) velocity.SetVelocity(Vector3(0.0f, 0.0f, 0.0f));
        } else {
            Vector3 moveOffset = tf.GetTranslate() - initialPosition;
            Vector3 newMoveDirection(
                NextDirection(moveOffset.x, moveRangeMin.x, moveRangeMax.x, moveDirection.x, moveSpeed.x),
                NextDirection(moveOffset.y, moveRangeMin.y, moveRangeMax.y, moveDirection.y, moveSpeed.y),
                NextDirection(moveOffset.z, moveRangeMin.z, moveRangeMax.z, moveDirection.z, moveSpeed.z));
            bool moveHit = newMoveDirection.x != moveDirection.x
                || newMoveDirection.y != moveDirection.y
                || newMoveDirection.z != moveDirection.z;
            moveDirection = newMoveDirection;

            if (moveHit) {
                // 境界ぴったりの位置へスナップしてから停止する
                moveOffset.x = ClampOffset(moveOffset.x, moveRangeMin.x, moveRangeMax.x);
                moveOffset.y = ClampOffset(moveOffset.y, moveRangeMin.y, moveRangeMax.y);
                moveOffset.z = ClampOffset(moveOffset.z, moveRangeMin.z, moveRangeMax.z);
                tf.SetTranslate(initialPosition + moveOffset);
                if (hasVelocity) velocity.SetVelocity(Vector3(0.0f, 0.0f, 0.0f));
                moveStopTimer = moveStopDuration;
            } else if (hasVelocity) {
                velocity.SetVelocity(Vector3(
                    moveSpeed.x * moveDirection.x,
                    moveSpeed.y * moveDirection.y,
                    moveSpeed.z * moveDirection.z));
            }
        }

        // --- 回転 ---
        Rotation@ rotation;
        bool hasRotation = GetComponent(@rotation);
        if (rotateStopTimer > 0.0f) {
            rotateStopTimer -= dt;
            if (hasRotation) rotation.SetAngularVelocity(Vector3(0.0f, 0.0f, 0.0f));
        } else {
            // rotateRangeMin/Max は度指定なので、ラジアンであるTransformの回転と
            // 比較できるようにここでラジアンへ変換する
            Vector3 rotateRangeMinRad = ToRadians(rotateRangeMin);
            Vector3 rotateRangeMaxRad = ToRadians(rotateRangeMax);

            Vector3 rotateOffset = tf.GetRotate() - initialRotation;
            Vector3 newRotateDirection(
                NextDirection(rotateOffset.x, rotateRangeMinRad.x, rotateRangeMaxRad.x, rotateDirection.x, rotateSpeed.x),
                NextDirection(rotateOffset.y, rotateRangeMinRad.y, rotateRangeMaxRad.y, rotateDirection.y, rotateSpeed.y),
                NextDirection(rotateOffset.z, rotateRangeMinRad.z, rotateRangeMaxRad.z, rotateDirection.z, rotateSpeed.z));
            bool rotateHit = newRotateDirection.x != rotateDirection.x
                || newRotateDirection.y != rotateDirection.y
                || newRotateDirection.z != rotateDirection.z;
            rotateDirection = newRotateDirection;

            if (rotateHit) {
                rotateOffset.x = ClampOffset(rotateOffset.x, rotateRangeMinRad.x, rotateRangeMaxRad.x);
                rotateOffset.y = ClampOffset(rotateOffset.y, rotateRangeMinRad.y, rotateRangeMaxRad.y);
                rotateOffset.z = ClampOffset(rotateOffset.z, rotateRangeMinRad.z, rotateRangeMaxRad.z);
                tf.SetRotate(initialRotation + rotateOffset);
                if (hasRotation) rotation.SetAngularVelocity(Vector3(0.0f, 0.0f, 0.0f));
                rotateStopTimer = rotateStopDuration;
            } else if (hasRotation) {
                rotation.SetAngularVelocity(Vector3(
                    ToRadians(rotateSpeed.x) * rotateDirection.x,
                    ToRadians(rotateSpeed.y) * rotateDirection.y,
                    ToRadians(rotateSpeed.z) * rotateDirection.z));
            }
        }
    }
}
