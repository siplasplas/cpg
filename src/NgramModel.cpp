#include <cpg/NgramModel.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <unicode/uchar.h>
#include <zlib.h>

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

void writeVarint(std::ostream& out, uint64_t value) {
    while (value >= 128) {
        out.put(static_cast<char>((value & 127) | 128));
        value >>= 7;
    }
    out.put(static_cast<char>(value));
}

uint64_t readVarint(std::istream& in) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 10; ++i) {
        int raw = in.get();
        if (raw == std::char_traits<char>::eof() || (i == 9 && (raw & 0xFE)))
            throw std::runtime_error("Invalid model integer");
        value |= static_cast<uint64_t>(raw & 127) << (7 * i);
        if (!(raw & 128)) return value;
    }
    throw std::runtime_error("Invalid model integer");
}
}

void NgramModel::add(const std::string& iso, std::u32string_view text) {
    if (retainedPercent != 100)
        throw std::runtime_error("Cannot train a pruned model; train the complete model first");
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

void NgramModel::prune(unsigned keepPercent) {
    if (keepPercent < 1 || keepPercent > 100)
        throw std::invalid_argument("keep-percent must be between 1 and 100");
    if (keepPercent == 100) return;
    if (retainedPercent != 100)
        throw std::runtime_error("Model is already pruned; use the complete model as input");

    auto mostFrequent = [keepPercent](const auto& counts) {
        std::vector<std::pair<uint64_t, uint64_t>> entries(counts.begin(), counts.end());
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.second != b.second ? a.second > b.second : a.first < b.first;
        });
        entries.resize((entries.size() * keepPercent + 99) / 100);
        return entries;
    };
    for (auto& [iso, p] : profiles) {
        std::unordered_map<uint64_t, uint64_t> triples;
        for (auto entry : mostFrequent(p.counts[2])) {
            triples.insert(entry);
            // Preserve original prefix counts, rather than recomputing them
            // from retained children and inflating conditional probabilities.
            // All bigrams are retained, including this trigram's prefix.
        }
        p.counts[2] = std::move(triples);
    }
    retainedPercent = keepPercent;
}

bool NgramModel::contains(const std::string& iso) const {
    auto it = profiles.find(iso);
    return it != profiles.end() && it->second.total > 0;
}

void NgramModel::retainLanguage(const std::string& iso) {
    if (!contains(iso)) throw std::invalid_argument("Model has no language: " + iso);
    for (auto it = profiles.begin(); it != profiles.end();) {
        if (it->first == iso) ++it;
        else it = profiles.erase(it);
    }
}

std::vector<std::string> NgramModel::languageCodes() const {
    std::vector<std::string> result;
    for (const auto& [iso, profile] : profiles) result.push_back(iso);
    std::sort(result.begin(), result.end());
    return result;
}

NgramModel NgramModel::forLanguage(const std::string& iso) const {
    if (!contains(iso)) throw std::invalid_argument("Model has no language: " + iso);
    NgramModel result;
    result.profiles.emplace(iso, profiles.at(iso));
    result.retainedPercent = retainedPercent;
    return result;
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
        if (i >= 1) {
            auto occurrences = count(p.counts[1], (previous << shift) | c);
            // A missing retained n-gram is deliberately omitted evidence,
            // not a claim that the sequence never occurred in the corpus.
            if (retainedPercent == 100 || occurrences > 0)
                probability = (occurrences + 4 * probability) /
                    (count(p.counts[0], previous) + 4.0);
        }
        if (i >= 2) {
            auto occurrences = count(p.counts[2], (pair << shift) | c);
            if (retainedPercent == 100 || occurrences > 0)
                probability = (occurrences + 4 * probability) /
                    (count(p.counts[1], pair) + 4.0);
        }
        logSum += std::log(probability);
        pair = (previous << shift) | c;
        previous = c;
    }
    return std::exp(logSum / input.size());
}

void NgramModel::save(const std::filesystem::path& path, bool compactBinary) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write model: " + path.string());
    std::ostringstream binary(std::ios::out | std::ios::binary);
    if (compactBinary) {
        writeVarint(binary, retainedPercent);
        writeVarint(binary, profiles.size());
    } else if (retainedPercent == 100) out << "CPG_NGRAM 1\n";
    else out << "CPG_NGRAM 2\npruning " << retainedPercent << '\n';
    auto names = languageCodes();
    for (const auto& name : names) {
        const auto& p = profiles.at(name);
        if (compactBinary) {
            writeVarint(binary, name.size());
            binary.write(name.data(), name.size());
            writeVarint(binary, p.total);
        } else out << "language " << name << ' ' << p.total << '\n';
        for (size_t n = 0; n < 3; ++n) {
            std::vector<std::pair<uint64_t, uint64_t>> sorted(p.counts[n].begin(), p.counts[n].end());
            std::sort(sorted.begin(), sorted.end());
            if (compactBinary) {
                writeVarint(binary, sorted.size());
                uint64_t previous = 0;
                for (auto [key, value] : sorted) {
                    writeVarint(binary, key - previous);
                    writeVarint(binary, value);
                    previous = key;
                }
            } else {
                out << "order " << n + 1 << ' ' << sorted.size() << '\n';
                for (auto entry : sorted) out << entry.first << ' ' << entry.second << '\n';
            }
        }
    }
    if (compactBinary) {
        auto plain = binary.str();
        uLongf length = compressBound(plain.size());
        std::string compressed(length, 0);
        if (compress2(reinterpret_cast<Bytef*>(compressed.data()), &length,
                      reinterpret_cast<const Bytef*>(plain.data()), plain.size(), 9) != Z_OK)
            throw std::runtime_error("Model compression failed");
        out << "CPG_NGRAM 3\n";
        writeVarint(out, plain.size());
        writeVarint(out, length);
        out.write(compressed.data(), length);
    } else out << "end\n";
    out.close();
    if (!out) throw std::runtime_error("Failed writing model: " + path.string());
}

void NgramModel::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read model: " + path.string());
    auto fail = [&]() { throw std::runtime_error("Invalid model: " + path.string()); };
    std::string token;
    int version;
    if (!(file >> token >> version) || token != "CPG_NGRAM" || version < 1 || version > 3) fail();
    const bool compactBinary = version == 3;
    std::istringstream decoded(std::ios::in | std::ios::binary);
    if (compactBinary) {
        if (file.get() != '\n') fail();
        auto plainSize = readVarint(file), compressedSize = readVarint(file);
        constexpr uint64_t maximumSize = 256 * 1024 * 1024;
        if (!plainSize || !compressedSize || plainSize > maximumSize || compressedSize > maximumSize) fail();
        std::string compressed(compressedSize, 0), plain(plainSize, 0);
        if (!file.read(compressed.data(), compressed.size()) || file.peek() != std::char_traits<char>::eof()) fail();
        uLongf actualSize = plainSize;
        if (uncompress(reinterpret_cast<Bytef*>(plain.data()), &actualSize,
                       reinterpret_cast<const Bytef*>(compressed.data()), compressed.size()) != Z_OK ||
            actualSize != plainSize) fail();
        decoded.str(std::move(plain));
    }
    std::istream& in = compactBinary ? static_cast<std::istream&>(decoded) : static_cast<std::istream&>(file);
    unsigned loadedPercent = 100;
    if (compactBinary) {
        auto percent = readVarint(in);
        if (percent < 1 || percent > 100) fail();
        loadedPercent = static_cast<unsigned>(percent);
    } else if (version == 2 && (!(in >> token >> loadedPercent) || token != "pruning" ||
                         loadedPercent < 1 || loadedPercent >= 100)) fail();
    std::unordered_map<std::string, Profile> loaded;
    uint64_t remaining = compactBinary ? readVarint(in) : 0;
    if (remaining > 1024) fail();
    while (true) {
        if (compactBinary) {
            if (!remaining) break;
            --remaining;
        } else {
            if (!(in >> token)) fail();
            if (token == "end") break;
            if (token != "language") fail();
        }
        std::string iso;
        Profile p;
        if (compactBinary) {
            auto size = readVarint(in);
            if (!size || size > 128) fail();
            iso.resize(size);
            if (!in.read(iso.data(), size)) fail();
            p.total = readVarint(in);
        } else if (!(in >> iso >> p.total)) fail();
        if (p.total == 0 || loaded.count(iso)) fail();
        for (size_t n = 0; n < 3; ++n) {
            uint64_t entries;
            if (compactBinary) entries = readVarint(in);
            else {
                size_t order;
                if (!(in >> token >> order >> entries) || token != "order" || order != n + 1) fail();
            }
            if (entries > 10000000) fail();
            uint64_t sum = 0;
            uint64_t previous = 0;
            for (size_t j = 0; j < entries; ++j) {
                uint64_t key, value;
                if (compactBinary) {
                    auto delta = readVarint(in);
                    if (delta > std::numeric_limits<uint64_t>::max() - previous) fail();
                    key = previous + delta;
                    value = readVarint(in);
                    previous = key;
                } else if (!(in >> key >> value)) fail();
                if (value == 0 || value > p.total ||
                    (key >> ((n + 1) * shift)) != 0 ||
                    sum > std::numeric_limits<uint64_t>::max() - value ||
                    !p.counts[n].emplace(key, value).second) fail();
                sum += value;
            }
            if ((n == 0 && sum != p.total) || sum > p.total) fail();
        }
        // Retained higher-order counts require their original prefixes.
        // In particular, pruning cannot silently remove a denominator.
        for (size_t n = 1; n < 3; ++n)
            for (const auto& [key, value] : p.counts[n])
                if (count(p.counts[n - 1], key >> shift) < value) fail();
        loaded.emplace(iso, std::move(p));
    }
    if (compactBinary) {
        if (in.peek() != std::char_traits<char>::eof()) fail();
    } else if (in >> token) fail();
    profiles = std::move(loaded);
    retainedPercent = loadedPercent;
}

void NgramModel::loadDirectory(const std::filesystem::path& path) {
    NgramModel result;
    bool first = true;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(path))
        if (entry.is_regular_file() && entry.path().extension() == ".ngram") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    if (files.empty()) throw std::runtime_error("No .ngram models in " + path.string());
    for (const auto& file : files) {
        NgramModel part;
        part.load(file);
        if (first) result.retainedPercent = part.retainedPercent;
        else if (result.retainedPercent != part.retainedPercent)
            throw std::runtime_error("Mixed pruning settings in model directory");
        first = false;
        for (auto& [iso, profile] : part.profiles)
            if (!result.profiles.emplace(iso, std::move(profile)).second)
                throw std::runtime_error("Duplicate language model: " + iso);
    }
    *this = std::move(result);
}
