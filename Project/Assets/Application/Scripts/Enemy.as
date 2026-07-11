// Application/Objects/Components/EnemyCollision.h / EnemyAliveStateController.h の移植版
// （EnemyMovement.h は中身が空のスタブだったため移植対象なし）
//
// 前提（エディター側で設定が必要）:
//   - このスクリプトを持つオブジェクトに SphereCollider（半径0.5、中心(0,0,0)推奨）を追加しておくこと
//   - 地面のオブジェクト名は "Ground"、プレイヤーのオブジェクト名は "Player" である前提（元コードと同じ判定方法）
//
// 元コードでは死亡判定後にParticleManagerでヒットエフェクトを生成していたが、
// 現エンジンにはParticleManagerが存在しないため省略している。
// また、元コードの死亡状態(IsAlive)はシーン側（GameScene.cpp）が毎フレーム監視して
// オブジェクトの削除・再スポーンを行っていたが、シーン側のロジックは今回の移植対象外のため、
// このスクリプトでは死亡時に自身を非アクティブ化するところまでを行う。

class Enemy : ScriptComponentBehavior {
    // プレイヤーとの衝突とみなす法線の閾値（この値未満＝上から踏まれた場合に死亡）
    [SerializeField]
    float playerCollisionThreshold = 0.5f;

    bool isGrounded = false;
    bool isCollidingWithPlayer = false;
    Vector3 hitNormal = Vector3(0.0f, 0.0f, 0.0f);
    bool isAlive = true;

    void Start() {
        Log("Enemy start: " + GetOwnerObject().GetName());
    }

    void Update() {
        if (!isAlive) return;

        // ConsumePlayerCollision() 相当（このフレームだけ有効なパルスとして消費する）
        bool playerContact = isCollidingWithPlayer;
        isCollidingWithPlayer = false;

        // プレイヤーと衝突しているかつ衝突の法線が上向きなら死亡状態にする
        if (playerContact && hitNormal.y < playerCollisionThreshold) {
            isAlive = false;
            Log(GetOwnerObject().GetName() + " defeated!");
            GetOwnerObject().SetActive(false);
        }
    }

    void OnCollisionEnter(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherObject.GetName() == "Ground") {
            // 法線が上向きなら地面に接触しているとみなす
            isGrounded = hit.normal.y > 0.5f;
        } else if (hit.otherObject.GetName() == "Player") {
            isCollidingWithPlayer = true;
            // プレイヤーにダメージを与えるなどの処理をここに追加
        }
        hitNormal = hit.normal;
    }

    void OnCollisionStay(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherObject.GetName() == "Ground") {
            // 衝突判定から押し戻しベクトルを計算してエネミーを押し戻す
            Transform@ tf = GetTransform();
            if (tf !is null) {
                Vector3 pushBack = hit.normal * hit.penetration;
                pushBack.z = 0.0f;
                tf.SetTranslate(tf.GetTranslate() + pushBack);
            }
        } else if (hit.otherObject.GetName() == "Player") {
            // プレイヤーにダメージを与えるなどの処理をここに追加
        }
        hitNormal = hit.normal;
    }

    void OnCollisionExit(const HitInfo &in hit) {
        if (hit.otherObject is null) return;
        if (hit.otherObject.GetName() == "Ground") {
            isGrounded = false;
        } else if (hit.otherObject.GetName() == "Player") {
            isCollidingWithPlayer = false;
            // プレイヤーとの接触が終了したときの処理をここに追加
        }
        hitNormal = hit.normal;
    }

    void End() {
        Log("Enemy end");
    }
}
