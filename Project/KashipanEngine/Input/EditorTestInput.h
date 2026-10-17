#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>
namespace KashipanEngine {
// Engine-local synthetic input. Never sends operating-system input to other applications.
class EditorTestInput final {
public:
    static void Key(unsigned code, bool down, unsigned frames) { Set(keys_, code, down, frames, 256); }
    static void Button(unsigned code, bool down, unsigned frames) { Set(buttons_, code, down, frames, 8); }
    static void Pointer(int x, int y, unsigned frames) {
        if (frames < 1 || frames > 3600 || x < -32768 || x > 32767 || y < -32768 || y > 32767) throw std::runtime_error("Invalid pointer input");
        pointerX_ = x; pointerY_ = y; pointerFrames_ = frames;
    }
    static void Wheel(int delta) { if (delta < -12000 || delta > 12000) throw std::runtime_error("Invalid wheel input"); wheel_ = delta; }
    static void Clear() { keys_.clear(); buttons_.clear(); pointerFrames_ = 0; wheel_ = 0; }
    static void ApplyKeys(std::array<std::uint8_t, 256> &values) { Apply(keys_, values); }
    static void ApplyButtons(std::array<std::uint8_t, 8> &values) { Apply(buttons_, values); }
    template<class Point> static void ApplyMotion(Point &current, const Point &previous, int &dx, int &dy, int &wheel, int &totalWheel) {
        if (pointerFrames_) { current.x = pointerX_; current.y = pointerY_; dx = static_cast<int>(current.x - previous.x); dy = static_cast<int>(current.y - previous.y); --pointerFrames_; }
        wheel += wheel_; totalWheel += wheel_; wheel_ = 0;
    }
private:
    struct State { bool down; unsigned frames; };
    static inline std::map<unsigned, State> keys_, buttons_;
    static inline int pointerX_ = 0, pointerY_ = 0, wheel_ = 0;
    static inline unsigned pointerFrames_ = 0;
    static void Set(std::map<unsigned, State> &values, unsigned code, bool down, unsigned frames, unsigned size) {
        if (code >= size || frames < 1 || frames > 3600) throw std::runtime_error("Invalid synthetic input");
        values[code] = {down, frames};
    }
    template<size_t N> static void Apply(std::map<unsigned, State> &states, std::array<std::uint8_t, N> &values) {
        for (auto it = states.begin(); it != states.end();) {
            values[it->first] = it->second.down ? 0x80 : 0;
            if (--it->second.frames == 0) it = states.erase(it); else ++it;
        }
    }
};
}
