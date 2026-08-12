// 被ダメージ時（無敵時間中）の色点滅演出を担当する
class PlayerDamageFlash {
    Player@ owner;

    float flashTimer = 0.0f;

    PlayerDamageFlash(Player@ inOwner) {
        @owner = inOwner;
    }

    // duration の間、点滅を開始する（被ダメージ時に無敵時間と同じ長さで呼ばれる）
    void StartFlash(float duration) {
        flashTimer = duration;
    }

    // 点滅を即座に止め、見た目を元の色へ戻す（リスポーン時などに使用）
    void Stop() {
        flashTimer = 0.0f;
        Reset();
    }

    void Update(float dt) {
        if (flashTimer <= 0.0f) return;
        flashTimer -= dt;

        if (flashTimer <= 0.0f) {
            flashTimer = 0.0f;
            Reset();
            return;
        }

        MeshRenderer@ meshRenderer;
        if (!GetComponent(@meshRenderer)) return;
        bool blinkOn = (int(flashTimer / owner.damageFlashInterval) % 2) == 0;
        if (blinkOn) {
            meshRenderer.SetInstanceColor(owner.damageFlashColor);
            meshRenderer.SetInstanceColorBlendMode(0); // Override
        } else {
            meshRenderer.SetInstanceColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            meshRenderer.SetInstanceColorBlendMode(1); // Multiply（見た目に影響しない）
        }
    }

    void Reset() {
        MeshRenderer@ meshRenderer;
        if (!GetComponent(@meshRenderer)) return;
        meshRenderer.SetInstanceColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        meshRenderer.SetInstanceColorBlendMode(1); // Multiply（見た目に影響しない）
    }
}
