#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#include <json.hpp>
#include <atomic>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#pragma comment(lib, "bcrypt.lib")

namespace KashipanEngine::ContentCache {
using Json = nlohmann::json;
inline std::string PathText(const std::filesystem::path &path) {
    const auto text = path.generic_u8string();
    return std::string(reinterpret_cast<const char *>(text.data()), text.size());
}
inline std::filesystem::path Path(const std::string &text) { return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size())); }
inline std::string Digest(const void *data, size_t size) {
    if (size > ULONG_MAX) return {};
    unsigned char hash[32]{};
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
        reinterpret_cast<PUCHAR>(const_cast<void *>(data)), static_cast<ULONG>(size), hash, sizeof(hash)) < 0) return {};
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (const auto value : hash) { result += hex[value >> 4]; result += hex[value & 15]; }
    return result;
}
inline std::string Digest(const std::string &text) { return Digest(text.data(), text.size()); }
inline std::optional<std::vector<char>> ReadFile(const std::filesystem::path &path) {
    try {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream) return std::nullopt;
        const auto size = stream.tellg();
        if (size < 0 || size > 1024LL * 1024 * 1024) return std::nullopt;
        std::vector<char> bytes(static_cast<size_t>(size));
        stream.seekg(0);
        if (!bytes.empty() && !stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) return std::nullopt;
        return bytes;
    } catch (...) { return std::nullopt; }
}
inline std::string FileDigest(const std::filesystem::path &path) {
    const auto bytes = ReadFile(path);
    return bytes ? Digest(bytes->data(), bytes->size()) : std::string{};
}
inline Json Dependency(const std::filesystem::path &path) {
    return Json{{"path", PathText(std::filesystem::absolute(path).lexically_normal())}, {"hash", FileDigest(path)}};
}
inline bool DependenciesValid(const Json &dependencies) {
    if (!dependencies.is_array() || dependencies.empty()) return false;
    for (const auto &dep : dependencies) {
        if (dep.value("missing", false)) {
            std::error_code ec;
            if (std::filesystem::exists(Path(dep.at("path").get<std::string>()), ec) || ec) return false;
            continue;
        }
        const auto expected = dep.at("hash").get<std::string>();
        if (expected.empty() || FileDigest(Path(dep.at("path").get<std::string>())) != expected) return false;
    }
    return true;
}
/// One atomic container: manifest and payload can never belong to different writes.
inline bool Store(const std::filesystem::path &path, const std::string &key,
    const Json &dependencies, const void *data, size_t size) noexcept {
    std::filesystem::path temporary;
    try {
        if (key.empty() || !size || !DependenciesValid(dependencies)) return false;
        const auto hash = Digest(data, size);
        if (hash.empty()) return false;
        const auto metadata = Json{{"version", 1}, {"key", key}, {"dependencies", dependencies}, {"hash", hash}}.dump();
        const uint64_t length = metadata.size();
        std::filesystem::create_directories(path.parent_path());
        static std::atomic_uint64_t sequence{0};
        temporary = path;
        temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++sequence);
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char *>(&length), sizeof(length));
        stream.write(metadata.data(), static_cast<std::streamsize>(metadata.size()));
        stream.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
        stream.close();
        if (!stream || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary);
            return false;
        }
        return true;
    } catch (...) {
        std::error_code ignored;
        if (!temporary.empty()) std::filesystem::remove(temporary, ignored);
        return false;
    }
}
inline std::optional<std::vector<char>> Load(const std::filesystem::path &path, const std::string &key) noexcept {
    try {
        auto bytes = ReadFile(path);
        if (!bytes || bytes->size() <= sizeof(uint64_t)) return std::nullopt;
        uint64_t length = 0;
        std::memcpy(&length, bytes->data(), sizeof(length));
        if (length > 1024 * 1024 || length >= bytes->size() - sizeof(length)) return std::nullopt;
        const auto metadata = Json::parse(bytes->begin() + sizeof(length), bytes->begin() + sizeof(length) + length);
        if (metadata.at("version") != 1 || metadata.at("key") != key || !DependenciesValid(metadata.at("dependencies"))) return std::nullopt;
        std::vector<char> payload(bytes->begin() + sizeof(length) + length, bytes->end());
        if (Digest(payload.data(), payload.size()) != metadata.at("hash").get<std::string>()) return std::nullopt;
        return payload;
    } catch (...) { return std::nullopt; }
}
/// Keep generated data outside Assets, so asset watchers never import cache files.
inline std::filesystem::path Directory(const std::string &projectRoot, const std::string &category) {
    auto preferred = Path(projectRoot) / "Cache" / "Startup" / category;
    try {
        std::filesystem::create_directories(preferred);
        const auto probe = preferred / (".probe-" + std::to_string(GetCurrentProcessId()));
        std::ofstream stream(probe);
        const bool writable = stream.good();
        stream.close();
        std::error_code ignored;
        std::filesystem::remove(probe, ignored);
        if (writable) return preferred;
    } catch (...) {}
    wchar_t local[32768]{};
    const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768);
    if (count && count < 32768)
        return std::filesystem::path(local) / "KashipanEngine" / "StartupCache" / Digest(projectRoot) / category;
    return preferred; // A write failure simply disables caching.
}
} // namespace KashipanEngine::ContentCache
