#ifndef CPG_NGRAM_MODEL_H
#define CPG_NGRAM_MODEL_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Lowercase Unicode character model with interpolated 1/2/3-gram probabilities.
// Punctuation/whitespace/digits become a single word boundary; diacritics remain.
class NgramModel {
    struct Profile {
        std::array<std::unordered_map<uint64_t, uint64_t>, 3> counts;
        uint64_t total = 0;
    };
    std::unordered_map<std::string, Profile> profiles;
    unsigned retainedPercent = 100;
public:
    void add(const std::string& iso, std::u32string_view text);
    // Keep the most frequent fraction of each language's 3-grams.
    // All 1/2-grams remain for backoff and original conditional denominators.
    // Pruning is applied once, to a complete model, with percentages 1..100.
    void prune(unsigned keepPercent);
    unsigned keepPercent() const { return retainedPercent; }
    void retainLanguage(const std::string& iso);
    std::vector<std::string> languageCodes() const;
    NgramModel forLanguage(const std::string& iso) const;
    bool contains(const std::string& iso) const;
    double score(const std::string& iso, std::u32string_view text) const;
    void save(const std::filesystem::path& path, bool compactBinary = false) const;
    void load(const std::filesystem::path& path);
    void loadDirectory(const std::filesystem::path& path);
};

#endif
