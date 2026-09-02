class WeaponEffect : ScriptComponentBehavior {
    [SerializeField, Tooltip("座標")]
    Vector3 pos;

    [SerializeField, Tooltip("回転")]
    Vector3 rotate;

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null)return;

        tf.SetTranslate(pos);
        tf.SetRotate(rotate);
    }

    void End() {

    }
}
