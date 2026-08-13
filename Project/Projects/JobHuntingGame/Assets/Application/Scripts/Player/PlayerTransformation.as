// プレイヤーの変身形態（Sphere/Box/Cone）
enum PlayerForm {
    Sphere,
    Box,
    Cone
}

// プレイヤーの変身（Sphere/Box/Cone切り替え）を担当する
// 見た目（MeshFilter）・当たり判定（コライダーの有効/無効・タグ）の切り替えと、
// 各形態でどんな挙動が可能かの問い合わせ（Can*系メソッド）を一手に引き受ける。
// PlayerMovement/PlayerCombat/PlayerEmbeddingはこのクラスのCan*系メソッドを見て
// 挙動を変えるだけで、形態そのものの分岐（if (form == ...)）を持たないようにする
class PlayerTransformation {
    Player@ owner;

    PlayerForm form = PlayerForm::Sphere;

    // Box形態時、コライダーとタグをこの値へ切り替える。GroundBoxタグにすることで
    // Enemy.as側の地面/壁判定ロジックをそのまま流用し、「側面はUターン、上に乗ったら
    // ただの地面として扱う」を敵側の改修無しに実現している
    // （SetTagは文字列引数のみを受け付けるため、Tag型ではなくstringで保持する）
    string boxFormColliderTagName = "GroundBox";
    // Sphere/Cone形態時のコライダータグ（変更しない。Enemy.as側の踏まれ判定に使われる）
    string defaultColliderTagName = "PlayerSphere";

    PlayerTransformation(Player@ inOwner) {
        @owner = inOwner;
    }

    void Awake() {
        // プレハブ/シーン側のコライダー初期状態（Sphere有効・Box無効）と確実に一致させるため、
        // 起動時にも一度反映しておく
        ApplyForm();
    }

    // 入力トリガーに応じてSphere→Box→Cone→Sphere...の順で切り替える
    void CycleForm() {
        if (form == PlayerForm::Sphere) {
            form = PlayerForm::Box;
        } else if (form == PlayerForm::Box) {
            form = PlayerForm::Cone;
        } else {
            form = PlayerForm::Sphere;
        }
        ApplyForm();
    }

    void ApplyForm() {
        MeshFilter@ meshFilter;
        if (GetComponent(@meshFilter)) {
            meshFilter.SetMeshHandle(GetModelHandleFromAssetPath(GetMeshAssetPath()));
        }

        SphereCollider@ sphereCollider;
        if (GetComponent(@sphereCollider)) {
            // Cone形態も専用コライダーが無いためSphereColliderをそのまま流用する
            sphereCollider.SetActive(form != PlayerForm::Box);
            sphereCollider.SetTag(defaultColliderTagName);
        }

        BoxCollider@ boxCollider;
        if (GetComponent(@boxCollider)) {
            boxCollider.SetActive(form == PlayerForm::Box);
            boxCollider.SetTag(boxFormColliderTagName);
        }
    }

    string GetMeshAssetPath() const {
        if (form == PlayerForm::Box) return "PrimitiveMesh-Box";
        if (form == PlayerForm::Cone) return "PrimitiveMesh-Cone";
        return "PrimitiveMesh-UVSphere";
    }

    // 斜面を滑れるか（Sphereのみ）
    bool CanSlide() const {
        return form == PlayerForm::Sphere;
    }

    // 接地中に左右移動できるか（Cone以外。Coneは空中移動のみ許可される）
    bool CanWalkOnGround() const {
        return form != PlayerForm::Cone;
    }

    // 敵との接触でダメージを受けるか（Sphereのみ。Box/Coneは耐性形態）
    bool TakesContactDamage() const {
        return form == PlayerForm::Sphere;
    }

    // 敵を上から踏んだ際、PlayerCombat側の判定で敵を撃破できるか（Boxのみ）。
    // Sphereは既にEnemy.as自身の踏まれ判定（PlayerSphereタグ）で撃破できているため不要。
    // Coneは接触した瞬間に刺さって撃破するため、PlayerEmbedding側で別途処理する
    bool CanStompDefeatEnemy() const {
        return form == PlayerForm::Box;
    }

    // 何かに接触した瞬間その場に刺さって静止するか（Coneのみ）
    bool CanEmbedOnImpact() const {
        return form == PlayerForm::Cone;
    }
}
