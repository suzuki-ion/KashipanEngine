// DitherEffect(ポストエフェクト)を使った画面フェードイン/フェードアウト用コンポーネント。
// フェードイン/アウトどちらもこのスクリプト1つで担当し、isFadedOutフラグの値が
// 切り替わったタイミングでDitherEffect.intensityを目標値までアニメーションさせる。
//   isFadedOut == false : intensity 0.0 (無適用、画面が見える状態)
//   isFadedOut == true  : intensity 1.0 (画面全体が消える状態)
//
// 前提（エディター側で設定が必要）:
//   - ditherObjectに、DitherEffectが付与されたオブジェクト(通常はScreenBufferObjectと同じオブジェクト)を指定しておくこと
//
// 他スクリプトからの呼び出し方:
//   Object@側でGetComponent(@ScriptComponent)して、CallMethod("FadeOut")のように呼ぶ。
//   (TitleMenuController.CoverScreenBeforeTransition()のDitherFade呼び出しと同じ要領)
//   例:
//     ScriptComponent@ fadeScript;
//     if (fadeObject.GetComponent(@fadeScript)) {
//         fadeScript.CallMethod("FadeOut");
//     }
//
// シーン遷移時のフェード（ScreenFade.asと同じ2段構え）:
//   1. 遷移元のシーンでは、遷移直前にSetFadedOutInstant(true)で画面を瞬時に覆ってからシーンを切り替える
//   2. 遷移先のシーンにも同じ設定(fadeInOnStart=true)のDitherFadeオブジェクトを配置しておくことで、
//      シーン開始時に覆われた状態から自動でフェードインし、画面が明ける
class DitherFade : ScriptComponentBehavior {
    [Header("対象")]
    [SerializeField, Tooltip("DitherEffectが付与されたオブジェクト(通常はScreenBufferObjectと同じオブジェクト)")]
    Object@ ditherObject;

    [Header("フェード状態")]
    [SerializeField, Tooltip("true: 画面を覆った状態(intensity=1) / false: 何も覆っていない状態(intensity=0)。この値が切り替わったタイミングでフェードアウト/フェードインが再生される")]
    bool isFadedOut = false;

    [Header("フェード時間")]
    [SerializeField, Tooltip("フェードイン(覆いを解く、intensity 1->0)にかける秒数")]
    float fadeInDuration = 0.5f;

    [SerializeField, Tooltip("フェードアウト(画面を覆う、intensity 0->1)にかける秒数")]
    float fadeOutDuration = 0.5f;

    [Header("シーン開始時の自動フェードイン")]
    [SerializeField, Tooltip("シーン開始時、覆われた状態(intensity=1)から自動でフェードインするか(ScreenFadeのfadeInOnStartと同じ用途)")]
    bool fadeInOnStart = false;

    // --- 参照コンポーネント(保存不要) ---
    DitherEffect@ ditherEffect;

    // --- フェード実行状態(保存不要) ---
    bool appliedFlag = false;
    bool isFading = false;
    float fadeElapsed = 0.0f;
    float fadeDuration = 0.0f;
    float fadeFromIntensity = 0.0f;
    float fadeToIntensity = 0.0f;

    void Start() {
        if (ditherObject !is null) {
            ditherObject.GetComponent(@ditherEffect);
        }

        if (fadeInOnStart) {
            // 遷移元シーンのSetFadedOutInstant(true)相当の状態から始まり、ここでフェードインする
            SetFadedOutInstant(true);
            FadeIn();
        } else {
            // 初期状態はアニメーションなしで即座に反映する
            appliedFlag = isFadedOut;
            ApplyIntensityInstant(isFadedOut ? 1.0f : 0.0f);
        }
    }

    void Update() {
        // インスペクター上での直接編集も含め、isFadedOutの値が変化したら対応するフェードを開始する
        if (isFadedOut != appliedFlag) {
            appliedFlag = isFadedOut;
            StartFade(appliedFlag);
        }

        if (!isFading) return;

        fadeElapsed += GetDeltaTime();
        float t = (fadeDuration > 0.0f) ? Clamp(fadeElapsed / fadeDuration, 0.0f, 1.0f) : 1.0f;
        ApplyIntensity(Lerp(fadeFromIntensity, fadeToIntensity, t));

        if (t >= 1.0f) {
            isFading = false;
        }
    }

    // フェードイン(覆いを解く)を再生する。isFadedOutをfalseに切り替える
    void FadeIn() {
        SetFadedOut(false);
    }

    // フェードアウト(画面を覆う)を再生する。isFadedOutをtrueに切り替える
    void FadeOut() {
        SetFadedOut(true);
    }

    // isFadedOutを直接指定して切り替える。現在の値と同じ場合は何もしない
    void SetFadedOut(bool fadedOut) {
        if (fadedOut == isFadedOut) return;
        isFadedOut = fadedOut;
    }

    // アニメーションを行わず、フェード状態を即座に反映する(シーン開始直後の初期化等に使用)
    void SetFadedOutInstant(bool fadedOut) {
        isFadedOut = fadedOut;
        appliedFlag = fadedOut;
        ApplyIntensityInstant(fadedOut ? 1.0f : 0.0f);
    }

    // 現在フェードアニメーション再生中かどうか
    bool IsFading() const {
        return isFading;
    }

    // targetFadedOutで指定した状態へ向けたフェードアニメーションを開始する
    void StartFade(bool targetFadedOut) {
        fadeFromIntensity = (ditherEffect !is null) ? ditherEffect.GetIntensity() : (targetFadedOut ? 0.0f : 1.0f);
        fadeToIntensity = targetFadedOut ? 1.0f : 0.0f;
        fadeDuration = targetFadedOut ? fadeOutDuration : fadeInDuration;
        fadeElapsed = 0.0f;
        isFading = fadeDuration > 0.0f;

        if (!isFading) {
            ApplyIntensity(fadeToIntensity);
        }
    }

    // DitherEffect.intensityへ値を反映する
    void ApplyIntensity(float intensity) {
        if (ditherEffect is null) return;
        ditherEffect.SetIntensity(intensity);
    }

    // アニメーションを中断し、intensityを即座に指定値へ反映する
    void ApplyIntensityInstant(float intensity) {
        isFading = false;
        ApplyIntensity(intensity);
    }
}
