// Pure regression for the 「〇〇の〇〇」 property grammar. No engine, network, model or GPU.
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <string>
#include "Utilities/ShortTextPropertyRequest.h"

using namespace KashipanEngine::ShortText;
namespace {
int failures = 0;
void Require(bool value, const std::string &message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message.c_str()); ++failures; }
}
bool Near(float a, float b) { return std::abs(a - b) < 1e-4f; }
PropertyParseResult Interpret(const std::string &text) { return ParsePropertyRequest(text, BuiltinPropertySchema()); }
void Accept(const std::string &text, const char *component, const char *member, int axis, PropertyMode mode, float from, float to) {
    const auto r = Interpret(text);
    Require(r.recognized && r.success, "accept: " + text + " / " + r.message);
    if (!r.success) return;
    Require(r.request.component == component && r.request.member == member, "target: " + text);
    Require(r.request.axis == axis, "axis: " + text);
    Require(r.request.mode == mode, "mode: " + text);
    Require(Near(r.request.from, from) && Near(r.request.to, to), "values: " + text);
}
void Reject(const std::string &text) {
    const auto r = Interpret(text);
    Require(r.recognized && !r.success && !r.message.empty(), "reject: " + text + " / " + r.message);
}
}

int main() {
    const auto PP = PropertyMode::PingPong;
    const auto Set = PropertyMode::Set;
    // Required sentences, verbatim.
    Accept("Transformの移動X軸を-1.0fから1.0fの間で往復移動", "Transform", "translate_", 0, PP, -1, 1);
    Accept("Transformの回転Y軸を-90.0fから90.0fの間で往復", "Transform", "rotate_", 1, PP, -90, 90);
    Accept("BoxColliderのサイズX軸を1.0fから3.0fの間で往復", "BoxCollider", "size_", 0, PP, 1, 3);
    Accept("BoxColliderのサイズX軸を2.0fにする", "BoxCollider", "size_", 0, Set, 2, 2);
    {
        const auto r = Interpret("Lightの色を(0.5f, 0.5f, 1.0f, 1.0f)にして");
        Require(r.success && r.request.component == "Light" && r.request.member == "color_" && r.request.axis == -1 &&
            r.request.values == std::vector<float>{0.5f,0.5f,1.0f,1.0f}, "RGBA tuple and natural property name");
        Require(Interpret("光源のカラーを（０．５Ｆ，０．５Ｆ，１．０Ｆ，１．０Ｆ）に設定").success, "component aliases and full-width tuple");
        Require(Interpret("トランスフォームの位置を(1,2,3)にする").success, "whole Vector3");
        Require(Interpret("Box2DColliderのサイズを(1,2)にする").success, "whole Vector2");
        Reject("Lightの色を(1,2,3)にする");
        Reject("Lightの色X軸を(1,2,3,4)にする");
        Reject("Lightの明るさを(1,2,3,4)にする");
        Reject("Lightの色を(1,2,3,4,5)にする");
        Reject("Lightの色を(1,2,NaN,4)にする");
        Reject("Lightの色を(1,2,3,10001)にする");
        Reject("Transformのスケールを(1,0,1)にする");
        Reject("TransformのworldMatrix_を(1,2,3,4)にする");
    }
    // Without f, with spaces, with まで, full-width and other numbers (unlearned).
    Accept("Transformの移動X軸を-1から1の間で往復移動", "Transform", "translate_", 0, PP, -1, 1);
    Accept("Transform の 移動 X 軸 を -1.0f から 1.0f の間 で 往復移動", "Transform", "translate_", 0, PP, -1, 1);
    Accept("Transformの移動X軸を-1から1まで往復移動", "Transform", "translate_", 0, PP, -1, 1);
    Accept("Transformの移動X軸を-1.0Fから1.0Fまで往復移動", "Transform", "translate_", 0, PP, -1, 1);
    Accept("Transformの移動Ｘ軸を－２．５から３まで往復移動", "Transform", "translate_", 0, PP, -2.5f, 3);
    Accept("Transformの回転Y軸を-45から30.5の間で往復", "Transform", "rotate_", 1, PP, -45, 30.5f);
    Accept("BoxColliderのサイズX軸を0.5から7の間で往復", "BoxCollider", "size_", 0, PP, 0.5f, 7);
    Accept("BoxColliderのサイズX軸を2にする", "BoxCollider", "size_", 0, Set, 2, 2);
    Accept("BoxColliderのサイズ X軸 を 2.0f に設定", "BoxCollider", "size_", 0, Set, 2, 2);
    Accept("transformのposition z軸を+1から2までの間で往復", "Transform", "translate_", 2, PP, 1, 2);
    // Aliases and formal registered names.
    Accept("Transformの位置Y軸を0から2の間で往復", "Transform", "translate_", 1, PP, 0, 2);
    Accept("Transformの大きさX軸を1から2の間で往復", "Transform", "scale_", 0, PP, 1, 2);
    Accept("Transformの拡縮Z軸を1から2の間で往復", "Transform", "scale_", 2, PP, 1, 2);
    Accept("Transformのスケールy軸を3にする", "Transform", "scale_", 1, Set, 3, 3);
    Accept("Transformのtranslate_X軸を1にする", "Transform", "translate_", 0, Set, 1, 1);
    Accept("Transformのtranslate.yを1にする", "Transform", "translate_", 1, Set, 1, 1);
    Accept("Transformのrotate_ Z軸を90にする", "Transform", "rotate_", 2, Set, 90, 90);
    Accept("BoxColliderの中心Y軸を-1から1の間で往復", "BoxCollider", "center_", 1, PP, -1, 1);
    Accept("BoxColliderのcenter_ X軸を-3にする", "BoxCollider", "center_", 0, Set, -3, -3);
    Accept("Box2DColliderのサイズY軸を1から2の間で往復", "Box2DCollider", "size_", 1, PP, 1, 2);
    Accept("Box2DColliderのsize_ X軸を4にする", "Box2DCollider", "size_", 0, Set, 4, 4);
    Accept("Transformの X軸の移動を 1 にする","Transform", "translate_", 0, Set, 1, 1);
    // Period, wave and units.
    {
        auto r = Interpret("Transformの移動X軸を-1.0fから1.0fの間で往復移動");
        Require(r.success && Near(r.request.period, PropertyDefaultPeriod) && r.request.wave == PropertyWave::Linear && r.request.unit == PropertyUnit::Raw, "defaults: period 2s linear raw");
        r = Interpret("Transformの移動X軸を-1から1の間で周期3秒で往復移動");
        Require(r.success && Near(r.request.period, 3), "explicit period 周期N秒");
        r = Interpret("Transformの移動X軸を周期0.5秒で-1から1の間でなめらかに往復");
        Require(r.success && Near(r.request.period, 0.5f) && r.request.wave == PropertyWave::Smooth, "period before range and smooth wave");
        r = Interpret("Transformの移動X軸を-1から1の間で4秒周期で往復");
        Require(r.success && Near(r.request.period, 4), "N秒周期");
        r = Interpret("Transformの回転Y軸を-90.0fから90.0fの間で往復");
        Require(r.success && r.request.unit == PropertyUnit::Degrees, "rotation defaults to degrees");
        r = Interpret("Transformの回転Y軸を-90度から90度の間で往復");
        Require(r.success && r.request.unit == PropertyUnit::Degrees, "explicit degree suffix");
        r = Interpret("Transformの回転Y軸を-1.5ラジアンから1.5ラジアンの間で往復");
        Require(r.success && r.request.unit == PropertyUnit::Raw && Near(r.request.to, 1.5f), "explicit radians stay raw");
        Require(r.success && r.message.find("ラジアン") != std::string::npos, "radian description names radians");
        r = Interpret("BoxColliderのサイズX軸を1.0fから3.0fの間で往復");
        Require(r.success && r.request.positiveOnly, "collider size is positive only");
    }
    // Evaluation: absolute values, starts at from, reaches to at half period, never leaves range.
    Require(Near(EvaluatePropertyPingPong(-1, 1, 2, 0, PropertyWave::Linear), -1), "t=0 is from");
    Require(Near(EvaluatePropertyPingPong(-1, 1, 2, 0.5, PropertyWave::Linear), 0), "quarter period is middle");
    Require(Near(EvaluatePropertyPingPong(-1, 1, 2, 1, PropertyWave::Linear), 1), "half period is to");
    Require(Near(EvaluatePropertyPingPong(-1, 1, 2, 2, PropertyWave::Linear), -1), "full period returns");
    Require(Near(EvaluatePropertyPingPong(-1, 1, 2, 1, PropertyWave::Smooth), 1), "smooth half period");
    for (int i = 0; i < 1000; ++i) {
        const double t = i * 0.0137;
        for (auto wave : {PropertyWave::Linear, PropertyWave::Smooth}) {
            const float v = EvaluatePropertyPingPong(-1, 1, 2, t, wave);
            Require(v >= -1.0f - 1e-5f && v <= 1.0f + 1e-5f, "value stays in range");
        }
    }
    Require(Near(EvaluatePropertyPingPong(-1, 1, 2, std::nan(""), PropertyWave::Linear), -1), "non-finite time is safe");
    // Invalid requests are recognized and rejected without a partial plan.
    Reject("Transformの移動X軸を1から-1の間で往復移動");            // reversed range
    Reject("Transformの移動X軸を1から1の間で往復移動");             // empty range
    Reject("Transformの移動X軸を-20000から1の間で往復移動");        // too large
    Reject("Transformの移動X軸を-1から99999999999999999999999999999999999999999の間で往復"); // overflow
    Reject("Transformの移動を-1から1の間で往復移動");                // missing axis on Vector3
    Reject("BoxColliderのサイズを2にする");                          // missing axis
    Reject("Transformの移動X軸Y軸を1にする");                        // two axes
    Reject("Box2DColliderのサイズZ軸を2にする");                     // Z on Vector2
    Reject("BoxColliderのサイズX軸を0にする");                       // zero size
    Reject("BoxColliderのサイズX軸を-1から3の間で往復");             // negative size
    Reject("Transformのスケールx軸を0から1の間で往復");             // zero scale
    Reject("Transformの worldMatrix_ X軸を1にする");                 // read only
    Reject("Transformの速さX軸を1にする");                          // unknown property
    Reject("Transformのfoo_ X軸を1にする");                          // unknown formal name
    Reject("FooColliderのサイズX軸を1にする");                       // unknown component
    Reject("Transformの移動X軸を-1から1の間で動かして");             // range needs 往復
    Reject("Transformの移動X軸を周期3秒で2にする");                  // period only for ranges
    Reject("Transformの移動X軸を-1度から1度の間で往復");             // degrees on non angle
    Reject("Transformの移動X軸を-1から1の間で周期0.01秒で往復");     // period too short
    Reject("Transformの移動X軸を-1から1の間で周期3秒で周期4秒で往復"); // duplicate period
    Reject("Transformの移動X軸を-1から1の間でなめらかに等速で往復");   // conflicting wave
    Reject("Transformの移動");                                       // no request
    // One-shot change (Tween): 「N秒かけて」 + AからBまで + easing → a real KeyframeAnimation in the engine.
    const auto TW = PropertyMode::Tween;
    auto tween = [&](const std::string &text, const char *component, const char *member, int axis, float from, float to,
        float duration, const char *ease) {
        Accept(text, component, member, axis, TW, from, to);
        const auto r = Interpret(text);
        Require(r.success && Near(r.request.duration, duration) && r.request.ease == ease, "tween duration/ease: " + text + " / " + r.message);
        Require(!Parse(text).success, "legacy parser does not claim: " + text);
    };
    tween("1秒かけてTransformの移動Y軸を0.0fから4.0fまでEaseInOutCubicで移動", "Transform", "translate_", 1, 0, 4, 1, "EaseInOutCubic");
    tween("Transformの移動Y軸を0から4まで1秒かけてEaseInOutCubicで移動", "Transform", "translate_", 1, 0, 4, 1, "EaseInOutCubic");
    tween("2秒かけてTransformの回転Z軸を0.0fから90.0fまでEaseOutQuadで回転", "Transform", "rotate_", 2, 0, 90, 2, "EaseOutQuad");
    tween("1秒かけてTransformのスケールX軸を1.0fから2.0fまでLinearで変更", "Transform", "scale_", 0, 1, 2, 1, "Linear");
    tween("1秒かけてBoxColliderのサイズX軸を1.0fから3.0fまでEaseInOutCubicで変更", "BoxCollider", "size_", 0, 1, 3, 1, "EaseInOutCubic");
    tween("1 秒かけて Transform の 移動 Y 軸 を 0 から 4 まで easeinoutcubic で 移動", "Transform", "translate_", 1, 0, 4, 1, "EaseInOutCubic");
    tween("１．５秒かけてTransformの移動Ｙ軸を０．０Ｆから４．０Ｆまで EASEINOUTCUBICで移動", "Transform", "translate_", 1, 0, 4, 1.5f, "EaseInOutCubic");
    tween("0.5f秒かけてTransformの移動X軸を+2から-3.5までEaseInQuadで移動させる", "Transform", "translate_", 0, 2, -3.5f, 0.5f, "EaseInQuad");
    tween("1秒かけてTransformの移動Y軸を4から0までEaseInCubicで移動", "Transform", "translate_", 1, 4, 0, 1, "EaseInCubic"); // decreasing
    tween("1秒かけてTransformの移動Y軸を2から2までEaseOutCubicで移動", "Transform", "translate_", 1, 2, 2, 1, "EaseOutCubic"); // hold
    tween("3秒かけてTransformのtranslate_ Z軸を-1から1までEaseInOutQuadで移動", "Transform", "translate_", 2, -1, 1, 3, "EaseInOutQuad");
    tween("1秒かけてTransformの移動X軸を0から1までEaseOutBackで移動", "Transform", "translate_", 0, 0, 1, 1, "EaseOutBack"); // overshoot ok for position
    tween("1秒かけてBox2DColliderのサイズY軸を1から2までEaseOutBounceで変更", "Box2DCollider", "size_", 1, 1, 2, 1, "EaseOutBounce");
    {
        auto r = Interpret("1秒かけてTransformの移動Y軸を0から4まで移動");
        Require(r.success && r.request.ease == "Linear" && r.request.easeDefaulted && r.message.find("既定") != std::string::npos, "omitted easing previews the Linear default");
        r = Interpret("2秒かけてTransformの回転Z軸を0.0fから90.0fまでEaseOutQuadで回転");
        Require(r.success && r.request.unit == PropertyUnit::Degrees, "tween rotation in degrees");
        r = Interpret("1秒かけてTransformの移動Y軸を0.0fから4.0fまでEaseInOutCubicで移動");
        Require(r.success && r.message.find("EaseInOutCubic") != std::string::npos && r.message.find("1 秒") != std::string::npos, "tween description");
        Require(PropertyEaseNames().size() == 41 && ResolvePropertyEase("easeoutinbounce") == "EaseOutInBounce" && ResolvePropertyEase("smoothstep").empty(), "ease table");
        // The ping-pong grammar still defaults to its period and ignores tween fields.
        r = Interpret("Transformの移動X軸を-1から1の間で往復移動");
        Require(r.success && r.request.mode == PP && r.request.ease.empty() && Near(r.request.period, 2), "ping-pong unchanged");
    }
    Reject("Transformの移動Y軸を0から4までEaseInOutCubicで移動");                 // duration required
    Reject("0秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動");        // zero duration
    Reject("-1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動");       // negative duration
    Reject("5000秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動");     // too long
    Reject("1秒かけてTransformの移動Y軸を0から4まで2秒かけてEaseInOutCubicで移動"); // duplicate duration
    Reject("1秒かけてTransformの移動Y軸を0から4までSmoothStepで移動");            // unknown easing
    Reject("1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicでLinearで移動"); // two easings
    Reject("1秒かけてTransformの移動Y軸を-1から1の間で往復");                     // duration on ping-pong
    Reject("Transformの移動Y軸を-1から1の間でEaseInOutCubicで往復");              // easing on ping-pong
    Reject("1秒かけてTransformの移動Y軸を4にする");                               // duration on set
    Reject("1秒かけてTransformの移動Y軸を0から4まで周期2秒でEaseInOutCubicで移動"); // period on tween
    Reject("1秒かけてTransformの移動Y軸を0から4までなめらかに移動");              // wave on tween
    Reject("1秒かけてBoxColliderのサイズX軸を1から3までEaseOutBackで変更");       // overshoot on positive-only
    Reject("1秒かけてTransformのスケールX軸を1から0までLinearで変更");            // zero scale
    Reject("1秒かけてBox2DColliderのサイズZ軸を1から2までLinearで変更");          // Z on Vector2
    Reject("1秒かけてTransformのworldMatrix_ X軸を0から1までLinearで変更");       // read only
    Reject("1秒かけてTransformの移動Y軸を0から20000までLinearで移動");            // too large
    Reject("1秒かけてTransformの移動を0から4までLinearで移動");                    // missing axis
    Reject("1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動してジャンプ"); // partial interpretation
    Reject("1秒かけてTransformの移動Y軸を0度から4度までLinearで移動");            // degrees on non angle
    // Text outside the grammar is not claimed, so existing parsers and dictionaries still run.
    Require(!Interpret("上下にふわふわ動かして").recognized, "legacy sentence is not claimed");
    Require(!Interpret("Playerの3上に毎フレームついていく").recognized, "dictionary phrase with ident+の is not claimed");
    Require(!Interpret("scoreの変化時だけHUDのScore: 表示を更新").recognized, "dictionary phrase is not claimed");
    Require(!Interpret("").recognized, "empty input");
    Require(!Interpret(std::string(600, 'a')).recognized, "oversized input");
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("Short-text property grammar: required sentences, aliases, units, period, evaluation and rejection passed.");
    return 0;
}
