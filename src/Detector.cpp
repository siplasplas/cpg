#include <cpg/Detector.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>
#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <unicode/ucsdet.h>

namespace {

std::optional<DetectionResult> detectUnicode(std::string_view bytes) {
    if (bytes.empty() || bytes.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
        return std::nullopt;

    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UCharsetDetector, decltype(&ucsdet_close)> detector(
        ucsdet_open(&status), &ucsdet_close);
    if (U_FAILURE(status) || !detector) return std::nullopt;
    ucsdet_setText(detector.get(), bytes.data(), static_cast<int32_t>(bytes.size()), &status);
    const UCharsetMatch* match = ucsdet_detect(detector.get(), &status);
    if (U_FAILURE(status) || !match) return std::nullopt;

    const char* name = ucsdet_getName(match, &status);
    const int confidence = ucsdet_getConfidence(match, &status);
    // Weak guesses (including plain ASCII) should reach the codepage fallback.
    constexpr int minimumConfidence = 80;
    if (U_FAILURE(status) || !name || confidence < minimumConfidence)
        return std::nullopt;

    std::string canonical;
    const std::string icuName(name);
    if (icuName == "UTF-8") canonical = "utf8";
    else if (icuName == "UTF-16LE") canonical = "utf16";
    else if (icuName == "UTF-16BE") canonical = "utf16be";
    else if (icuName == "UTF-32LE") canonical = "utf32";
    else if (icuName == "UTF-32BE") canonical = "utf32be";
    else return std::nullopt;

    // Detection is heuristic; reject truncated or malformed Unicode even when
    // ICU gives it a high confidence. Preflight conversion validates all bytes.
    std::unique_ptr<UConverter, decltype(&ucnv_close)> converter(
        ucnv_open(name, &status), &ucnv_close);
    if (U_FAILURE(status) || !converter) return std::nullopt;
    ucnv_setToUCallBack(converter.get(), UCNV_TO_U_CALLBACK_STOP, nullptr,
                      nullptr, nullptr, &status);
    if (U_FAILURE(status)) return std::nullopt;
    ucnv_toUChars(converter.get(), nullptr, 0, bytes.data(),
                 static_cast<int32_t>(bytes.size()), &status);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
        return std::nullopt;

    return DetectionResult{canonical, confidence / 100.0, 0};
}

std::string toLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char) std::tolower(c); });
    return s;
}

// Map aliases used in languages.txt to canonical names in CpManager.
std::string normalizeName(const std::string& raw) {
    std::string n = toLowerAscii(raw);
    if (n.rfind("windows-", 0) == 0) return "cp" + n.substr(8);  // WINDOWS-1250 -> cp1250
    if (n.rfind("ibm", 0) == 0) {
        std::string rest = n.substr(3);
        if (!rest.empty() && std::isdigit((unsigned char) rest[0]))
            return "cp" + rest;   // IBM866 -> cp866
    }
    if (n == "maccyrillic") return "mac-cyrillic";
    if (n == "macroman") return "mac-roman";
    if (n == "macgreek") return "mac-greek";
    if (n == "macturkish") return "mac-turkish";
    if (n == "maciceland") return "mac-iceland";
    return n;
}

// Characters that can legitimately appear in any language: ASCII printable,
// whitespace, common Unicode punctuation, digits. Do NOT count as alphabet
// hit, but also not as noise.
bool isCommon(char32_t c) {
    if (c == 0) return false;
    if (c < 0x80) {
        // ASCII printable + whitespace
        return c == '\t' || c == '\n' || c == '\r' || (c >= 0x20 && c < 0x7F);
    }
    // NBSP, soft hyphen, general punctuation block
    if (c == 0x00A0 || c == 0x00AD) return true;
    if (c >= 0x2000 && c <= 0x206F) return true;  // general punctuation
    return false;
}

bool isControlOrPrivate(char32_t c) {
    if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') return true;
    if (c >= 0x7F && c < 0xA0) return true;
    if (c >= 0xE000 && c <= 0xF8FF) return true;   // PUA
    return false;
}

std::unordered_set<char32_t> alphabetSet(const Language& lang) {
    std::unordered_set<char32_t> s;
    for (char16_t c : lang.alphabet) s.insert((char32_t) c);
    return s;
}

void sortResults(std::vector<DetectionResult>& results) {
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        if (a.confidence != b.confidence) return a.confidence > b.confidence;
        if (a.score != b.score) return a.score > b.score;
        if (a.languageScore != b.languageScore) return a.languageScore > b.languageScore;
        if (a.rank != b.rank) return a.rank < b.rank;
        if (a.codepage != b.codepage) return a.codepage < b.codepage;
        return a.language < b.language;
    });
}

// Posterior probabilities with equal priors for every (decoding, language)
// pair: P ~ score^(tokens / divisor). Candidates sharing a decoding and
// language have the same score and count once. confidence = P(decoding),
// languageConfidence = P(language) over all decodings.
void assignConfidence(std::vector<DetectionResult>& results, size_t tokens, double divisor) {
    std::map<std::pair<int, std::string>, double> joint;
    double best = -std::numeric_limits<double>::infinity();
    for (const auto& result : results) {
        if (result.score <= 0) continue;
        double logit = tokens * std::log(result.score) / divisor;
        joint[{result.decoding, result.language}] = logit;
        best = std::max(best, logit);
    }
    std::map<int, double> decodings;
    std::map<std::string, double> languages;
    double total = 0;
    for (auto& [key, weight] : joint) {
        weight = std::exp(weight - best);
        decodings[key.first] += weight;
        languages[key.second] += weight;
        total += weight;
    }
    for (auto& result : results) {
        result.confidence = result.languageConfidence = 0;
        if (result.score <= 0 || total == 0) continue;
        result.confidence = decodings[result.decoding] / total;
        result.languageConfidence = languages[result.language] / total;
    }
}

constexpr size_t samplePart = 2048;

template<class Char>
std::basic_string<Char> modelSample(std::basic_string_view<Char> input) {
    // Bound editor latency: sample beginning, middle and end for large files.
    constexpr size_t part = samplePart;
    if (input.size() <= part * 3) return std::basic_string<Char>(input);
    std::basic_string<Char> sample(input.substr(0, part));
    sample.push_back(static_cast<Char>(' '));
    sample.append(input.substr((input.size() - part) / 2, part));
    sample.push_back(static_cast<Char>(' '));
    sample.append(input.substr(input.size() - part));
    return sample;
}

// Sample of legacy (non-Unicode) bytes. ASCII decodes the same in every
// candidate code page, so only text around non-ASCII bytes tells them apart.
// In source code those may be a few comments that a beginning/middle/end
// sample would miss, and the surrounding code would act as (misleading)
// language evidence, e.g. Pascal looking more Danish than Polish. The sample
// is made of the non-ASCII bytes with at least 16 bytes of context on each
// side, extended to whole words, within the line (up to the usual size);
// input without any falls back to modelSample(). Decoded Unicode text is
// sampled the same way for its language (non-ASCII characters).
template<class Char>
std::basic_string<Char> nonAsciiSample(std::basic_string_view<Char> input) {
    const size_t budget = samplePart * 3, context = 16;
    const auto high = [&](size_t i) {
        return static_cast<std::make_unsigned_t<Char>>(input[i]) >= 0x80;
    };
    const auto space = [&](size_t i) { return input[i] == ' ' || input[i] == '\t' || input[i] == '\r'; };
    std::basic_string<Char> sample;
    size_t covered = 0; // end of the previous piece
    for (size_t i = 0; i < input.size() && sample.size() < budget; ++i) {
        if (!high(i)) continue;
        // Back 16 bytes and on to a word start, at most 64 bytes and not into
        // the previous piece or line; these bytes are ASCII.
        size_t from = i;
        while (from > covered && input[from - 1] != '\n' && i - from < 64 &&
               (i - from < context || !space(from - 1))) --from;
        // Forward likewise to a word end; past 64 bytes stop at whitespace,
        // which is never inside a multi-byte (e.g. UTF-8) character.
        size_t to = i;
        while (to < input.size() && input[to] != '\n' && (to - i < context || !space(to))) ++to;
        sample.append(input.substr(from, to - from));
        sample.push_back(' ');
        covered = to;
        i = to;
    }
    return sample.empty() ? modelSample<Char>(input) : sample;
}

} // namespace

Detector::Detector(CpManager& cpm, Languages& langs, const NgramModel* model)
    : cpManager(cpm), languages(langs), model(model) {}

std::string Detector::canonicalCodepageName(const std::string& name) {
    return normalizeName(name);
}

int Detector::codepageRank(const std::string& name) {
    std::string n = toLowerAscii(name);
    if (n.rfind("utf", 0) == 0) return 0;                    // UTF = always best if valid
    if (n.rfind("iso-8859", 0) == 0) return 1;               // ISO Latin family
    if (n.rfind("cp125", 0) == 0) return 1;                  // Windows ANSI
    if (n.rfind("cp1250", 0) == 0 || n.rfind("windows-", 0) == 0) return 1;
    if (n == "cp852" || n == "cp866" || n == "cp437" ||
        n == "cp850" || n == "cp855" || n == "cp862") return 2;   // common DOS
    if (n.rfind("cp", 0) == 0) return 3;                     // other DOS
    if (n.rfind("mac-", 0) == 0 || n.rfind("apple-", 0) == 0) return 3;
    if (n == "mazovia" || n == "fido-mazovia") return 4;     // well-known PL DOS
    // Everything else (DHN, Cyfromat, IINTE-ISIS, SMC, TeXPL, Amiga*, ...)
    return 5;
}

std::vector<DetectionResult>
Detector::detectCodepage(const std::string& iso, std::string_view bytes) {
    if (iso.empty() || iso == "auto") return detectCodepage(bytes);
    if (iso == "cs") return detectCodepage("cz", bytes);
    std::vector<DetectionResult> results;

    if (bytes.empty()) return results;
    // With a model, a known language is automatic detection among one language.
    if (model && model->contains(iso) && languages.getByIsoCode(iso)) return detectAuto({iso}, bytes);
    if (auto unicode = detectUnicode(bytes)) {
        if (languages.getByIsoCode(iso)) unicode->language = iso;
        return {*unicode};
    }

    const Language* lang = languages.getByIsoCode(iso);
    if (!lang) return results;

    auto alpha = alphabetSet(*lang);
    auto charsets = languages.getCharsetsForLanguage(iso);

    for (const auto& charsetName : charsets) {
        std::string canonical = normalizeName(charsetName);
        Codepage* cp = cpManager.getByName(canonical);
        if (!cp) continue;

        std::u32string u32 = cp->toU32(bytes);

        int64_t hits = 0, misses = 0, noise = 0;
        for (char32_t c : u32) {
            if (alpha.count(c)) hits++;
            else if (isCommon(c)) { /* neutral */ }
            else if (isControlOrPrivate(c)) noise++;
            else misses++;
        }

        double denom = (double)(hits + misses + noise);
        double score = denom > 0 ? (double) hits / denom : 0.0;
        if (denom > 0) {
            double noiseRatio = (double) noise / denom;
            score *= (1.0 - noiseRatio);
        }

        // Without a model no decodings are merged; plausible() lists them all.
        DetectionResult result{canonical, score, codepageRank(canonical), iso};
        result.decoding = static_cast<int>(results.size());
        results.push_back(result);
    }

    sortResults(results);
    return results;
}

std::vector<DetectionResult> Detector::detectCodepage(std::string_view bytes) {
    return detectAuto({}, bytes);
}

std::vector<DetectionResult>
Detector::detectCodepageAmong(const std::vector<std::string>& isos, std::string_view bytes) {
    std::vector<std::string> wanted;
    for (const auto& raw : isos) {
        const std::string iso = raw == "cs" ? "cz" : raw;
        if (iso.empty() || iso == "auto") return detectAuto({}, bytes);
        if (std::find(wanted.begin(), wanted.end(), iso) == wanted.end()) wanted.push_back(iso);
    }
    if (wanted.size() <= 1) return detectCodepage(wanted.empty() ? "auto" : wanted.front(), bytes);
    return detectAuto(wanted, bytes);
}

std::vector<DetectionResult>
Detector::detectAuto(const std::vector<std::string>& isos, std::string_view bytes) {
    if (bytes.empty()) return {};
    std::vector<std::string> codes = model ? model->languageCodes() : std::vector<std::string>();
    if (!isos.empty())
        codes.erase(std::remove_if(codes.begin(), codes.end(), [&](const auto& iso) {
            return std::find(isos.begin(), isos.end(), iso) == isos.end();
        }), codes.end());
    std::vector<DetectionResult> results;
    auto unicode = detectUnicode(bytes);
    // Valid UTF-8 with only a few multi-byte sequences (ICU confidence 80)
    // may as well be legacy text, e.g. Polish "Ół" in cp1250 is valid UTF-8;
    // then UTF-8 is one more candidate decoding for the language models.
    const bool weakUtf8 = model && unicode && unicode->codepage == "utf8" && unicode->score < 1;
    if (unicode && !weakUtf8) {
        unicode->confidence = unicode->score;
        if (!model) return {*unicode};
        auto text = cpManager.getByName(unicode->codepage)->toU32(bytes);
        auto sample = nonAsciiSample<char32_t>(text);
        for (const auto& iso : codes) {
            if (!languages.getByIsoCode(iso)) continue;
            auto result = *unicode;
            result.language = iso;
            result.languageScore = model->score(iso, sample);
            results.push_back(std::move(result));
        }
        if (results.empty()) return {*unicode};
        // Only the language is uncertain: one decoding, ICU's confidence.
        for (auto& result : results) result.score = result.languageScore;
        assignConfidence(results, NgramModel::tokenCount(sample), evidenceDivisor);
        for (auto& result : results) {
            result.score = unicode->score;
            result.confidence = result.languageScore > 0 ? unicode->score : 0;
        }
        sortResults(results);
        if (results.front().languageScore == 0) return {*unicode};
        return results;
    }
    if (!model) throw std::runtime_error("Automatic legacy detection requires language models");
    const auto sample = nonAsciiSample<char>(bytes);
    std::unordered_map<std::string, std::u32string> decoded;
    std::map<std::u32string, int> decodings;
    size_t tokens = 0;
    for (const auto& iso : codes) {
        if (!languages.getByIsoCode(iso)) continue;
        std::vector<std::string> names;
        for (const auto& raw : languages.getCharsetsForLanguage(iso)) names.push_back(normalizeName(raw));
        if (weakUtf8) names.push_back("utf8");
        std::unordered_set<std::string> seen;
        for (const auto& name : names) {
            if (!seen.insert(name).second) continue;
            auto* cp = cpManager.getByName(name);
            if (!cp) continue;
            auto it = decoded.find(name);
            if (it == decoded.end()) {
                it = decoded.emplace(name, cp->toU32(sample)).first;
                tokens = std::max(tokens, NgramModel::tokenCount(it->second));
            }
            // Undefined-byte candidates cannot provide lossless editor text.
            // (ICU has validated the whole input as UTF-8 already.)
            if (name != "utf8" && it->second.size() != sample.size()) continue;
            auto score = model->score(iso, it->second);
            DetectionResult result{name, score, codepageRank(name), iso, score};
            result.decoding = decodings.emplace(it->second, static_cast<int>(decodings.size())).first->second;
            results.push_back(result);
        }
    }
    // Every likely single-byte candidate must also preserve every byte of the
    // complete file, including positions outside the statistical sample.
    // Failures are removed and the probabilities recomputed without them.
    std::unordered_set<std::string> verified{"utf8"};
    for (bool changed = true; changed;) {
        changed = false;
        assignConfidence(results, tokens, evidenceDivisor);
        sortResults(results);
        double covered = 0;
        std::unordered_set<int> counted;
        for (const auto& result : results) {
            if (covered >= 1 - 1e-6) break;
            if (counted.insert(result.decoding).second) covered += result.confidence;
            if (!verified.insert(result.codepage).second) continue;
            if (cpManager.getByName(result.codepage)->toU32(bytes).size() == bytes.size()) continue;
            const auto name = result.codepage;
            results.erase(std::remove_if(results.begin(), results.end(),
                [&](const auto& r) { return r.codepage == name; }), results.end());
            changed = true;
            break;
        }
    }
    return results;
}

std::vector<DetectionResult>
Detector::plausible(const std::vector<DetectionResult>& results, double threshold) {
    std::vector<DetectionResult> chosen;
    std::unordered_set<int> seen;
    double covered = 0;
    for (const auto& result : results) {
        if (covered >= threshold) break;
        if (!seen.insert(result.decoding).second) continue;
        chosen.push_back(result);
        covered += result.confidence;
    }
    return chosen;
}
