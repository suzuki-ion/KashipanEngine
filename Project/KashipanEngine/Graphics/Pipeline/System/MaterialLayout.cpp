#include "MaterialLayout.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>

#include "Debug/Logger.h"

namespace KashipanEngine {
namespace {

std::string ReadFileText(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/// @brief #include "X" を再帰的にインライン展開する（構造体探索が目的の簡易展開。多重インクルードガードは考慮しない）
std::string ExpandIncludes(const std::string &source, const std::filesystem::path &baseDir, std::unordered_set<std::string> &visited, int depth) {
    if (depth > 8) return source; // 無限再帰対策
    static const std::regex kIncludeRe(R"(#include\s*\"([^\"]+)\")");
    std::string result;
    result.reserve(source.size());
    auto it = std::sregex_iterator(source.begin(), source.end(), kIncludeRe);
    auto end = std::sregex_iterator();
    size_t lastPos = 0;
    for (; it != end; ++it) {
        result.append(source, lastPos, static_cast<size_t>(it->position()) - lastPos);
        std::filesystem::path includePath = baseDir / it->str(1);
        std::string canonical = includePath.lexically_normal().string();
        if (visited.find(canonical) == visited.end()) {
            visited.insert(canonical);
            std::string includedText = ReadFileText(canonical);
            result += ExpandIncludes(includedText, includePath.parent_path(), visited, depth + 1);
        }
        lastPos = static_cast<size_t>(it->position()) + it->length();
    }
    result.append(source, lastPos, source.size() - lastPos);
    return result;
}

/// @brief 単純な #ifdef/#ifndef/#else/#endif のみを評価する簡易プリプロセッサ（#if式や#define非対応）
std::string StripConditionals(const std::string &source, const std::unordered_set<std::string> &definedMacros) {
    struct CondFrame {
        bool parentActive;
        bool anyTaken;
        bool active;
    };
    std::vector<CondFrame> stack{ CondFrame{ true, true, true } };

    std::istringstream stream(source);
    std::string line;
    std::string result;
    result.reserve(source.size());

    while (std::getline(stream, line)) {
        size_t firstNonSpace = line.find_first_not_of(" \t");
        std::string directive = (firstNonSpace == std::string::npos) ? std::string{} : line.substr(firstNonSpace);

        if (directive.rfind("#ifdef", 0) == 0 || directive.rfind("#ifndef", 0) == 0) {
            bool isIfndef = directive.rfind("#ifndef", 0) == 0;
            std::string macroName = directive.substr(isIfndef ? 7 : 6);
            size_t s = macroName.find_first_not_of(" \t");
            size_t e = macroName.find_last_not_of(" \t\r");
            macroName = (s == std::string::npos) ? std::string{} : macroName.substr(s, e - s + 1);

            bool defined = definedMacros.count(macroName) > 0;
            bool cond = isIfndef ? !defined : defined;
            bool parentActive = stack.back().active;
            bool active = parentActive && cond;
            stack.push_back(CondFrame{ parentActive, active, active });
            continue;
        }
        if (directive.rfind("#else", 0) == 0) {
            CondFrame &frame = stack.back();
            bool active = frame.parentActive && !frame.anyTaken;
            frame.anyTaken = frame.anyTaken || active;
            frame.active = active;
            continue;
        }
        if (directive.rfind("#endif", 0) == 0) {
            if (stack.size() > 1) stack.pop_back();
            continue;
        }
        if (stack.back().active) {
            result += line;
            result += '\n';
        }
    }
    return result;
}

/// @brief ブロックコメント(/* */)のみを除去する（行コメント// @Range等の注釈を読み取るために残す）
std::string StripBlockCommentsOnly(const std::string &source) {
    std::string result;
    result.reserve(source.size());
    bool inBlockComment = false;
    for (size_t i = 0; i < source.size(); ++i) {
        if (inBlockComment) {
            if (source[i] == '*' && i + 1 < source.size() && source[i + 1] == '/') {
                inBlockComment = false;
                ++i;
            }
            continue;
        }
        if (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '*') {
            inBlockComment = true;
            ++i;
            continue;
        }
        result += source[i];
    }
    return result;
}

struct HlslTypeInfo {
    std::uint32_t byteSize;
    ValueType valueType;
};

bool ResolveHlslType(const std::string &typeName, HlslTypeInfo &out) {
    static const std::unordered_map<std::string, HlslTypeInfo> kTypeMap = {
        { "float",    { 4,  ValueType::Float } },
        { "float2",   { 8,  ValueType::Vector2 } },
        { "float3",   { 12, ValueType::Vector3 } },
        { "float4",   { 16, ValueType::Vector4 } },
        { "float4x4", { 64, ValueType::Matrix4x4 } },
        { "int",      { 4,  ValueType::Int32 } },
        { "uint",     { 4,  ValueType::UInt32 } },
        { "bool",     { 4,  ValueType::Bool } },
    };
    auto it = kTypeMap.find(typeName);
    if (it == kTypeMap.end()) return false;
    out = it->second;
    return true;
}

/// @brief フィールド宣言末尾の注釈テキスト（"// " より後ろ）から @Color / @Range(...) を読み取り、
///        fieldへ反映する。@ColorはvalueTypeをVector4からColorへ差し替える（呼び出し側でfloat4の
///        場合のみ意味を持つ。それ以外の型に付いていても無視する）
void ApplyAnnotations(const std::string &annotationText, MaterialFieldLayout &field) {
    if (annotationText.empty()) return;

    if (annotationText.find("@Color") != std::string::npos && field.valueType == ValueType::Vector4) {
        field.valueType = ValueType::Color;
    }

    static const std::regex kRangeRe(R"(@Range\(\s*([+-]?[0-9]*\.?[0-9]+)\s*,\s*([+-]?[0-9]*\.?[0-9]+)\s*(?:,\s*([+-]?[0-9]*\.?[0-9]+)\s*)?\))");
    std::smatch rangeMatch;
    if (std::regex_search(annotationText, rangeMatch, kRangeRe)) {
        field.hasRange = true;
        field.rangeMin = std::stof(rangeMatch[1].str());
        field.rangeMax = std::stof(rangeMatch[2].str());
        field.rangeStep = rangeMatch[3].matched ? std::stof(rangeMatch[3].str()) : (field.rangeMax - field.rangeMin) / 100.0f;
    }
}

/// @brief 前処理済みソースから、指定した種類のテクスチャ宣言（`<Kind> <name> : register(tN);`）の
///        変数名を列挙する共通実装
std::vector<std::string> ScanTextureDeclarationsOfKind(const std::string &source, const char *kind) {
    const std::regex declRe(std::string(kind) + R"(\s+(\w+)\s*:\s*register\(\s*t\d+\s*\))");
    std::vector<std::string> names;
    auto it = std::sregex_iterator(source.begin(), source.end(), declRe);
    auto end = std::sregex_iterator();
    for (; it != end; ++it) {
        names.push_back((*it)[1].str());
    }
    return names;
}

/// @brief 前処理済みソースから `Texture2D <name> : register(tN);` 宣言の変数名を列挙する
std::vector<std::string> ScanTextureDeclarations(const std::string &source) {
    return ScanTextureDeclarationsOfKind(source, "Texture2D");
}

/// @brief 前処理済みソースから `TextureCube <name> : register(tN);` 宣言の変数名を列挙する
std::vector<std::string> ScanTextureCubeDeclarations(const std::string &source) {
    return ScanTextureDeclarationsOfKind(source, "TextureCube");
}

} // namespace

MaterialLayout MaterialLayout::BuildFromHlslSource(const std::string &shaderFilePath, const std::unordered_set<std::string> &definedMacros) {
    MaterialLayout layout;
    if (shaderFilePath.empty()) return layout;

    std::filesystem::path filePath(shaderFilePath);
    std::string source = ReadFileText(shaderFilePath);
    if (source.empty()) {
        Log("[MaterialLayout] failed to read shader file: " + shaderFilePath, LogSeverity::Debug);
        return layout;
    }

    std::unordered_set<std::string> visited{ filePath.lexically_normal().string() };
    source = ExpandIncludes(source, filePath.parent_path(), visited, 0);
    source = StripConditionals(source, definedMacros);
    // ブロックコメントのみ除去し、行コメント（// @Range等の注釈）はここでは残す
    source = StripBlockCommentsOnly(source);

    layout.textureFields = ScanTextureDeclarations(source);
    layout.textureCubeFields = ScanTextureCubeDeclarations(source);

    static const std::regex kStructRe(R"(struct\s+Material\s*\{([^}]*)\})");
    std::smatch structMatch;
    if (!std::regex_search(source, structMatch, kStructRe)) {
        // Material構造体を持たないシェーダー（Vertex/Compute等）では正常な結果。デバッグ用途のみログ
        Log("[MaterialLayout] struct Material not found in: " + shaderFilePath, LogSeverity::Debug);
        return layout;
    }

    const std::string body = structMatch[1].str();
    // 1行ずつ走査し、"type name[n]?;" 部分と末尾の "// 注釈" 部分を分けて取り出す
    // （説明用の独立コメント行はこの正規表現にマッチせずスキップされるだけで無害）
    static const std::regex kFieldLineRe(R"(^\s*(\w+)\s+(\w+)\s*(?:\[\s*(\d+)\s*\])?\s*;\s*(?://\s*(.*?)\s*)?$)");

    std::uint32_t offset = 0;
    std::istringstream bodyStream(body);
    std::string line;
    while (std::getline(bodyStream, line)) {
        std::smatch lineMatch;
        if (!std::regex_match(line, lineMatch, kFieldLineRe)) continue;

        const std::string typeName = lineMatch[1].str();
        const std::string fieldName = lineMatch[2].str();
        const std::string arrayCountStr = lineMatch[3].str();
        const std::string annotationText = lineMatch[4].str();

        HlslTypeInfo typeInfo{};
        if (!ResolveHlslType(typeName, typeInfo)) {
            Log("[MaterialLayout] unsupported field type '" + typeName + "' for '" + fieldName + "' in: " + shaderFilePath, LogSeverity::Warning);
            continue;
        }

        std::uint32_t arrayCount = arrayCountStr.empty() ? 1 : static_cast<std::uint32_t>(std::stoul(arrayCountStr));
        std::uint32_t fieldSize = typeInfo.byteSize * arrayCount;

        MaterialFieldLayout field;
        field.name = fieldName;
        field.byteOffset = offset;
        field.byteSize = fieldSize;
        field.valueType = (arrayCount > 1) ? ValueType::None : typeInfo.valueType;
        if (arrayCount == 1) ApplyAnnotations(annotationText, field);
        layout.fields.push_back(field);

        offset += fieldSize;
    }

    layout.totalByteSize = offset;
    return layout;
}

} // namespace KashipanEngine
