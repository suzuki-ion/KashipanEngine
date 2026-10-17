// Real scene/Undo plus authenticated loopback MCP. No window, GPU, model or credentials.
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>
#include <stdexcept>
#include "Scene/Scene.h"
#include "Scene/SceneContext.h"
#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/DevelopmentAssistantApi.h"
#include "Objects/Components/Transform.h"
#include "Utilities/DevelopmentAssistantTools.h"
#include "Utilities/DevelopmentAssistant.h"
#include <json.hpp>
using namespace KashipanEngine;
using J = nlohmann::json;
namespace {
void Require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
std::string Http(const DevelopmentAI::EditorTools &tools, const J &body, bool auth = true, const std::string &extra = "") {
    const auto portEnd = tools.Url().find('/', 7);
    const auto host = tools.Url().substr(7, portEnd - 7);
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<unsigned short>(std::stoi(host.substr(host.find(':') + 1))));
    SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address))) { closesocket(socket); throw std::runtime_error("connect failed"); }
    const auto data = body.dump();
    const auto request = "POST /mcp HTTP/1.1\r\nHost: " + host + "\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(data.size()) + "\r\n" + (auth ? "Authorization: Bearer " + tools.Token() + "\r\n" : "") + extra + "\r\n" + data;
    send(socket, request.data(), static_cast<int>(request.size()), 0);
    std::string result; char chunk[4096]; int n;
    while ((n = recv(socket, chunk, sizeof(chunk), 0)) > 0) result.append(chunk, n);
    closesocket(socket); return result;
}
J Body(const std::string &response) { return J::parse(response.substr(response.find("\r\n\r\n") + 4)); }
}
int main(int argc, char **argv) {
    try {
        Scene scene(std::string("Editor API regression"));
        SceneEditorContext context(std::bit_cast<Passkey<Scene>>(std::uint8_t{}), &scene);
        SceneEditorCommands commands(std::bit_cast<Passkey<SceneEditor>>(std::uint8_t{}), &context);
        const UUID128 none;
        auto execute = [&](auto command) { return commands.Execute(std::move(command)); };
        auto call = [&](const char *name, J args) { return J::parse(ExecuteEditorApi(&context, none, execute, name, args.dump())); };
        Require(J::parse(EditorApiCatalog()).size() == 31, "tool count");
        auto created = call("create_object", {{"name", "AI Object"}}); Require(created.at("ok"), "create");
        const auto id = created.at("id").get<std::string>();
        auto *object = context.GetSceneObject(UUID128(id)); Require(object != nullptr, "object exists");
        auto inspected = call("object", {{"id", id}}); Require(inspected.at("ok"), "inspect");
        const auto component = object->GetComponent<Transform>()->GetComponentRef().addedID;
        auto update = call("set_property", {{"id", id}, {"component", component}, {"member", "translate_"}, {"value", {2, 4, 6}}});
        Require(update.at("ok") && object->GetComponent<Transform>()->GetTranslate().y == 4, "typed setter");
        Require(commands.Undo() && object->GetComponent<Transform>()->GetTranslate().y == 0, "setter Undo");
        Require(commands.Redo() && object->GetComponent<Transform>()->GetTranslate().y == 4, "setter Redo");
        const auto before = context.SaveSceneToJSON().dump();
        Require(!call("set_property", {{"id", id}, {"component", component}, {"member", "translate_"}, {"value", {2, 4}}}).at("ok"), "dimension rejected");
        Require(!call("set_property", {{"id", id}, {"component", component}, {"member", "worldMatrix_"}, {"value", 5}}).at("ok"), "read-only rejected");
        Require(!call("delete_object", {{"id", "bad"}}).at("ok"), "invalid ID rejected");
        Require(!call("scene", {{"limit", 201}}).at("ok"), "page bound");
        Require(!call("create_object", {{"name", "bad"}, {"unexpected", true}}).at("ok"), "extra argument rejected");
        Require(context.SaveSceneToJSON().dump() == before, "rejections do not mutate");
        auto preview = call("apply_sentence", {{"id", id}, {"sentence", "1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動"}, {"preview", true}});
        Require(preview.at("ok") && context.SaveSceneToJSON().dump() == before, "preview without changes");
        Require(call("apply_sentence", {{"id", id}, {"sentence", "1秒かけてTransformの移動Y軸を0から4までEaseInOutCubicで移動"}, {"preview", false}}).at("ok"), "keyframe apply");
        Require(commands.Undo(), "keyframe Undo");
        const auto child = call("create_object", {{"name", "child"}, {"parent", id}});
        const auto childID = child.at("id").get<std::string>();
        Require(context.GetSceneObject(UUID128(childID))->GetComponent<Transform>()->GetParentObject() == object, "parent");
        Require(call("set_object", {{"id", id}, {"active", false}}).at("ok"), "disable parent");
        Require(call("set_object", {{"id", childID}, {"name", "renamed"}}).at("ok"), "rename inactive child");
        Require(context.SaveObjectToJson(context.GetSceneObject(UUID128(childID))).at("isActive"), "rename preserves local active state");
        Require(commands.Undo() && commands.Undo(), "restore child name and parent state");
        Require(call("delete_object", {{"id", id}}).at("ok") && !context.GetSceneObject(UUID128(childID)), "delete descendants");
        Require(commands.Undo() && context.GetSceneObject(UUID128(childID)), "delete Undo descendants");
        object = context.GetSceneObject(UUID128(id));
        Require(call("add_component", {{"id", id}, {"type", "Light"}}).at("ok"), "add Light");
        inspected = call("object", {{"id", id}});
        size_t light = SIZE_MAX;
        for (const auto &item : inspected.at("components")) if (item.at("type") == "Light") light = item.at("component").get<size_t>();
        Require(light != SIZE_MAX, "Light ID");
        Require(context.SaveSceneToJSON().dump().find("editorAddedID") == std::string::npos, "scene format unchanged");
        Require(context.SaveObjectToJson(object).dump().find("editorAddedID") == std::string::npos, "prefab snapshots unchanged");
        Require(context.SaveObjectToUndoJson(object).dump().find("editorAddedID") != std::string::npos, "Undo snapshot carries identity");
        Require(call("set_property", {{"id", id}, {"component", light}, {"member", "color_"}, {"value", {0.5, 0.5, 1.0, 1.0}}}).at("ok"), "4D color");
        Require(call("remove_component", {{"id", id}, {"component", light}}).at("ok"), "remove component");
        Require(!call("set_property", {{"id", id}, {"component", light}, {"member", "color_"}, {"value", {1, 1, 1, 1}}}).at("ok"), "stale component rejected");
        Require(commands.Undo(), "remove Undo");
        Require(commands.Undo(), "color Undo after remove Undo preserves component identity");
        Require(commands.Redo() && commands.Redo(), "color and removal Redo");
        Require(commands.Undo(), "remove Undo again");
        Require(call("object", {{"id", id}}).at("ok"), "inspect object containing vacant component slots");
        Require(call("delete_object", {{"id", id}}).at("ok") && commands.Undo(), "object delete/restore");
        Require(commands.Undo(), "color Undo after object restore preserves component identity");
        Require(commands.Undo(), "add Undo after object restore preserves component identity");
        const auto epoch = commands.GetHistoryEpoch(); commands.Clear(); Require(commands.GetHistoryEpoch() != epoch, "scene reload epoch");

        auto tools = std::make_shared<DevelopmentAI::EditorTools>(EditorApiCatalog());
        const J list{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};
        Require(Http(*tools, list, false).find(" 401 ") != std::string::npos, "authentication");
        Require(Http(*tools, list, true, "Origin: https://example.com\r\n").find(" 403 ") != std::string::npos, "origin rejection");
        Require(Body(Http(*tools, list)).at("result").at("tools").size() == 31, "MCP tools");
        Require(Body(Http(*tools, {{"jsonrpc", "2.0"}, {"id", 0}, {"method", "initialize"},
            {"params", {{"protocolVersion", "2025-03-26"}}}})).at("result").at("protocolVersion") == "2025-03-26", "MCP initialize");
        auto request = std::async(std::launch::async, [&] { return Http(*tools, {{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/call"},
            {"params", {{"name", "create_object"}, {"arguments", {{"name", "MCP-created"}}}}}}); });
        for (int i = 0; i < 100 && request.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready; ++i) {
            tools->Pump([&](const auto &name, const auto &args) { return ExecuteEditorApi(&context, none, execute, name, args); });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        Require(!Body(request.get()).at("result").at("isError").get<bool>(), "MCP main-thread mutation");
        Require(commands.CanUndo(), "MCP Undo recorded");
        auto imageRequest = std::async(std::launch::async, [&] { return Http(*tools, {{"jsonrpc", "2.0"}, {"id", 10}, {"method", "tools/call"},
            {"params", {{"name", "capture"}, {"arguments", J::object()}}}}); });
        for (int i = 0; i < 100 && imageRequest.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready; ++i) {
            tools->Pump([&](const auto &, const auto &) { return J{{"ok", true}, {"path", "Assets/test.png"},
                {"_mcp_images", J::array({{{"type", "image"}, {"mimeType", "image/png"}, {"data", "iVBORw0KGgo="}}})}}.dump(); });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        const auto imageContent = Body(imageRequest.get()).at("result").at("content");
        Require(imageContent.size() == 2 && imageContent.at(1).at("type") == "image" && imageContent.at(1).at("mimeType") == "image/png", "MCP image content");
        Require(imageContent.at(0).at("text").get<std::string>().find("iVBOR") == std::string::npos, "image base64 omitted from text");
        auto expired = std::async(std::launch::async, [&] { return Http(*tools, {{"jsonrpc", "2.0"}, {"id", 9}, {"method", "tools/call"},
            {"params", {{"name", "create_object"}, {"arguments", {{"name", "expired-must-not-create"}}}}}}); });
        Require(Body(expired.get()).at("result").at("isError"), "unserviced request times out");
        bool late = false;
        tools->Pump([&](const auto &, const auto &) { late = true; return std::string("{}"); });
        Require(!late, "expired request never executes later");
        auto revoked = std::async(std::launch::async, [&] { return Http(*tools, {{"jsonrpc", "2.0"}, {"id", 3}, {"method", "tools/call"},
            {"params", {{"name", "create_object"}, {"arguments", {{"name", "must-not-create"}}}}}}); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); tools->Revoke();
        tools->Pump([&](const auto &, const auto &) -> std::string { throw std::runtime_error("must not execute"); });
        Require(Body(revoked.get()).at("result").at("isError"), "revocation rejects pending work");
        tools->Stop();
        if ((argc > 1 && std::string(argv[1]) == "--live-codex") || std::getenv("KASHIPAN_EDITOR_API_LIVE")) {
            const auto *live = std::getenv("KASHIPAN_EDITOR_API_LIVE");
            const auto provider = live && std::string(live) == "claude" ? DevelopmentAI::Provider::Claude : DevelopmentAI::Provider::Codex;
            tools = std::make_shared<DevelopmentAI::EditorTools>(EditorApiCatalog());
            DevelopmentAI::Session session; session.Refresh("", "");
            while (session.Get().busy) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            Require((provider == DevelopmentAI::Provider::Claude ? session.Get().claude : session.Get().codex).subscription, "CLI subscription");
            Require(session.Start(provider, DevelopmentAI::Mode::EditScene, provider == DevelopmentAI::Provider::Claude ? "" : "gpt-6-luna", std::filesystem::current_path(),
                "Use kashipan_editor MCP only. Call scene, then create_object with name LIVE_MCP_Verified. Inspect it with object. Do not use shell or file tools. Report the resulting UUID.", "auto", tools), "start live CLI");
            while (session.Get().busy) {
                tools->Pump([&](const auto &name, const auto &args) {
                    auto result = ExecuteEditorApi(&context, none, execute, name, args);
                    std::printf("MCP %s -> %s\n", name.c_str(), J::parse(result).value("ok", false) ? "ok" : "error");
                    return result;
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            std::printf("%s\n%s\n%s\n", session.Get().status.c_str(), session.Get().output.c_str(), session.Get().diagnostics.c_str());
            Require(context.GetSceneObject("LIVE_MCP_Verified") != nullptr, "actual CLI created live object");
            tools->Stop();
        }
        std::puts("DevelopmentAssistantEditorRegression passed");
        return 0;
    } catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
