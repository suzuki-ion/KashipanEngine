class EntryArea : ScriptComponentBehavior {
    [SerializeField, Tooltip("入口のお邪魔ブロック")]
    Object@ leftGroup;

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
                leftGroup.SetActive(false);
            }
        }
    }

    void End() {
    }

    void OnCollisionEnter(const HitInfo &in hit){
        if(hit.otherCollider.GetTag() == "Player"){
            leftGroup.SetActive(true);
        }
    }
}
