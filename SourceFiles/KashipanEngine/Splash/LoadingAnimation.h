#pragma once
#include "Utilities/KeyframeAnimation.h"
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
namespace KashipanEngine::LoadingAnimation {
enum class Section { Intro, Waiting, Outro, Common, Finished };
struct Timing { double seconds = 0; bool cycleEnd = true; std::string source; };
struct Frame { Section section = Section::Finished; double local = 0, elapsed = 0; bool finished = false; double waitingEnd = 0; };
struct Node {
    std::string id, name, image;
    int parent = -1, section = -1, priority = 0;
    bool active = true, sprite = false;
    float x=0,y=0,sx=1,sy=1,rotation=0,anchorX=.5f,anchorY=.5f;
    float r=1,g=1,b=1,a=1,u=0,v=0,us=1,vs=1,opacity=1;
    float materialR=1,materialG=1,materialB=1,materialA=1,materialU=0,materialV=0,materialUS=1,materialVS=1;
    int colorBlend=0,uvCombine=0;
};
struct Track {
    int owner=-1, target=-1, section=-1, channel=0;
    std::string property, name;
    KeyframeAnimation animation;
    float speed=1, offset=0, scale=1, bias=0;
    bool loop=false;
};
struct Document {
    std::vector<Node> nodes;
    std::vector<Track> tracks;
    std::array<Timing,3> timing;
    std::vector<std::string> warnings, errors;
    JSON package;
    std::filesystem::path folder;
    Frame Sample(double elapsed, double readyAt) const;
    std::vector<Node> Evaluate(const Frame &frame) const;
};
// The resolver replaces scene material references with image/color data before exporting.
using MaterialResolver = std::function<bool(const std::string&, JSON&)>;
Document Compile(const JSON &scene, const MaterialResolver &material = {});
bool Export(Document &document, const std::filesystem::path &folder, std::string &error);
Document Load(const std::filesystem::path &file);
// Same D2D renderer for editor preview and pre-engine startup. COM must be initialized by caller.
class Renderer {
public:
    Renderer(); ~Renderer();
    Renderer(const Renderer&) = delete; Renderer& operator=(const Renderer&) = delete;
    bool Draw(void *hwnd, const Document&, const Frame&);
    bool IsHardware() const;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
