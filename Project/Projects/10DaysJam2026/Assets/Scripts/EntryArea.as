class EntryArea : ScriptComponentBehavior {
    [SerializeField, Tooltip("入口のお邪魔ブロック")]
    Object@ leftGroup;

    void Start() {
    }

    void Update() {
    }

    void End() {
    }

    void OnCollisionEnter(const HitInfo &in hit){
        if(hit.otherCollider.GetTag() == "Player"){
            leftGroup.SetActive(true);
        }
    }
}
