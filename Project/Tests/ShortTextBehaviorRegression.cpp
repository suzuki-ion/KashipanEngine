#include "Utilities/ShortTextBehavior.h"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace KashipanEngine::ShortText;
void Require(bool value, const std::string &message) { if (!value) throw std::runtime_error(message); }
void Near(double actual, double expected, const char *message) { Require(std::abs(actual - expected) < 0.0001, message); }
int main() {
    try {
        struct Case { const char *text; Kind kind; int axis; int sign; };
        const Case cases[] = {
            {"上下にふわふわ動かして", Kind::Oscillate, 1, 1},
            {"ゆっくり浮いたり沈んだりして", Kind::Oscillate, 1, 1},
            {"左右にゆらゆら動かして", Kind::Oscillate, 0, 1},
            {"前後に往復して", Kind::Oscillate, 2, 1},
            {"上下に動かして", Kind::Oscillate, 1, 1},
            {"縦方向に往復させて", Kind::Oscillate, 1, 1},
            {"横に往復して", Kind::Oscillate, 0, 1},
            {"選択中のオブジェクトをＸ軸にふわふわさせて。", Kind::Oscillate, 0, 1},
            {"ゆっくり右に回して", Kind::Rotate, 2, -1},
            {"反時計回りに回転して", Kind::Rotate, 2, 1},
            {"左回りに回して", Kind::Rotate, 2, 1},
            {"Y軸で毎秒90度回転して", Kind::Rotate, 1, 1},
            {"右に毎秒2で動かして", Kind::Move, 0, 1},
            {"左へ移動してください", Kind::Move, 0, -1},
            {"上に進めて", Kind::Move, 1, 1},
            {"下に移動させて", Kind::Move, 1, -1},
            {"前へ進む", Kind::Move, 2, 1},
            {"後ろへ動かして", Kind::Move, 2, -1},
            {"速く右に移動して", Kind::Move, 0, 1},
            {"ゆっくりめ左に動かして", Kind::Move, 0, -1}
        };
        for (const auto &c : cases) {
            const auto parsed = Parse(c.text);
            Require(parsed.success, std::string(c.text) + ": " + parsed.message);
            Require(parsed.recipe.kind == c.kind && parsed.recipe.axis == c.axis && parsed.recipe.direction == c.sign, c.text);
        }
        const auto numeric = Parse("左右に振幅２．５で周期３秒で往復して");
        Require(numeric.success, numeric.message);
        Near(numeric.recipe.amplitude, 2.5, "full-width amplitude");
        Near(numeric.recipe.period, 3, "explicit period");
        Near(Parse("ゆっくりふわふわして").recipe.period, 4, "slow period");
        Near(Parse("右に速度2単位/秒で移動して").recipe.speed, 2, "movement unit");
        Near(Parse("回転して").recipe.speed, 90, "default rotation speed");
        Require(Parse("上下に3秒周期で往復して").success, "suffix period");
        const char *rejected[] = {
            "", "　", "右に動かさないで", "回転せずに動かして", "触れたら消して",
            "近づいたら右に動かして", "回してそして動かして", "猫っぽく動かして",
            "右に動かして消して", "右にふわふわして", "上下左右に往復して",
            "左右に移動して毎秒2で", "動かして", "x軸に移動して", "右と左に動かして",
            "ゆっくり速く回して", "ゆっくり毎秒90度回して", "振幅0で往復して",
            "振幅-1で往復して", "周期0秒で往復して", "周期0.01秒で往復して",
            "周期4000秒で往復して", "右に毎秒0で動かして", "右に速度10001で動かして",
            "右に毎秒2度で移動して", "毎秒2単位で回転して", "振幅2で回して",
            "毎秒2で往復して", "周期2秒で周期3秒で往復して", "速度1で毎秒2で回して",
            "右に速度1e3で動かして", "右に毎秒999999999999999999999999999999999999999999999999で動かして",
            "x軸とy軸に回して", "回転させながら右に移動して", "止めて", "回転寿司"
        };
        // Paired directions imply oscillation, so speed is intentionally rejected.
        for (const auto *text : rejected) Require(!Parse(text).success, std::string("must reject: ") + text);
        Require(!Parse(std::string(513, 'a')).success, "input length limit");
        const auto bob = Parse("上下に振幅2で周期4秒で往復して").recipe;
        Near(EvaluateDelta(bob, 0, 1).translation[1], 2, "quarter-cycle offset");
        double sum = 0;
        for (int i = 0; i < 400; ++i) sum += EvaluateDelta(bob, i * 0.01, 0.01).translation[1];
        Near(sum, 0, "full cycle returns to origin");
        Near(EvaluateDelta(Parse("右に毎秒2で動かして").recipe, 0, 0.5).translation[0], 1, "movement displacement");
        Near(EvaluateDelta(Parse("右に毎秒90度回して").recipe, 0, 1).rotation[2], -1.57079632679, "degrees to radians and clockwise sign");
        Near(EvaluateDelta(bob, 0, -1).translation[1], 0, "negative dt rejected");
        Near(EvaluateDelta(bob, 0, std::numeric_limits<double>::quiet_NaN()).translation[1], 0, "NaN dt rejected");
        std::cout << "Short-text parser: 20 phrase cases, 36 rejection cases, numeric and motion regression passed.\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
