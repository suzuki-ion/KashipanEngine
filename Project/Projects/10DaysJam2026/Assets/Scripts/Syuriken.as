class Syuriken : ScriptComponentBehavior {
    [SerializeField, Tooltip("コマ切り替え速度(秒)")]
    float frameInterval = 0.15f;

    [SerializeField, Tooltip("1コマあたりのUV移動量")]
    float uvStep = 0.5f;

    [SerializeField, Tooltip("座標")]
    Vector3 pos;

    [SerializeField, Tooltip("スピード")]
    float speed = 80.0f;

    [SerializeField, Tooltip("威力")]
    float damageAmount = 1.0f;

    // アニメーション用タイマー
    float animTimer = 0.0f;
    
    // 進む方向
    float movingX = 0.0f;

    SpriteRenderer@ sprite;
    Box2DCollider@ col;

    void Start() {
        GetComponent(@sprite);
        GetComponent(@col);
    }

    void Update() {
        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 移動処理
        pos.x += movingX * speed * GetDeltaTime();
        tf.SetTranslate(pos);

        // アニメーション処理
        animTimer += GetDeltaTime();
        int frame = int(animTimer / frameInterval) % 2;

        if (sprite !is null) {
            sprite.SetInstanceUvTranslate(Vector2(frame * uvStep, 0.0f));
        }
    }

    void End() {
    }

    void Attack(float moveX){
        //col.SetActive(true);
        movingX = moveX;
    }

    void OnCollisionEnter(const HitInfo &in hit){
        // 敵のダメージ処理
        if(hit.otherCollider.GetTag() == "Enemy"){
            Object@ enemy = hit.otherObject;

            if (enemy !is null) {
                ScriptComponent@ sc;
                if (enemy.GetComponent(@sc)) {
                    float hp;
                    if (sc.GetVariable("hp", hp)) {
                        Log("敵のHP: " + hp);
                        sc.SetVariable("hp", Clamp(hp - damageAmount, 0.0f, 100.0f));
                    }
                }
            }
        }else if(hit.otherCollider.GetTag() != "Player"){
            hit.selfCollider.SetActive(false);
            pos = Vector3(-100.0f, 0.0f, 0.0f);
            movingX = 0.0f;
        }
    }

    void SetPos(Vector3 playerPos){
        pos = playerPos;
    }
}
