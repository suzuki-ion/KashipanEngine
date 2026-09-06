// 画面全体を覆うフェード演出用コンポーネント。
// 単色のSpriteRendererを、Camera2Dの表示範囲いっぱいに広がるよう毎フレーム自動でサイズ合わせしつつ、
// アルファ値をアニメーションさせて画面を覆ったり(FadeOut)覆いを解いたり(FadeIn)する。
//
// シーンを跨いでオブジェクトを保持する仕組みがこのエンジンには無いため、
// 「シーン遷移時のフェード」は次の2段構えで実現する:
//   1. 遷移元のシーンでは、遷移直前にSetAlphaInstant(1.0)で画面を瞬時に覆ってからシーンを切り替える
//      (TitleMenuController.GoToGameScene()を参照)
//   2. 遷移先のシーンにも同じ設定(fadeInOnStart=true)のScreenFadeオブジェクトを配置しておくことで、
//      シーン開始時にアルファ値1.0(不透明)から0.0(透明)へ自動でフェードインし、画面が明ける
//
// 前提（エディター側で設定が必要）:
//   - 同じオブジェクトに、単色表示できるSpriteRenderer(マテリアルはDefault等の白テクスチャ)を付けておくこと
//   - 画面全体を覆うよう、Transform.translateをカメラ中心に固定するScreenAnchor(anchorPoint=(0.5,0.5))も
//     合わせて付けておくこと(このスクリプトはTransform.scaleのみを毎フレーム画面サイズへ合わせる)
//   - cameraObjectに、サイズ基準にするCamera2Dを持つオブジェクトを指定しておくこと
class ScreenFade : ScriptComponentBehavior {
    [Header("見た目")]
    [SerializeField, ColorPicker, Tooltip("フェードに使う色(RGB)。アルファ値はフェードアニメーションで上書きされる")]
    Vector4 fadeColor = Vector4(0.0f, 0.0f, 0.0f, 1.0f);

    [Header("全画面サイズ合わせ")]
    [SerializeField, Tooltip("画面サイズの基準にするCamera2Dを持つオブジェクト(毎フレームこのカメラの幅/高さにTransform.scaleを合わせる)")]
    Object@ cameraObject;

    [Header("シーン開始時の自動フェードイン")]
    [SerializeField, Tooltip("シーン開始時、アルファ値1.0(不透明)から0.0(透明)へ自動でフェードインするか")]
    bool fadeInOnStart = true;

    [SerializeField, Tooltip("自動フェードインにかける秒数")]
    float fadeInDuration = 0.5f;

    // --- 参照コンポーネント(保存不要) ---
    SpriteRenderer@ spriteRenderer;
    Transform@ transformComponent;

    // --- フェード実行状態(保存不要) ---
    bool isFading = false;
    float fadeElapsed = 0.0f;
    float fadeDuration = 0.0f;
    float fadeFromAlpha = 0.0f;
    float fadeToAlpha = 0.0f;

    void Start() {
        GetComponent(@spriteRenderer);
        GetComponent(@transformComponent);

        if (spriteRenderer !is null) {
            spriteRenderer.SetInstanceColorBlendMode(0); // 0 = Override
        }

        if (fadeInOnStart) {
            SetAlphaInstant(1.0f);
            StartFade(1.0f, 0.0f, fadeInDuration);
        }
    }

    void Update() {
        ApplyScreenSize();

        if (!isFading) return;

        fadeElapsed += GetDeltaTime();
        float t = (fadeDuration > 0.0f) ? Clamp(fadeElapsed / fadeDuration, 0.0f, 1.0f) : 1.0f;
        ApplyAlpha(Lerp(fadeFromAlpha, fadeToAlpha, t));

        if (t >= 1.0f) {
            isFading = false;
        }
    }

    // アルファ値をfromAlphaからtoAlphaへ、duration秒かけてフェードさせる
    void StartFade(float fromAlpha, float toAlpha, float duration) {
        fadeFromAlpha = fromAlpha;
        fadeToAlpha = toAlpha;
        fadeDuration = duration;
        fadeElapsed = 0.0f;
        isFading = duration > 0.0f;
        ApplyAlpha(fromAlpha);
        if (duration <= 0.0f) {
            ApplyAlpha(toAlpha);
        }
    }

    // アルファ値1.0(不透明)から0.0(透明)へduration秒かけてフェードインする(覆いを解く)
    void FadeIn(float duration) {
        StartFade(1.0f, 0.0f, duration);
    }

    // アルファ値0.0(透明)から1.0(不透明)へduration秒かけてフェードアウトする(画面を覆い隠す)
    void FadeOut(float duration) {
        StartFade(0.0f, 1.0f, duration);
    }

    // アニメーションを行わず、アルファ値を即座に設定する(シーン遷移直前に画面を瞬時に覆う用途等)
    void SetAlphaInstant(float alpha) {
        isFading = false;
        ApplyAlpha(alpha);
    }

    // fadeColorのRGBを保ったまま、アルファ値だけを差し替えてSpriteRendererへ反映する
    void ApplyAlpha(float alpha) {
        if (spriteRenderer is null) return;
        Vector4 color = fadeColor;
        color.w = alpha;
        spriteRenderer.SetInstanceColor(color);
    }

    // cameraObjectの表示範囲(幅/高さ)に合わせて、画面全体を覆うようTransform.scaleを更新する
    void ApplyScreenSize() {
        if (cameraObject is null || transformComponent is null) return;

        Camera2D@ camera;
        if (!cameraObject.GetComponent(@camera)) return;

        transformComponent.SetScale(Vector3(camera.GetWidth(), camera.GetHeight(), 1.0f));
    }
}
