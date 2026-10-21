#pragma once
#ifdef USE_IMGUI
#include "Utilities/DevelopmentAssistant.h"
#include "Utilities/UUID128.h"
#include "Utilities/DevelopmentAssistantTools.h"
namespace KashipanEngine {
class EmptyObject;
class SceneEditorContext;
class SceneEditorCommands;
class DevelopmentAssistantEditor final {
public:
    bool open = false;
    ~DevelopmentAssistantEditor();
    // A returned sentence still needs the normal interpret/preview/apply workflow.
    bool Show(SceneEditorContext *context, SceneEditorCommands *commands, EmptyObject *selected, std::string &sentence,
        const std::function<std::string(const std::string &, const std::string &)> &host = {}, const UUID128 &hostInstance = UUID128{});
private:
    void Load(const std::string &root);
    void Save();
    DevelopmentAI::Session session_;
    std::shared_ptr<DevelopmentAI::EditorTools> tools_;
    size_t requestEpoch_ = 0;
    std::string project_, claudePath_, codexPath_, claudeModel_, codexModel_;
    std::string prompt_, message_;
    std::string apiLog_;
    std::string expectedScene_;
    bool requireSceneRefresh_ = false;
    int provider_ = 0, mode_ = 0;
    int windowsBackend_ = 0;
    bool scanned_ = false, filesAuthorized_ = false, sceneAuthorized_ = false;
    UUID128 target_, scene_;
    UUID128 requestHost_;
    std::string requestProject_;
};
}
#endif
