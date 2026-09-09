class Rubble : ScriptComponentBehavior {
    [SerializeField, Tooltip("落下時の重力")]
    float gravity = 200.0f;

    [SerializeField, Tooltip("生成されてから落下を開始するまでの待機時間(秒)")]
    float dropDelay = 1.0f;

    [SerializeField, Tooltip("全体の最大生存時間(秒)")]
    float lifeTime = 5.0f;

    [SerializeField, Tooltip("地面に落下してから消滅するまでの時間(秒)")]
    float destroyDelay = 0.5f;

    [SerializeField, Tooltip("ダメージ量")]
    float damageAmount = 1;

    [SerializeField, Tooltip("着地音を鳴らす最低落下距離。クローン生成用に地面の高さで待機しているテンプレート自身が、初回のdropDelay経過時に「0距離の着地」として誤検知しないためのしきい値")]
    float landSoundMinFallDistance = 5.0f;

    float timer = 0.0f;
    float dropTimer = 0.0f;
    float groundTimer = 0.0f;
    bool isDropping = false;
    bool isGrounded = false;
    Vector2 velocity;

    // 落下開始位置のY座標(着地音を鳴らすべき実際の落下かどうかの判定に使う)
    float dropStartY = 0.0f;
    
    CharacterController2D@ controller;

    void Start() {
        GetComponent(@controller);
        velocity.y = 0.0f;
    }

    void PlayTaggedAudio(const string &in tagName) {
        // Start()時点のキャッシュだと、クローン直後同フレームで呼ばれた場合等に
        // まだ取得できていないことがあるため、呼び出す都度取得し直す
        array<AudioSource@>@ sources;
        if (!GetComponents(@sources)) return;
        for(uint i = 0; i < sources.length(); ++i) {
            if(sources[i] !is null && sources[i].GetTag() == tagName) {
                sources[i].SetActive(true);
                sources[i].Play();
            }
        }
    }

    void Update() {
        // 会話中(isDialogueActive)は落下などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        Transform@ tf = GetTransform();
        if (tf is null) return;

        // 最大生存時間を超えたら強制的に非アクティブ化して削除扱いにする
        timer += GetDeltaTime();
        if (timer >= lifeTime) {
            Object@ obj = GetOwnerObject();
            if (obj !is null) obj.SetActive(false);
            return;
        }

        if (!isDropping) {
            // 落下前の待機時間カウント
            dropTimer += GetDeltaTime();
            if (dropTimer >= dropDelay) {
                isDropping = true;
                dropStartY = tf.GetTranslate().y;
            }
        } else if (!isGrounded) {
            // 重力を適用して落下
            velocity.y -= gravity * GetDeltaTime();

            if (controller !is null) {
                // CharacterController2Dがある場合はコライダーベースで移動と着地判定を行う
                controller.Move(velocity * GetDeltaTime());
                if (controller.IsGrounded() && velocity.y <= 0.0f) {
                    isGrounded = true;
                    velocity.y = 0.0f;
                    // クローン生成用に地面の高さで待機しているテンプレート自身が、
                    // 実際には落下していないのに「着地」してしまうケースを除外する
                    if (dropStartY - tf.GetTranslate().y >= landSoundMinFallDistance) {
                        PlayTaggedAudio("Land");
                    }
                }
            } else {
                // Controllerがない場合の簡易フォールバック
                Vector3 pos = tf.GetTranslate();
                pos.y += velocity.y * GetDeltaTime();

                if (pos.y <= 0.0f) {
                    bool didFall = dropStartY - pos.y >= landSoundMinFallDistance;
                    pos.y = 0.0f;
                    isGrounded = true;
                    velocity.y = 0.0f;
                    if (didFall) {
                        PlayTaggedAudio("Land");
                    }
                }
                tf.SetTranslate(pos);
            }
        } else {
            // 地面に落ちたあとは少し待ってから消滅させる
            groundTimer += GetDeltaTime();
            if (groundTimer >= destroyDelay) {
                Object@ obj = GetOwnerObject();
                if (obj !is null) obj.SetActive(false);
            }
        }
    }

    // プレイヤーなどの動的オブジェクトとぶつかった際の処理
    void OnCollisionEnter(const HitInfo &in hit) {
        // プレイヤーに直撃した場合は即座に岩を消滅させる
        if (hit.otherCollider.GetTag() == "Player") {
            Object@ obj = GetOwnerObject();
            if (obj !is null) {
                obj.SetActive(false);
            }

            Object@ player = hit.otherObject;

            if (player !is null) {
                ScriptComponent@ sc;
                if (player.GetComponent(@sc)) {
                    sc.CallMethod("Damage", damageAmount);
                }
            }
        }
    }

    void SetDropDelay(float delay) {
        dropDelay = delay;
    }

    void End() {
    }
}