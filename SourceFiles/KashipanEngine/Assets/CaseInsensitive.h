#pragma once
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>

struct CaseInsensitiveHash {
    size_t operator()(const std::string &s) const noexcept {
        std::string lower;
        lower.resize(s.size());
        std::transform(s.begin(), s.end(), lower.begin(),
            [](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });
        return std::hash<std::string>()(lower);
    }
};

struct CaseInsensitiveEqual {
    bool operator()(const std::string &a, const std::string &b) const noexcept {
        if (a.size() != b.size()) return false;
        return std::equal(a.begin(), a.end(), b.begin(),
            [](unsigned char c1, unsigned char c2) {
                return std::tolower(c1) == std::tolower(c2);
            });
    }
};

template<typename T>
using FileMap = std::unordered_map<std::string, T, CaseInsensitiveHash, CaseInsensitiveEqual>;