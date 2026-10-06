#ifndef CPG_NGRAM_MODEL_H
#define CPG_NGRAM_MODEL_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

// Lowercase Unicode character model with interpolated 1/2/3-gram probabilities.
// Punctuation/whitespace/digits become a single word boundary; diacritics remain.
class NgramModel {
    struct Profile {
        std::array<std::unordered_map<uint64_t, uint64_t>, 3> counts;
        uint64_t total = 0;
    };
    std::unordered_map<std::string, Profile> profiles;
public:
    void add(const std::string& iso, std::u32string_view text);
    bool contains(const std::string& iso) const;
    double score(const std::string& iso, std::u32string_view text) const;
    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);
};

#endif
