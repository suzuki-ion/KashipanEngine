class EnemyBullet : ScriptComponentBehavior {
    [SerializeField, Tooltip("弾の速度")]
    float speed = 10.0f;

    [SerializeField, Tooltip("威力")]
    float damageAmount = 1.0f;

    [SerializeField, Tooltip("座標")]
    Vector3 pos;

    // 進む方向
    float movingX = 0.0f;

    Box2DCollider@ col;

    void Start() {
        GetComponent(@col);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 移動処理
        pos.x += movingX * speed * GetDeltaTime();
        tf.SetTranslate(pos);
    }

    void End() {
    }

    void Attack(float moveX){
        movingX = moveX;
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
            movingX = 0.0f;
        } else if(hit.otherCollider.GetTag() != "Enemy"){
            // 敵・プレイヤー以外に当たったら消滅
            hit.selfCollider.SetActive(false);
            pos = Vector3(-1000.0f, 0.0f, 0.0f);
            movingX = 0.0f;
        }
    }

    void SetPos(Vector3 enemyPos){
        pos = enemyPos;
    }
}
