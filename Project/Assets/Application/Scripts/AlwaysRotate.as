class AlwaysRotate : ScriptComponentBehavior {
    [SerializeField]
    Vector3 rotateSpeed = Vector3(0.0f, 0.0f, 0.0f);

    void Start() {
        Log("AlwaysRotate start: " + GetOwnerObject().GetName());
    }

    void Update() {
        Transform@ tf = GetTransform();
        if (tf is null) return;

        // 回転
        Vector3 rotate = tf.GetRotate();
        tf.SetRotate(rotate + rotateSpeed * GetDeltaTime());
    }

    void End() {
        Log("AlwaysRotate end");
    }
}