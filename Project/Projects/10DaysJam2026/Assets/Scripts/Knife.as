class Knife : ScriptComponentBehavior {
    [SerializeField, Tooltip("X方向の初速度")]
    float initialSpeedX = 60.0f;

    [SerializeField, Tooltip("Y方向の初速度(打ち上げ力)")]
    float initialSpeedY = 150.0f;

    [SerializeField, Tooltip("重力")]
    float gravity = 300.0f;

    [SerializeField, Tooltip("威力")]
    float damageAmount = 1.0f;

    [SerializeField, Tooltip("座標")]
    Vector3 pos;

    // 現在の速度
    float currentSpeedX = 0.0f;
    float currentSpeedY = 0.0f;

    Box2DCollider@ col;

    void Start() {
        GetComponent(@col);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 重力によるY方向の速度減衰
        currentSpeedY -= gravity * GetDeltaTime();

        // 移動処理
        pos.x += currentSpeedX * GetDeltaTime();
        pos.y += currentSpeedY * GetDeltaTime();
        tf.SetTranslate(pos);
    }

    void End() {
    }

    void Attack(float moveX){
        currentSpeedX = moveX * initialSpeedX; // 向きに合わせてX速度を決定
        currentSpeedY = initialSpeedY;         // 上方向に打ち上げる
    }

    void OnCollisionEnter(const HitInfo &in hit){
        // プレイヤーへのダメージ処理
        if(hit.otherCollider.GetTag() == "Player"){
            Object@ player = hit.otherObject;

            if (player !is null) {
                ScriptComponent@ sc;
                if (player.GetComponent(@sc)) {
                    sc.CallMethod("Damage", damageAmount);
                }
            }

            hit.selfCollider.SetActive(false);
            pos = Vector3(-1000.0f, 0.0f, 0.0f);
            currentSpeedX = 0.0f;
            currentSpeedY = 0.0f;
        } 
        // Tilemapに当たったら消滅させる
        else if(hit.otherObject.GetTag() == "Tilemap"){
            hit.selfCollider.SetActive(false);
            pos = Vector3(-1000.0f, 0.0f, 0.0f);
            currentSpeedX = 0.0f;
            currentSpeedY = 0.0f;
        }
    }

    void SetPos(Vector3 enemyPos){
        pos = enemyPos;
    }
}