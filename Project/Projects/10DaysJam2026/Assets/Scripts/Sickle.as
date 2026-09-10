enum SickleState {
    Waiting,
    Flying
}

class Sickle : ScriptComponentBehavior {
    [SerializeField, Tooltip("狙う対象(プレイヤー)")]
    Object@ player;

    [SerializeField, Tooltip("発生後、飛び出すまでの待機時間(秒)")]
    float waitDuration = 0.4f;

    [SerializeField, Tooltip("飛翔速度")]
    float flySpeed = 140.0f;

    [SerializeField, Tooltip("生成されてから自動的に消えるまでの時間(秒)")]
    float lifeTime = 4.0f;

    [SerializeField, Tooltip("これに当たると消える相手のタグ")]
    string despawnTag = "Weapon";

    SickleState state = SickleState::Waiting;

    // 状態管理用タイマー
    float stateTimer = 0.0f;
    float lifeTimer = 0.0f;
    Vector2 velocity;

    ParticleSystem2D@ particle;

    float particleDeleteTimer = 0.0f;
    float particleDeleteDuratiom = 1.0f;

    float deleteTimer = 0.0f;
    float deleteDuration = 2.0f;

    void Start() {
        state = SickleState::Waiting;
        stateTimer = 0.0f;
        lifeTimer = 0.0f;
        velocity = Vector2(0.0f, 0.0f);
    }

    void Update() {
        // 会話中(isDialogueActive)は飛行などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        // 生存時間チェック
        lifeTimer += GetDeltaTime();
        if (lifeTimer >= lifeTime) {
            Despawn();
            return;
        }

        if (state == SickleState::Waiting) {
            stateTimer += GetDeltaTime();
            if (stateTimer >= waitDuration) {
                StartFlying();
            }
        } else if (state == SickleState::Flying) {
            Transform@ tf = GetTransform();
            if (tf !is null) {
                Vector3 pos = tf.GetTranslate();
                pos.x += velocity.x * GetDeltaTime();
                pos.y += velocity.y * GetDeltaTime();
                tf.SetTranslate(pos);
            }
        }

        if(particle !is null){
            if(particle.IsPlaying()){
                particleDeleteTimer += GetDeltaTime();
                if(particleDeleteTimer >= particleDeleteDuratiom){
                    particleDeleteTimer = 0.0f;
                    particle.Stop();
                }
            }
        }

        if(particle !is null){
            deleteTimer += GetDeltaTime();
            if(deleteTimer >= deleteDuration){
                deleteTimer = 0.0f;
                GetOwnerObject().SetActive(false);
            }
        }
    }

    // 待機終了。その時点のプレイヤー座標へ向けて直進を開始する
    void StartFlying() {
        state = SickleState::Flying;
        stateTimer = 0.0f;

        Transform@ tf = GetTransform();
        if (tf !is null && player !is null) {
            Transform@ playerTf = player.GetTransform();
            if (playerTf !is null) {
                Vector3 selfPos = tf.GetTranslate();
                Vector3 targetPos = playerTf.GetTranslate();
                float dx = targetPos.x - selfPos.x;
                float dy = targetPos.y - selfPos.y;
                float len = Sqrt(dx * dx + dy * dy);
                if (len > 0.0001f) {
                    velocity.x = (dx / len) * flySpeed;
                    velocity.y = (dy / len) * flySpeed;
                }

                // 左右反転
                tf.SetRotate(Vector3(0.0f, (dx < 0.0f) ? 3.14159f : 0.0f, 0.0f));
            }
        }
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherCollider.GetTag() == despawnTag) {
            Despawn();
        }
    }

    // 非Active化
    void Despawn() {
        // パーティクル生成
        GetComponent(@particle);
        if(particle !is null){
            particle.Play();
        }

        Box2DCollider@ col;
        SpriteRenderer@ sprite;
        if(GetComponent(@col) && GetComponent(@sprite)){
            col.SetActive(false);
            sprite.SetActive(false);
        }
    }

    void End() {
    }
}
