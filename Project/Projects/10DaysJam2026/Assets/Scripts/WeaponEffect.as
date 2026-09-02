class WeaponEffect : ScriptComponentBehavior {
    [SerializeField, Tooltip("アクティブ時間")]
    float activeDuration = 0.1f;

    [SerializeField, Tooltip("座標")]
    Vector3 pos;

    float activeTimer = 0.0f;
    bool isActive = false;

    void Update() {
        if(isActive){
            activeTimer += GetDeltaTime();
            if(activeTimer>=activeDuration){
                activeTimer = 0.0f;
                isActive = false;
            }
        }
    }

    void CreateEffect(Vector3 pos){
        Transform@ tf = GetTransform();
        if(tf is null)return;

        tf.SetTranslate(pos);
        isActive = true;
    }

    void End() {

    }
}
