#include <cpg/NgramModel.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <vector>
#include <unicode/uchar.h>

namespace {
constexpr unsigned shift = 21;

std::u32string tokens(std::u32string_view text) {
    std::u32string result;
    result.reserve(text.size());
    for (char32_t c : text) {
        if (c == 0xFEFF) continue;
        auto category = u_charType(static_cast<UChar32>(c));
        if (u_isalpha(c) || category == U_NON_SPACING_MARK ||
            category == U_COMBINING_SPACING_MARK || category == U_ENCLOSING_MARK) {
            result.push_back(static_cast<char32_t>(u_tolower(c)));
        } else if (u_ispunct(c) || u_isUWhiteSpace(c) || u_isdigit(c)) {
            if (!result.empty() && result.back() != U' ') result.push_back(U' ');
        } else {
            // Keep controls, undefined bytes and unusual symbols as evidence.
            result.push_back(c);
        }
    }
    return result;
}

uint64_t count(const std::unordered_map<uint64_t, uint64_t>& counts, uint64_t key) {
    auto it = counts.find(key);
    return it == counts.end() ? 0 : it->second;
}
}

void NgramModel::add(const std::string& iso, std::u32string_view text) {
    auto input = tokens(text);
    if (input.empty()) return;
    auto& p = profiles[iso];
    uint64_t previous = 0, pair = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        uint64_t c = input[i];
        ++p.counts[0][c];
        if (i >= 1) ++p.counts[1][(previous << shift) | c];
        if (i >= 2) ++p.counts[2][(pair << shift) | c];
        pair = (previous << shift) | c;
        previous = c;
        ++p.total;
    }
}

bool NgramModel::contains(const std::string& iso) const {
    auto it = profiles.find(iso);
    return it != profiles.end() && it->second.total > 0;
}

double NgramModel::score(const std::string& iso, std::u32string_view text) const {
    auto it = profiles.find(iso);
    if (it == profiles.end() || it->second.total == 0) return 0;
    auto input = tokens(text);
    if (input.empty()) return 0;
    const auto& p = it->second;
    double logSum = 0;
    uint64_t previous = 0, pair = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        uint64_t c = input[i];
        // Additive unigram smoothing and conditional backoff avoid zero scores
        // for unseen words, while retaining evidence from malformed decoding.
        double probability = (count(p.counts[0], c) + 0.1) /
            (p.total + 0.1 * (p.counts[0].size() + 1));
        if (i >= 1)
            probability = (count(p.counts[1], (previous << shift) | c) + 4 * probability) /
                (count(p.counts[0], previous) + 4.0);
        if (i >= 2)
            probability = (count(p.counts[2], (pair << shift) | c) + 4 * probability) /
                (count(p.counts[1], pair) + 4.0);
        logSum += std::log(probability);
        pair = (previous << shift) | c;
        previous = c;
    }
    return std::exp(logSum / input.size());
}

void NgramModel::save(const std::filesystem::path& path) const {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write model: " + path.string());
    out << "CPG_NGRAM 1\n";
    std::vector<std::string> names;
    for (const auto& item : profiles) names.push_back(item.first);
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
        const auto& p = profiles.at(name);
        out << "language " << name << ' ' << p.total << '\n';
        for (size_t n = 0; n < 3; ++n) {
            std::vector<std::pair<uint64_t, uint64_t>> sorted(p.counts[n].begin(), p.counts[n].end());
            std::sort(sorted.begin(), sorted.end());
            out << "order " << n + 1 << ' ' << sorted.size() << '\n';
            for (auto entry : sorted) out << entry.first << ' ' << entry.second << '\n';
        }
    }
    out << "end\n";
    out.close();
    if (!out) throw std::runtime_error("Failed writing model: " + path.string());
}

void NgramModel::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot read model: " + path.string());
    auto fail = [&]() { throw std::runtime_error("Invalid model: " + path.string()); };
    std::string token;
    int version;
    if (!(in >> token >> version) || token != "CPG_NGRAM" || version != 1) fail();
    std::unordered_map<std::string, Profile> loaded;
    while (in >> token) {
        if (token == "end") {
            if (in >> token) fail();
            profiles = std::move(loaded);
            return;
        }
        std::string iso;
        Profile p;
        if (token != "language" || !(in >> iso >> p.total) || p.total == 0 || loaded.count(iso)) fail();
        for (size_t n = 0; n < 3; ++n) {
            size_t order, entries;
            if (!(in >> token >> order >> entries) || token != "order" || order != n + 1 ||
                entries > 10000000) fail();
            uint64_t sum = 0;
            for (size_t j = 0; j < entries; ++j) {
                uint64_t key, value;
                if (!(in >> key >> value) || value == 0 || value > p.total ||
                    (key >> ((n + 1) * shift)) != 0 ||
                    sum > std::numeric_limits<uint64_t>::max() - value ||
                    !p.counts[n].emplace(key, value).second) fail();
                sum += value;
            }
            if ((n == 0 && sum != p.total) || sum > p.total) fail();
        }
        loaded.emplace(iso, std::move(p));
    }
    fail();
}
