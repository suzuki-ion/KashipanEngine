class BossDeadDetection : ScriptComponentBehavior {
    [SerializeField, Tooltip("出口のオブジェクト")]
    Object@ rightGroup;

    [SerializeField, Tooltip("ボス")]
    Object@ boss;

    void Start() {
    }

    void Update() {
        if(boss is null) return;

        Box2DCollider@ col;
        if(boss.GetComponent(@col)){
            float hp;
            if(!col.IsActive()){
                rightGroup.SetActive(false);
            }
        }
    }

    void End() {
    }
}
