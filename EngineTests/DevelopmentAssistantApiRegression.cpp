#include "Utilities/DevelopmentAssistantApiContext.h"
#include <cassert>
#include <fstream>
#include <iostream>
#define NOMINMAX
#include <Windows.h>
using namespace KashipanEngine::DevelopmentAI;
int main() {
    wchar_t temp[32768]; GetTempPathW(32768, temp);
    const auto root = std::filesystem::path(temp) / (L"KashipanApiTest-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root / L"SourceFiles/KashipanEngine/Utilities");
    std::filesystem::create_directories(root / L"SourceFiles/KashipanEngine/Scene/Components/Script");
    std::filesystem::create_directories(root / L"Docs/Reference/Script");
    std::ofstream(root / L"SourceFiles/KashipanEngine/Utilities/WaveFunctionCollapse.h") << "class WaveFunctionCollapse { public: void SourceOnly(); };\n";
    std::ofstream(root / L"SourceFiles/KashipanEngine/Scene/Components/Script/ScriptBindings.cpp") <<
        "#include \"Utilities/WaveFunctionCollapse.h\"\n// UNRELATED INCLUDE CONTEXT\n" + std::string(400, '\n') +
        "class ScriptWaveFunctionCollapse {};\n.method(\"bool Solve()\", &ScriptWaveFunctionCollapse::Solve);\n";
    std::ofstream(root / L"Docs/Reference/Script/10_ProceduralGeneration.html") <<
        "<script>HiddenScript</script><h2>WaveFunctionCollapse</h2><pre>bool ReferenceOnly(string &amp;out name)</pre>\n";
    std::ofstream(root / L"Docs/Reference/Script/00_Index.html") << "<p>WaveFunctionCollapse</p>\n";
    auto source = BuildApiContext(root, "Wave Function Collapseで街を作って");
    assert(source.sourceFiles >= 1 && source.referenceFiles == 0);
    assert(source.text.find("SourceOnly") != std::string::npos && source.text.find("bool Solve()") != std::string::npos);
    assert(source.text.find("UNRELATED INCLUDE CONTEXT") == std::string::npos);
    std::filesystem::remove(root / L"SourceFiles/KashipanEngine/Utilities/WaveFunctionCollapse.h");
    std::filesystem::remove(root / L"SourceFiles/KashipanEngine/Scene/Components/Script/ScriptBindings.cpp");
    auto reference = BuildApiContext(root, "WFCで街を生成");
    assert(reference.sourceFiles == 0 && reference.referenceFiles == 1);
    assert(reference.text.find("ReferenceOnly(string &out name)") != std::string::npos);
    assert(reference.text.find("HiddenScript") == std::string::npos);
    auto japanese = BuildApiContext(root, "波動関数崩壊を使う");
    assert(japanese.referenceFiles == 1);
    assert(BuildApiContext(root, "WFC", 1024).text.size() <= 1024);
    std::filesystem::remove(root / L"Docs/Reference/Script/10_ProceduralGeneration.html");
    std::filesystem::remove(root / L"Docs/Reference/Script/00_Index.html");
    auto missing = BuildApiContext(root, "WFC");
    assert(missing.sourceFiles == 0 && missing.referenceFiles == 0);
    // Only this test's known files/directories are removed; no recursive cleanup of a computed root.
    std::filesystem::remove(root / L"SourceFiles/KashipanEngine/Scene/Components/Script");
    std::filesystem::remove(root / L"SourceFiles/KashipanEngine/Scene/Components");
    std::filesystem::remove(root / L"SourceFiles/KashipanEngine/Scene");
    std::filesystem::remove(root / L"SourceFiles/KashipanEngine/Utilities");
    std::filesystem::remove(root / L"SourceFiles/KashipanEngine");
    std::filesystem::remove(root / L"Docs/Reference/Script"); std::filesystem::remove(root / L"Docs/Reference"); std::filesystem::remove(root / L"SourceFiles"); std::filesystem::remove(root / L"Docs"); std::filesystem::remove(root);
    std::cout << "Development assistant API source/reference regression passed\n";
}
