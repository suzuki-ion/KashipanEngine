enum MoveDirection{
    Right, // 右移動
    Left // 左移動
}

class Green : ScriptComponentBehavior {

    [SerializeField, Tooltip("移動速度")]
    float moveSpeed = 1.0f;

    [SerializeField, Tooltip("HP")]
    float hp = 1.0f;

    [SerializeField, Tooltip("撃破時にもらえる経験値")]
    float expValue = 5.0f;

    [SerializeField, Tooltip("重力")]
    float gravity = 12.0f;

    [SerializeField, Tooltip("無敵時間(秒)")]
    float invincibleDuration = 0.5f;

    [SerializeField, Tooltip("点滅の切り替え間隔(秒)")]
    float blinkInterval = 0.08f;

    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    [SerializeField, Tooltip("カメラの表示範囲からこの距離(px)以内であれば移動を行う(画面端での見切れを防ぐマージン)")]
    float activationRange = 16.0f;

    [SerializeField, Tooltip("死亡エフェクト")]
    Object@ deathEffect;

    [SerializeField, Tooltip("セルフオブジェ")]
    Object@ self;

    [SerializeField, Tooltip("接地とみなす法線Y成分のしきい値")]
    float groundedThreshold = 0.5f;

    [SerializeField, Tooltip("押し戻しを行わない相手のタグ一覧")]
    array<string>@ pushBackExcludeTags;

    // エフェクトのクローン
    Object@ cloneEffect;

    // 進行方向
    MoveDirection moveDir = MoveDirection::Left;

    bool isAlive = true;
    bool isAnimation = false;
    float deathEffectTimer = 0.0f;

    // 左右の足元チェッカーが接触しているTilemapの数
    int leftContactCount = 0;
    int rightContactCount = 0;

    // 無敵時間・点滅管理用
    bool isInvincible = false;
    float invincibleTimer = 0.0f;
    SpriteRenderer@ sprite;

    Vector2 velocity;

    Box2DCollider@ col;
    CharacterController2D@ controller;

    void PlayTaggedAudio(const string &in tagName) {
        // Start()時点のキャッシュだと、クローン直後同フレームで呼ばれた場合等に
        // まだ取得できていないことがあるため、呼び出す都度取得し直す
        array<AudioSource@>@ sources;
        if (!GetComponents(@sources)) return;
        for(uint i = 0; i < sources.length(); ++i) {
            if(sources[i] !is null && sources[i].GetTag() == tagName) {
                // 直前にSetComponentsActiveExceptTransformAndScript(false)等で
                // 巻き添え無効化されていた場合、Finalize()内のStop()で再生できないため
                // 念のため再度有効化してから再生する
                sources[i].SetActive(true);
                sources[i].Play();
            }
        }
    }

    void Start() {
        GetComponent(@col);
        if (GetComponent(@controller)) {
            if (col !is null) {
                controller.SetSelectedCollider(col);
            }
            controller.SetGroundedThreshold(groundedThreshold);
            controller.ClearIgnoredTags();
            if (pushBackExcludeTags !is null) {
                for (uint i = 0; i < pushBackExcludeTags.length(); ++i) {
                    controller.AddIgnoredTag(pushBackExcludeTags[i]);
                }
            }
        }
        GetComponent(@sprite);
    }

    void Update() {
        // 会話中(isDialogueActive)は移動などの処理を止める
        bool isDialogueActive = false;
        GetScene().GetVariable("isDialogueActive", isDialogueActive);
        if (isDialogueActive) return;

        Transform@ tf = GetTransform();
        if(tf is null) return;

        // 生存時の無敵時間タイマー更新および点滅処理
        if (isAlive && isInvincible) {
            invincibleTimer += GetDeltaTime();

            // blinkIntervalごとにスプライトの表示・非表示を交互に切替
            bool isVisible = (int(invincibleTimer / blinkInterval) % 2 == 0);
            if (sprite !is null) {
                sprite.SetActive(isVisible);
            }

            // 無敵時間終了
            if (invincibleTimer >= invincibleDuration) {
                invincibleTimer = 0.0f;
                isInvincible = false;

                // 終了時は必ず表示状態に戻す
                if (sprite !is null) {
                    sprite.SetActive(true);
                }
            }
        }

        // カメラの表示範囲を判定し、範囲外なら移動処理を行わない
        bool inCameraView = false;
        Vector3 camPos;
        Vector2 camSize;
        if (GetScene().GetVariable("gameplayCameraPos", camPos) && GetScene().GetVariable("gameplayCameraSize", camSize)) {
            Vector3 pos = tf.GetTranslate();
            inCameraView = (pos.x >= camPos.x - activationRange && pos.x <= camPos.x + camSize.x + activationRange &&
                             pos.y >= camPos.y - activationRange && pos.y <= camPos.y + camSize.y + activationRange);
        }

        if (inCameraView) {
            // moveDirに応じてY軸の回転を設定
            float rotY = (moveDir == MoveDirection::Left) ? 0.0f : 3.14159f;
            tf.SetRotate(Vector3(0.0f, rotY, 0.0f));

            // 左右移動
            float dir = (moveDir == MoveDirection::Left) ? -1.0f : 1.0f;
            velocity.x = dir * moveSpeed;

            // 重力
            velocity.y -= gravity * GetDeltaTime();

            // 移動・当たり判定処理(プレイヤーと同じCharacterController2Dによる押し戻し)
            if (controller !is null) {
                controller.Move(velocity * GetDeltaTime());

                // 着地判定
                if (controller.IsGrounded() && velocity.y <= 0.0f) {
                    velocity.y = 0.0f;
                }
            } else {
                // controllerがない場合の簡易フォールバック
                tf.SetTranslate(tf.GetTranslate() + Vector3(velocity.x, velocity.y, 0.0f) * GetDeltaTime());
            }
        }

        // HPが0になったら
        if(hp <= 0.0f){
            isAlive = false;
        }

        if(!isAlive){
            if(!isAnimation){
                // 経験値付与処理
                if (player !is null) {
                    ScriptComponent@ playerSc;
                    if (player.GetComponent(@playerSc)) {
                        playerSc.CallMethod("AddExp", expValue);
                    }
                }

                @cloneEffect = GetScene().CloneObject(deathEffect, "CloneDeathEffect");

                if(cloneEffect !is null){
                    Transform@ cloneTf = cloneEffect.GetTransform();
                    if(cloneTf !is null){
                        cloneTf.SetScale(Vector3(32.0f, 32.0f, 1.0f));
                    }

                    ScriptComponent@ sc;
                    if(cloneEffect.GetComponent(@sc)){
                        sc.CallMethod("StartAnimation");
                        sc.SetVariable("pos", tf.GetTranslate());
                    }
                }

                // 自身の描画と当たり判定を無効化
                self.SetComponentsActiveExceptTransformAndScript(false);

                // 上記の無効化でAudioSourceも巻き添えで無効化され、Finalize()内のStop()で
                // 再生開始直後の音が止まってしまうため、無効化した後に鳴らす
                // (PlayTaggedAudio側でも再生対象を再度有効化しているため二重の対策になっている)
                PlayTaggedAudio("Dead");

                isAnimation = true;
            }

            // アニメーション更新処理
            if(cloneEffect !is null){
                deathEffectTimer += GetDeltaTime();
                float deathEffectDuration = 0.6f;
                ScriptComponent@ sc;
                if(cloneEffect.GetComponent(@sc)){
                    sc.CallMethod("UpdateAnimation");

                    // アニメーションが終了したら非アクティブ化
                    if(deathEffectTimer >= deathEffectDuration){
                        cloneEffect.SetActive(false);
                    }
                }
            }
        }
    }

    // ダメージ受けて無敵状態を開始するメソッド
    void Damage(float amount) {
        if (!isAlive || isInvincible) return;

        hp -= amount;
        isInvincible = true;
        invincibleTimer = 0.0f;
        PlayTaggedAudio("Damage");
    }

    void End() {
        Log("GoombaEnd");
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        // 進行方向の切り替え (壁や段差への衝突)
        if(hit.selfCollider.GetTag() == "Direction" && (hit.otherObject.GetTag() == "Tilemap" || hit.otherCollider.GetTag() == "Wall")){
            if (moveDir == MoveDirection::Left) {
                moveDir = MoveDirection::Right;
            } else {
                moveDir = MoveDirection::Left;
            }
        }

        // 足元チェッカーがTilemapに触れたらカウントを増やす
        if (hit.selfCollider.GetTag() == "TilemapCheckerLeft" && hit.otherObject.GetTag() == "Tilemap") {
            leftContactCount++;
        }
        if (hit.selfCollider.GetTag() == "TilemapCheckerRight" && hit.otherObject.GetTag() == "Tilemap") {
            rightContactCount++;
        }
    }

    void OnCollisionExit(const HitInfo &in hit) {
        // 足元チェッカーがTilemapから離れたらカウントを減らす
        if(hit.selfCollider.GetTag() == "TilemapCheckerLeft" && hit.otherObject.GetTag() == "Tilemap"){
            leftContactCount--;
            // 接触しているTilemapがなくなったら反転
            if (leftContactCount <= 0) {
                leftContactCount = 0; // マイナス防止
                if (moveDir == MoveDirection::Left) {
                    moveDir = MoveDirection::Right;
                }
            }
        }

        if(hit.selfCollider.GetTag() == "TilemapCheckerRight" && hit.otherObject.GetTag() == "Tilemap"){
            rightContactCount--;
            // 接触しているTilemapがなくなったら反転
            if (rightContactCount <= 0) {
                rightContactCount = 0; // マイナス防止
                if (moveDir == MoveDirection::Right) {
                    moveDir = MoveDirection::Left;
                }
            }
        }
    }
}