// Application/Objects/Components/AlwaysRotate.h の移植版
// 毎フレーム一定の角速度でオブジェクトを回転させ続けるだけのスクリプト

class AlwaysRotate : ScriptComponentBehavior {
    // 各軸の角速度（ラジアン/秒）。元コードの既定値 (0, 0, 1) を踏襲
    [SerializeField]
    Vector3 angularVelocity = Vector3(0.0f, 0.0f, 1.0f);

    void Update() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        float dt = GetDeltaTime() * GetGameSpeed();
        if (dt < 0.0f) dt = 0.0f;

        tf.SetRotate(tf.GetRotate() + angularVelocity * dt);
    }
}
